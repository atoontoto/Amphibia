#include "../../NeuralAmpModeler/Credentials/ISecureCredentialStore.h"
#include "../../NeuralAmpModeler/Credentials/CredentialRefreshLock.h"
#include "../../NeuralAmpModeler/Network/HttpPolicy.h"
#include "../../NeuralAmpModeler/Network/RequestPolicy.h"
#include "../../NeuralAmpModeler/Network/PlatformHttpTransport.h"
#include "../../NeuralAmpModeler/Loading/FileInspection.h"
#include "../../NeuralAmpModeler/Platform/SystemBrowser.h"
#include "../../NeuralAmpModeler/Providers/Tone3000/OAuthController.h"
#include "../../NeuralAmpModeler/Providers/Tone3000/Pkce.h"
#include "../../NeuralAmpModeler/Providers/Tone3000/Tone3000ApiClient.h"
#include "../../NeuralAmpModeler/Providers/Tone3000/Tone3000DownloadCoordinator.h"
#include "../../NeuralAmpModeler/Providers/Tone3000/Tone3000Provider.h"
#include "../../NeuralAmpModeler/Providers/Tone3000/Tone3000Parsing.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

using namespace amphibia;

namespace {

int gPassed = 0;

void Require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

template <typename Function> void Test(const char* name, Function function)
{
  try { function(); ++gPassed; std::cout << "PASS " << name << '\n'; }
  catch (const std::exception& error) { std::cerr << "FAIL " << name << ": " << error.what() << '\n'; std::exit(1); }
}

providers::tone3000::Tone3000Configuration TestConfiguration()
{
  providers::tone3000::Tone3000Configuration result;
  result.clientId = "test-publishable-client";
  result.authorizationEndpoint = "https://provider.test/api/v1/oauth/authorize";
  result.tokenEndpoint = "https://provider.test/api/v1/oauth/token";
  result.apiBaseUrl = "https://provider.test";
  result.redirectUri = "http://127.0.0.1:43821/oauth/callback";
  result.kind = providers::tone3000::Tone3000ConfigurationKind::Test;
  result.allowTestHttp = true;
  result.credentialServiceNamespace = "org.amphibiaaudio.Amphibia.TONE3000.test";
  result.credentialAccount = "milestone4-provider";
  return result;
}

credentials::SecureCredential Credential(const char* access, const char* refresh, std::uint64_t generation)
{
  credentials::SecureCredential value;
  value.tokens.accessToken = access;
  value.tokens.refreshToken = refresh;
  value.tokens.tokenType = "bearer";
  value.tokens.accessTokenExpiry = std::chrono::system_clock::now() + std::chrono::hours(1);
  value.tokens.scope = "read";
  value.tokens.rotationGeneration = generation;
  value.providerUserId = "test-user";
  value.username = "amphibia-test";
  value.verifiedAt = 1234;
  return value;
}

class FakeTransport final : public network::IHttpTransport
{
public:
  network::HttpResponse next;
  network::HttpRequest last;
  std::vector<std::uint8_t> downloadBody;
  network::StreamingDownloadRequest lastDownload;
  network::RequestHandle Begin(network::HttpRequest request, network::HttpCompletion completion) override
  {
    last = std::move(request);
    const network::RequestHandle handle{++id};
    completion(handle, network::HttpResult<network::HttpResponse>{next, std::nullopt});
    return handle;
  }
  network::RequestHandle BeginDownload(network::StreamingDownloadRequest request, network::DownloadProgress progress,
                                       network::DownloadCompletion completion) override
  {
    lastDownload = request;
    const network::RequestHandle handle{++id};
    std::ofstream output(request.destination, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(downloadBody.data()), static_cast<std::streamsize>(downloadBody.size()));
    output.close();
    if (progress) progress(handle, downloadBody.size(), downloadBody.size());
    network::StreamingDownloadResponse response{200, {{"Content-Length", std::to_string(downloadBody.size())}}, request.request.url, downloadBody.size()};
    completion(handle, network::HttpResult<network::StreamingDownloadResponse>{response, std::nullopt});
    return handle;
  }
  void Cancel(network::RequestHandle) override {}
  void Shutdown() override {}
  std::uint64_t id{};
};

class FakeBrowser final : public platform::ISystemBrowser
{
public:
  providers::ProviderError OpenExplicit(const std::string& url) override { opened = url; ++calls; return {}; }
  std::string opened;
  int calls{};
};

network::HttpResponse JsonResponse(const std::string& text, int status = 200)
{
  network::HttpResponse response;
  response.statusCode = status;
  response.body.assign(text.begin(), text.end());
  response.finalUrl = "https://provider.test/api";
  return response;
}

std::vector<std::uint8_t> WaveBytes()
{
  std::vector<std::uint8_t> bytes;
  const auto u16 = [&](std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value)); bytes.push_back(static_cast<std::uint8_t>(value >> 8));
  };
  const auto u32 = [&](std::uint32_t value) {
    for (int index = 0; index < 4; ++index) bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
  };
  const std::array<std::int16_t, 8> samples{0, 100, -100, 200, -200, 50, -50, 0};
  bytes.insert(bytes.end(), {'R','I','F','F'}); u32(36 + static_cast<std::uint32_t>(samples.size() * 2));
  bytes.insert(bytes.end(), {'W','A','V','E','f','m','t',' '});
  u32(16); u16(1); u16(1); u32(48000); u32(96000); u16(2); u16(16);
  bytes.insert(bytes.end(), {'d','a','t','a'}); u32(static_cast<std::uint32_t>(samples.size() * 2));
  for (const auto sample : samples) u16(static_cast<std::uint16_t>(sample));
  return bytes;
}

#if defined(_WIN32)
class LoopbackServer
{
public:
  LoopbackServer()
  {
    WSADATA data{}; Require(WSAStartup(MAKEWORD(2, 2), &data) == 0, "WSAStartup");
    mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); Require(mSocket != INVALID_SOCKET, "mock socket");
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = 0;
    Require(bind(mSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "mock bind");
    int size = sizeof(address); Require(getsockname(mSocket, reinterpret_cast<sockaddr*>(&address), &size) == 0, "mock port");
    mPort = ntohs(address.sin_port); Require(listen(mSocket, 8) == 0, "mock listen");
    mThread = std::thread([this] { Run(); });
  }
  ~LoopbackServer()
  {
    mStop.store(true); closesocket(mSocket);
    if (mThread.joinable()) mThread.join();
    WSACleanup();
  }
  std::string Url(const char* path) const { return "http://127.0.0.1:" + std::to_string(mPort) + path; }

private:
  static void Send(SOCKET socket, const std::string& text)
  {
    std::size_t sent = 0;
    while (sent < text.size())
    {
      const auto amount = send(socket, text.data() + sent, static_cast<int>(text.size() - sent), 0);
      if (amount <= 0) return;
      sent += static_cast<std::size_t>(amount);
    }
  }
  void Run()
  {
    while (!mStop.load())
    {
      fd_set readable; FD_ZERO(&readable); FD_SET(mSocket, &readable); timeval timeout{0, 100000};
      if (select(0, &readable, nullptr, nullptr, &timeout) <= 0) continue;
      const auto client = accept(mSocket, nullptr, nullptr); if (client == INVALID_SOCKET) continue;
      std::string request; std::array<char, 2048> buffer{};
      while (request.find("\r\n\r\n") == std::string::npos && request.size() < 65536)
      { const auto amount = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0); if (amount <= 0) break; request.append(buffer.data(), amount); }
      const auto firstSpace = request.find(' '), secondSpace = request.find(' ', firstSpace + 1);
      const auto path = firstSpace == std::string::npos || secondSpace == std::string::npos ? std::string{} : request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
      if (path == "/redirect") Send(client, "HTTP/1.1 302 Found\r\nLocation: /get\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
      else if (path == "/large") Send(client, "HTTP/1.1 200 OK\r\nContent-Length: 4096\r\nConnection: close\r\n\r\n" + std::string(4096, 'x'));
      else if (path == "/truncated") Send(client, "HTTP/1.1 200 OK\r\nContent-Length: 10\r\nConnection: close\r\n\r\nabc");
      else if (path == "/slow")
      { std::this_thread::sleep_for(std::chrono::milliseconds(500)); Send(client, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok"); }
      else if (path == "/download") Send(client, "HTTP/1.1 200 OK\r\nContent-Length: 12\r\nConnection: close\r\n\r\nmodel-bytes!");
      else Send(client, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 11\r\nConnection: close\r\n\r\n{\"ok\":true}");
      shutdown(client, SD_BOTH); closesocket(client);
    }
  }
  SOCKET mSocket{INVALID_SOCKET};
  std::uint16_t mPort{};
  std::atomic<bool> mStop{};
  std::thread mThread;
};
#endif

} // namespace

int main()
{
  Test("PKCE RFC vector", [] {
    const auto pair = providers::tone3000::CreatePkceFromVerifier(
      "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");
    Require(pair.challenge == "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM", "RFC challenge mismatch");
  });
  Test("PKCE random contract", [] {
    const auto first = providers::tone3000::GeneratePkce();
    const auto second = providers::tone3000::GeneratePkce();
    Require(first.verifier.size() >= 43 && first.verifier.size() <= 128, "verifier length");
    Require(first.challenge.size() == 43, "challenge length");
    Require(first.verifier != second.verifier, "verifier reused");
    Require(std::all_of(first.verifier.begin(), first.verifier.end(), [](unsigned char c) {
      return std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
    }), "verifier character set");
    Require(providers::tone3000::GenerateOAuthState() != providers::tone3000::GenerateOAuthState(), "state reused");
  });
  Test("callback validation", [] {
    const auto good = providers::tone3000::ParseOAuthCallback(
      "http://127.0.0.1:43821/oauth/callback?code=short-code&state=expected&tone_id=42",
      "http://127.0.0.1:43821/oauth/callback");
    Require(static_cast<bool>(good) && good.value->code == "short-code" && good.value->toneId == "42", "valid callback");
    Require(!providers::tone3000::ParseOAuthCallback(
      "http://127.0.0.1:43821/oauth/callback?code=a&state=x&state=y",
      "http://127.0.0.1:43821/oauth/callback"), "duplicate state accepted");
    Require(!providers::tone3000::ParseOAuthCallback(
      "http://127.0.0.1:43821/wrong?code=a&state=x", "http://127.0.0.1:43821/oauth/callback"),
      "wrong route accepted");
    Require(!providers::tone3000::ParseOAuthCallback(
      "http://127.0.0.1:43821/oauth/callback?code=%GG&state=x",
      "http://127.0.0.1:43821/oauth/callback"), "malformed encoding accepted");
    Require(!providers::tone3000::ParseOAuthCallback(
      "http://127.0.0.1:43821/oauth/callback?code=a",
      "http://127.0.0.1:43821/oauth/callback"), "missing state accepted");
    Require(!providers::tone3000::ParseOAuthCallback(
      "http://127.0.0.1:43821/oauth/callback?state=x",
      "http://127.0.0.1:43821/oauth/callback"), "missing result accepted");
    Require(!providers::tone3000::ParseOAuthCallback(
      "http://127.0.0.1:43821/oauth/callback?code=" + std::string(4096, 'a') + "&state=x",
      "http://127.0.0.1:43821/oauth/callback"), "oversized callback accepted");
  });
  Test("OAuth state is single use", [] {
    providers::tone3000::OAuthController controller(TestConfiguration());
    providers::AuthorizationRequest request;
    request.purpose = providers::AuthorizationPurpose::SelectTone;
    request.format = providers::RemoteContentFormat::Nam;
    const auto start = controller.Begin(request);
    Require(static_cast<bool>(start), "begin failed");
    const auto stateAt = start.value->authorizationUrl.find("state=");
    Require(stateAt != std::string::npos, "state absent");
    const auto stateEnd = start.value->authorizationUrl.find('&', stateAt);
    const auto state = start.value->authorizationUrl.substr(stateAt + 6, stateEnd - (stateAt + 6));
    const std::string callback = "http://127.0.0.1:43821/oauth/callback?code=one-time&state=" + state + "&tone_id=9";
    auto consumed = controller.HandleCallback(callback);
    Require(static_cast<bool>(consumed) && consumed.value->selectedToneId == "9", "callback failed");
    Require(!controller.HandleCallback(callback), "callback replay accepted");
  });
  Test("OAuth terminal paths invalidate pending state", [] {
    const auto stateFrom = [](const providers::ProviderResult<providers::tone3000::AuthorizationStart>& start) {
      Require(static_cast<bool>(start), "authorization start failed");
      const auto at = start.value->authorizationUrl.find("state=");
      Require(at != std::string::npos, "state absent");
      const auto end = start.value->authorizationUrl.find('&', at);
      return start.value->authorizationUrl.substr(at + 6, end - (at + 6));
    };
    providers::AuthorizationRequest request;
    request.purpose = providers::AuthorizationPurpose::SelectTone;

    providers::tone3000::OAuthController superseded(TestConfiguration());
    const auto firstState = stateFrom(superseded.Begin(request));
    const auto secondState = stateFrom(superseded.Begin(request));
    Require(!superseded.HandleCallback("http://127.0.0.1:43821/oauth/callback?code=old&state=" + firstState),
            "superseded state accepted");
    Require(!superseded.HandleCallback("http://127.0.0.1:43821/oauth/callback?code=new&state=" + secondState),
            "failed callback did not consume pending state");

    providers::tone3000::OAuthController rejected(TestConfiguration());
    const auto rejectedState = stateFrom(rejected.Begin(request));
    const auto rejectedResult = rejected.HandleCallback(
      "http://127.0.0.1:43821/oauth/callback?error=access_denied&state=" + rejectedState);
    Require(!rejectedResult && rejectedResult.error.code == providers::ProviderErrorCode::AuthorizationRejected,
            "provider rejection not reported");

    providers::tone3000::OAuthController cancelled(TestConfiguration());
    const auto cancelledState = stateFrom(cancelled.Begin(request));
    const auto cancelledResult = cancelled.HandleCallback(
      "http://127.0.0.1:43821/oauth/callback?canceled=true&state=" + cancelledState);
    Require(!cancelledResult && cancelledResult.error.code == providers::ProviderErrorCode::Cancelled,
            "provider cancellation not reported");

    providers::tone3000::OAuthController timedOut(TestConfiguration(), std::chrono::seconds(0));
    const auto timedOutState = stateFrom(timedOut.Begin(request));
    const auto timedOutResult = timedOut.HandleCallback(
      "http://127.0.0.1:43821/oauth/callback?code=late&state=" + timedOutState);
    Require(!timedOutResult && timedOutResult.error.code == providers::ProviderErrorCode::Cancelled,
            "expired authorization accepted");

    providers::tone3000::OAuthController shutDown(TestConfiguration());
    const auto shutDownState = stateFrom(shutDown.Begin(request));
    shutDown.Shutdown();
    Require(!shutDown.HandleCallback(
      "http://127.0.0.1:43821/oauth/callback?code=late&state=" + shutDownState),
      "post-shutdown callback accepted");
  });
  Test("known-tone prompt stays blocked", [] {
    providers::tone3000::OAuthController controller(TestConfiguration());
    providers::AuthorizationRequest request;
    request.purpose = providers::AuthorizationPurpose::ResolveKnownTone;
    request.toneId = "42";
    Require(!controller.Begin(request), "ambiguous known-tone prompt enabled");
  });
  Test("HTTPS and redirect policy", [] {
    Require(static_cast<bool>(network::ParseAndValidateUrl("https://www.tone3000.com/api/v1/user")), "HTTPS rejected");
    Require(!network::ParseAndValidateUrl("http://www.tone3000.com/api"), "plain HTTP accepted");
    Require(!network::ParseAndValidateUrl("https://user@tone3000.com/api"), "userinfo accepted");
    Require(!network::ParseAndValidateUrl("https://127.0.0.1/model.nam"), "loopback accepted");
    Require(!network::ParseAndValidateUrl("https://169.254.169.254/latest/meta-data"), "metadata IP accepted");
    Require(!network::ParseAndValidateUrl("https://10.0.0.1/model.nam"), "private IP accepted");
    Require(!network::ParseAndValidateUrl("https://provider.test:0/model.nam"), "invalid port accepted");
    Require(!network::ParseAndValidateUrl("https://provider.test/model.nam#secret"), "fragment accepted");
    Require(!network::ParseAndValidateUrl("https://[::1]/model.nam"), "IPv6 loopback accepted");
    Require(static_cast<bool>(network::ParseAndValidateUrl("http://127.0.0.1:5123/test", true)), "test loopback rejected");
    Require(!network::ParseAndValidateUrl("http://10.0.0.1/test", true), "test policy allowed LAN");
    std::vector<network::HttpHeader> headers{{"Authorization", "Bearer canary"}, {"Accept", "application/json"}};
    const auto stripped = network::HeadersForRedirect(headers, false);
    Require(stripped.size() == 1 && stripped[0].name == "Accept", "cross-origin secret retained");
    Require(network::HeadersForRedirect(headers, true).size() == 2, "same-origin header lost");
  });
  Test("retry and redaction policy", [] {
    const auto now = std::chrono::system_clock::from_time_t(0);
    Require(network::ParseRetryAfter("120", now) == std::chrono::seconds(120), "seconds Retry-After");
    Require(!network::ParseRetryAfter("999999", now), "unbounded Retry-After");
    Require(network::IsRetryableStatus(429) && network::IsRetryableStatus(503) && !network::IsRetryableStatus(403), "retry status");
    Require(network::RedactUrl("https://cdn.test/a.nam?signature=secret") == "https://cdn.test/a.nam?[redacted]", "URL redaction");
    const auto redacted = network::RedactDiagnostic("Authorization: Bearer canary\nurl=https://x.test/a?sig=secret");
    Require(redacted.find("canary") == std::string::npos && redacted.find("secret") == std::string::npos, "diagnostic leaked secret");
  });
  Test("bounded retry and rate policy", [] {
    network::HttpRequest request; request.idempotent = true;
    network::HttpResponse response; response.statusCode = 503;
    const auto retry = network::RetryDelay(request, 1, response, std::nullopt,
                                            std::chrono::system_clock::now(), 0);
    Require(retry && *retry >= std::chrono::milliseconds(250), "retry missing");
    request.idempotent = false;
    Require(!network::RetryDelay(request, 1, response, std::nullopt,
                                 std::chrono::system_clock::now(), 0), "POST retried");
    network::SlidingWindowRateLimiter limiter(2, 1);
    const auto now = std::chrono::steady_clock::now();
    const auto first = limiter.Reserve(now), second = limiter.Reserve(now), queued = limiter.Reserve(now);
    Require(first && second && queued && *queued >= now + std::chrono::minutes(1), "rate queue incorrect");
    Require(!limiter.Reserve(now), "rate queue was unbounded");
  });
  Test("token and API parsing", [] {
    const auto tokens = providers::tone3000::ParseTokenResponse(
      R"({"access_token":"access-canary","refresh_token":"refresh-canary","expires_in":3600,"token_type":"bearer","scope":"read"})",
      std::chrono::system_clock::now());
    Require(static_cast<bool>(tokens) && tokens.value->refreshToken == "refresh-canary", "token parse");
    const auto user = providers::tone3000::ParseCurrentUser(
      R"({"id":"u-1","username":"unicode-\u03bc","avatar_url":null,"url":"https://provider.test/users/u-1","future":true})");
    Require(static_cast<bool>(user) && user.value->providerUserId == "u-1", "user parse");
    const auto tone = providers::tone3000::ParseTone(
      R"({"id":42,"title":"Clean","description":null,"gear":"future-gear","format":"nam","license":"future-license","url":"https://provider.test/tones/42","user":{"id":"u-1","username":"maker","avatar_url":null,"url":"https://provider.test/users/u-1"},"tags":[{"name":"clean"}],"images":null})");
    Require(static_cast<bool>(tone) && tone.value->providerToneId == "42" &&
            tone.value->gearRole == providers::RemoteGearRole::Unknown &&
            tone.value->licenseId == "future-license", "tone parse");
    const auto models = providers::tone3000::ParseModelList(
      R"({"data":[{"id":7,"tone_id":"42","name":"A2","model_url":"https://provider.test/models/7.nam?signature=secret","architecture_version":"2"}],"page":1,"page_size":1,"total":1,"total_pages":1})",
      providers::RemoteContentFormat::Nam);
    Require(static_cast<bool>(models) && models.value->size() == 1 &&
            models.value->front().architecture == providers::RemoteNamArchitecture::A2, "models parse");
    Require(!providers::tone3000::ParseCurrentUser(R"({"username":"missing-id"})"), "missing user id accepted");
  });
  Test("adversarial API parsing bounds", [] {
    const auto now = std::chrono::system_clock::now();
    Require(!providers::tone3000::ParseTokenResponse("not-json", now), "invalid token JSON accepted");
    Require(!providers::tone3000::ParseTokenResponse(
      R"({"access_token":"x","token_type":"Bearer","expires_in":3600})", now), "invalid token type accepted");
    Require(!providers::tone3000::ParseTokenResponse(
      R"({"access_token":"x","token_type":"bearer","expires_in":0})", now), "invalid expiry accepted");
    providers::tone3000::JsonLimits shallow; shallow.maximumDepth = 2;
    Require(!providers::tone3000::ParseCurrentUser(R"({"id":"u","username":"x","future":{"a":{"b":1}}})", shallow),
            "deep JSON accepted");
    providers::tone3000::JsonLimits tiny; tiny.maximumStringBytes = 4;
    Require(!providers::tone3000::ParseCurrentUser(R"({"id":"user","username":"long-name"})", tiny),
            "oversized string accepted");
    providers::tone3000::JsonLimits tags; tags.maximumTags = 1;
    Require(!providers::tone3000::ParseTone(
      R"({"id":"1","title":"x","user":{"id":"u","username":"n"},"tags":["a","b"]})", tags),
      "excessive tags accepted");
    providers::tone3000::JsonLimits models; models.maximumModels = 1;
    Require(!providers::tone3000::ParseModelList(
      R"({"data":[{"id":"1","tone_id":"t","name":"a","model_url":"https://provider.test/a.nam"},{"id":"2","tone_id":"t","name":"b","model_url":"https://provider.test/b.nam"}]})",
      providers::RemoteContentFormat::Nam, models), "excessive model count accepted");
    Require(!providers::tone3000::ParseModel(
      R"({"id":"1","tone_id":"t","name":"a","model_url":"http://provider.test/a.nam"})",
      providers::RemoteContentFormat::Nam), "insecure model URL accepted");
  });
  Test("typed API request boundary", [] {
    FakeTransport transport;
    transport.next = JsonResponse(R"({"id":"u-1","username":"tester","avatar_url":null,"url":"https://provider.test/u/u-1"})");
    providers::tone3000::Tone3000ApiClient client(TestConfiguration(), transport);
    bool called = false;
    client.FetchCurrentUser("access-canary", [&](auto result) { called = true; Require(static_cast<bool>(result), "API callback failed"); });
    Require(called && transport.last.url == "https://provider.test/api/v1/user", "wrong current-user endpoint");
    Require(std::any_of(transport.last.headers.begin(), transport.last.headers.end(), [](const auto& header) {
      return header.name == "Authorization" && header.value == "Bearer access-canary";
    }), "bearer header absent");
    transport.next = JsonResponse(R"({"access_token":"new-access","refresh_token":"new-refresh","expires_in":300,"token_type":"bearer"})");
    bool exchanged = false;
    client.ExchangeAuthorizationCode("code value", "verifier-value", [&](auto result) { exchanged = static_cast<bool>(result); });
    Require(exchanged && transport.last.url == "https://provider.test/api/v1/oauth/token", "token endpoint");
    const std::string body(transport.last.body.begin(), transport.last.body.end());
    Require(body.find("code=code%20value") != std::string::npos && body.find("client_secret") == std::string::npos,
            "token form or secret policy");
  });
  Test("typed API status mapping", [] {
    FakeTransport transport; providers::tone3000::Tone3000ApiClient client(TestConfiguration(), transport);
    const std::array<std::pair<int, providers::ProviderErrorCode>, 6> cases{{
      {401, providers::ProviderErrorCode::AuthenticationRequired}, {403, providers::ProviderErrorCode::Forbidden},
      {404, providers::ProviderErrorCode::NotFound}, {429, providers::ProviderErrorCode::RateLimited},
      {500, providers::ProviderErrorCode::ServerError}, {503, providers::ProviderErrorCode::ServerError}}};
    for (const auto& [status, expected] : cases)
    {
      transport.next = JsonResponse(R"({"error":"test"})", status);
      transport.next.headers = {{"Retry-After", "1"}};
      providers::ProviderErrorCode actual = providers::ProviderErrorCode::None;
      client.FetchCurrentUser("status-canary", [&](auto result) { if (!result) actual = result.error.code; });
      Require(actual == expected, "HTTP status mapping mismatch");
    }
    transport.next = JsonResponse("not-json");
    providers::ProviderErrorCode invalid = providers::ProviderErrorCode::None;
    client.FetchCurrentUser("status-canary", [&](auto result) { if (!result) invalid = result.error.code; });
    Require(invalid == providers::ProviderErrorCode::InvalidJson, "invalid API JSON mapping");
  });
  Test("download validation promotion and provenance", [] {
    const auto root = std::filesystem::temp_directory_path() /
      ("amphibia-m4-library-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup(){ std::error_code error; std::filesystem::remove_all(path, error); } } cleanup{root};
    library::ManagedLibrary library(root); library.Initialize();
    FakeTransport transport;
    const std::string nam = R"({"architecture":"Linear","config":{},"weights":[]})";
    transport.downloadBody.assign(nam.begin(), nam.end());
    providers::RemoteTone tone; tone.providerToneId = "42"; tone.title = "Clean";
    tone.creator.providerUserId = "creator-1"; tone.creator.username = "maker";
    tone.licenseId = "future-license"; tone.licenseName = "Future License";
    tone.sourcePageUrl = "https://www.tone3000.com/tones/42";
    providers::RemoteModel model; model.providerToneId = "42"; model.providerModelId = "7";
    model.name = "Linear A2"; model.format = providers::RemoteContentFormat::Nam;
    model.architecture = providers::RemoteNamArchitecture::A2;
    model.downloadUrl = "https://provider.test/models/7.nam?signature=redacted";
    providers::tone3000::Tone3000DownloadCoordinator coordinator(transport, library);
    providers::ProviderResult<providers::DownloadResult> completed;
    coordinator.Begin(tone, model, "access-canary", [&](auto result) { completed = std::move(result); });
    Require(static_cast<bool>(completed) && completed.value->managedObjectSha256.size() == 64, "download promotion failed");
    const auto snapshot = library.Snapshot();
    Require(snapshot.objects.size() == 1 && snapshot.providerAssociations.size() == 1 &&
            snapshot.providerAssociations.front().creatorUsername == "maker" &&
            snapshot.providerAssociations.front().licenseId == "future-license", "download provenance missing");
    Require(std::any_of(transport.lastDownload.request.headers.begin(), transport.lastDownload.request.headers.end(), [](const auto& header) {
      return header.name == "Authorization" && header.value == "Bearer access-canary";
    }), "download authorization absent");
    const auto metadata = root / "metadata" / "index-v2.json";
    std::ifstream input(metadata, std::ios::binary); const std::string stored((std::istreambuf_iterator<char>(input)), {});
    input.close();
    Require(stored.find("signature=redacted") == std::string::npos && stored.find("access-canary") == std::string::npos,
            "signed URL or token persisted");

    transport.downloadBody.assign({'n','o','t','-','n','a','m'}); model.providerModelId = "8";
    model.downloadUrl = "https://provider.test/models/8.nam";
    providers::ProviderResult<providers::DownloadResult> rejected;
    coordinator.Begin(tone, model, "access-canary", [&](auto result) { rejected = std::move(result); });
    Require(!rejected && rejected.error.code == providers::ProviderErrorCode::IntegrityFailed,
            "invalid NAM download was promoted");
    Require(std::filesystem::is_empty(root / "staging"), "failed download part was not cleaned");

    transport.downloadBody = WaveBytes();
    const auto waveProbe = root / "wave-probe.wav";
    { std::ofstream output(waveProbe, std::ios::binary); output.write(
        reinterpret_cast<const char*>(transport.downloadBody.data()),
        static_cast<std::streamsize>(transport.downloadBody.size())); }
    loading::InspectWaveFile(waveProbe); std::filesystem::remove(waveProbe);
    model.providerModelId = "9"; model.name = "Room IR";
    model.format = providers::RemoteContentFormat::ImpulseResponse; model.architecture = providers::RemoteNamArchitecture::Unknown;
    model.downloadUrl = "https://provider.test/models/9.wav";
    providers::ProviderResult<providers::DownloadResult> irCompleted;
    coordinator.Begin(tone, model, "access-canary", [&](auto result) { irCompleted = std::move(result); });
    Require(static_cast<bool>(irCompleted), "valid WAV download was rejected");
    const auto afterIr = library.Snapshot();
    Require(afterIr.objects.size() == 2 && afterIr.providerAssociations.size() == 2, "IR provenance missing");
  });
  Test("provider lifecycle is explicit and unconfigured-safe", [] {
    const auto root = std::filesystem::temp_directory_path() /
      ("amphibia-m4-provider-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup(){ std::error_code error; std::filesystem::remove_all(path, error); } } cleanup{root};
    library::ManagedLibrary library(root); library.Initialize(); FakeTransport transport; FakeBrowser browser;
    credentials::PlatformSecureCredentialStore credentials;
    providers::tone3000::Tone3000Provider unconfigured({}, browser, credentials, transport, library);
    Require(unconfigured.GetConnectionState() == providers::ProviderConnectionState::Unconfigured && browser.calls == 0,
            "unconfigured provider started work");
    bool rejected = false;
    unconfigured.BeginAuthorization({}, [&](auto result) { rejected = !result && result.error.code == providers::ProviderErrorCode::Unconfigured; });
    Require(rejected && browser.calls == 0, "unconfigured provider opened browser");
    unconfigured.Shutdown();

    providers::tone3000::Tone3000Provider configured(TestConfiguration(), browser, credentials, transport, library);
    const credentials::CredentialKey providerKey{"org.amphibiaaudio.Amphibia.TONE3000.test", "milestone4-provider"};
    credentials.Remove(providerKey);
    Require(static_cast<bool>(credentials.Store(providerKey, Credential("disconnect-access", "disconnect-refresh", 1))),
            "provider credential setup failed");
    bool cancelled = false; providers::AuthorizationRequest request; request.purpose = providers::AuthorizationPurpose::SelectTone;
    configured.BeginAuthorization(request, [&](auto result) { cancelled = !result && result.error.code == providers::ProviderErrorCode::Cancelled; });
    Require(browser.calls == 1 && browser.opened.starts_with("https://provider.test/"), "explicit browser launch missing");
    configured.CancelAuthorization(); Require(cancelled, "authorization cancellation missing");
    configured.Disconnect(); credentials::SecureCredential disconnected;
    Require(credentials.Load(providerKey, disconnected).error == credentials::CredentialError::NotFound,
            "disconnect retained credential");
    configured.Shutdown();
  });
  Test("cross-process refresh lock is bounded", [] {
    credentials::CredentialKey key{"org.amphibiaaudio.Amphibia.TONE3000.test", "refresh-lock"};
    credentials::CredentialRefreshLock first(key, std::chrono::milliseconds(100)); Require(first.IsLocked(), "first refresh lock");
    auto waiter = std::async(std::launch::async, [&] { credentials::CredentialRefreshLock second(key, std::chrono::milliseconds(50)); return second.IsLocked(); });
    Require(!waiter.get(), "contending refresh lock was not bounded");
  });
#if defined(_WIN32)
  Test("WinHTTP mock GET redirect and limits", [] {
    LoopbackServer server; network::PlatformHttpTransport transport;
    auto execute = [&](const char* path, std::size_t maximum, int redirects) {
      std::promise<network::HttpResult<network::HttpResponse>> promise; auto future = promise.get_future();
      network::HttpRequest request; request.url = server.Url(path); request.allowLoopbackHttpForTest = true;
      request.maximumResponseBytes = maximum; request.maximumRedirects = redirects; request.category = network::HttpEndpointCategory::TestOnly;
      transport.Begin(std::move(request), [&](auto, auto result) { promise.set_value(std::move(result)); });
      Require(future.wait_for(std::chrono::seconds(3)) == std::future_status::ready, "mock GET timeout"); return future.get();
    };
    const auto get = execute("/get", 1024, 0); Require(static_cast<bool>(get) && get.value->statusCode == 200, "mock GET");
    const auto redirect = execute("/redirect", 1024, 1); Require(static_cast<bool>(redirect) && redirect.value->finalUrl.ends_with("/get"), "mock redirect");
    const auto oversized = execute("/large", 1024, 0); Require(!oversized && oversized.failure->code == providers::ProviderErrorCode::ContentTooLarge, "response cap");
    const auto truncated = execute("/truncated", 1024, 0); Require(!truncated, "truncation accepted");

    std::promise<network::HttpResult<network::HttpResponse>> postPromise;
    auto postFuture = postPromise.get_future();
    network::HttpRequest post;
    post.method = network::HttpMethod::Post;
    post.url = server.Url("/post");
    post.allowLoopbackHttpForTest = true;
    post.category = network::HttpEndpointCategory::TestOnly;
    post.headers.push_back({"Content-Type", "application/x-www-form-urlencoded"});
    post.body.assign({'c','o','d','e','=','o','n','e'});
    post.maximumResponseBytes = 1024;
    transport.Begin(std::move(post), [&](auto, auto result) { postPromise.set_value(std::move(result)); });
    Require(postFuture.wait_for(std::chrono::seconds(3)) == std::future_status::ready &&
            static_cast<bool>(postFuture.get()), "mock POST");
  });
  Test("WinHTTP streamed part download and cancellation", [] {
    LoopbackServer server; network::PlatformHttpTransport transport;
    const auto path = std::filesystem::temp_directory_path() /
      ("amphibia-m4-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".part");
    struct Cleanup { std::filesystem::path path; ~Cleanup(){ std::error_code error; std::filesystem::remove(path, error); } } cleanup{path};
    network::StreamingDownloadRequest request; request.request.url = server.Url("/download");
    request.request.allowLoopbackHttpForTest = true; request.request.category = network::HttpEndpointCategory::TestOnly;
    request.destination = path; request.maximumBytes = 1024; request.minimumFreeSpaceReserve = 0;
    std::promise<network::HttpResult<network::StreamingDownloadResponse>> promise; auto future = promise.get_future();
    transport.BeginDownload(std::move(request), {}, [&](auto, auto result) { promise.set_value(std::move(result)); });
    Require(future.wait_for(std::chrono::seconds(3)) == std::future_status::ready, "download timeout");
    const auto result = future.get(); Require(static_cast<bool>(result) && result.value->bytes == 12 && std::filesystem::exists(path), "streamed download");
    std::promise<network::HttpResult<network::HttpResponse>> cancelledPromise; auto cancelledFuture = cancelledPromise.get_future();
    network::HttpRequest slow; slow.url = server.Url("/slow"); slow.allowLoopbackHttpForTest = true; slow.category = network::HttpEndpointCategory::TestOnly;
    const auto handle = transport.Begin(std::move(slow), [&](auto, auto value) { cancelledPromise.set_value(std::move(value)); });
    std::this_thread::sleep_for(std::chrono::milliseconds(30)); transport.Cancel(handle);
    Require(cancelledFuture.wait_for(std::chrono::seconds(3)) == std::future_status::ready && !cancelledFuture.get(), "cancel did not complete");
  });
#endif
  Test("browser URL validation", [] {
    Require(!platform::ValidateExternalAuthorizationUrl("https://tone3000.com/oauth?client_id=x&state=y"), "valid auth URL rejected");
    Require(static_cast<bool>(platform::ValidateExternalAuthorizationUrl("http://tone3000.com/oauth")), "browser allowed HTTP");
    Require(static_cast<bool>(platform::ValidateExternalAuthorizationUrl("https://tone3000.com/oauth?access_token=secret")), "browser allowed token");
  });
  Test("Windows credential lifecycle", [] {
    credentials::PlatformSecureCredentialStore store;
    credentials::CredentialKey key{"org.amphibiaaudio.Amphibia.TONE3000.test", "milestone4-foundation"};
    store.Remove(key);
    struct Cleanup { credentials::PlatformSecureCredentialStore& store; credentials::CredentialKey key; ~Cleanup(){ store.Remove(key); } } cleanup{store, key};
    auto original = Credential("access-canary-1", "refresh-canary-1", 1);
    Require(static_cast<bool>(store.Store(key, original)), "store failed");
    credentials::SecureCredential loaded;
    Require(static_cast<bool>(store.Load(key, loaded)), "load failed");
    Require(loaded.tokens.accessToken == "access-canary-1" && loaded.tokens.rotationGeneration == 1, "stored data mismatch");
    auto replacement = Credential("access-canary-2", "refresh-canary-2", 2);
    Require(static_cast<bool>(store.Store(key, replacement)), "replace failed");
    credentials::SecureCredential replaced;
    Require(store.Load(key, replaced) && replaced.tokens.rotationGeneration == 2, "replacement mismatch");
    Require(static_cast<bool>(store.Remove(key)), "remove failed");
    credentials::SecureCredential missing;
    const auto missingResult = store.Load(key, missing);
    Require(missingResult.error == credentials::CredentialError::NotFound, "missing credential not reported");
  });
  std::cout << "Milestone 4 foundation tests passed: " << gPassed << '\n';
  return 0;
}
