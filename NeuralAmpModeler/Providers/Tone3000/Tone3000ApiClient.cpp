#include "Tone3000ApiClient.h"

#include "../../Network/HttpPolicy.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <mutex>
#include <unordered_set>

namespace amphibia::providers::tone3000 {
namespace {

std::string Encode(std::string_view value)
{
  constexpr char digits[] = "0123456789ABCDEF";
  std::string output;
  for (const auto raw : value)
  {
    const auto c = static_cast<unsigned char>(raw);
    if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') output.push_back(static_cast<char>(c));
    else { output.push_back('%'); output.push_back(digits[c >> 4]); output.push_back(digits[c & 15]); }
  }
  return output;
}

std::string BodyText(const network::HttpResponse& response)
{
  return {reinterpret_cast<const char*>(response.body.data()), response.body.size()};
}

ProviderError MapFailure(const network::HttpFailure& failure)
{
  return {failure.code, failure.safeMessage, std::nullopt, std::nullopt,
          failure.platformCode ? std::optional<std::string>("platform error " + std::to_string(*failure.platformCode))
                               : std::nullopt};
}

ProviderError StatusError(const network::HttpResponse& response, bool token)
{
  ProviderErrorCode code = token ? ProviderErrorCode::TokenExchangeFailed : ProviderErrorCode::ServerError;
  switch (response.statusCode)
  {
    case 400: code = token ? ProviderErrorCode::InvalidGrant : ProviderErrorCode::InvalidResponse; break;
    case 401: code = ProviderErrorCode::AuthenticationRequired; break;
    case 403: code = ProviderErrorCode::Forbidden; break;
    case 404: code = ProviderErrorCode::NotFound; break;
    case 429: code = ProviderErrorCode::RateLimited; break;
    default: if (response.statusCode >= 500) code = ProviderErrorCode::ServerError; break;
  }
  ProviderError error{code, "TONE3000 request was not successful", response.statusCode};
  for (const auto& header : response.headers)
    if ((header.name == "Retry-After" || header.name == "retry-after") && response.statusCode == 429)
      if (const auto delay = network::ParseRetryAfter(header.value, std::chrono::system_clock::now()))
        error.retryAt = std::chrono::system_clock::now() + *delay;
  return error;
}

template <typename T> ProviderResult<T> Failed(ProviderError error)
{
  ProviderResult<T> result;
  result.error = std::move(error);
  return result;
}

network::HttpRequest ApiGet(const Tone3000Configuration& config, std::string path, std::string token)
{
  network::HttpRequest request;
  request.method = network::HttpMethod::Get;
  request.url = config.apiBaseUrl + std::move(path);
  request.headers = {{"Accept", "application/json"}, {"Authorization", "Bearer " + std::move(token)},
                     {"User-Agent", config.userAgent}};
  request.maximumResponseBytes = 4 * 1024 * 1024;
  request.maximumRedirects = 0;
  request.category = network::HttpEndpointCategory::ProviderApi;
  request.idempotent = true;
  request.allowLoopbackHttpForTest = config.kind == Tone3000ConfigurationKind::Test && config.allowTestHttp;
  return request;
}

} // namespace

struct Tone3000ApiClient::State
{
  Tone3000Configuration configuration;
  network::IHttpTransport* transport{};
  std::atomic<bool> alive{true};
};

Tone3000ApiClient::Tone3000ApiClient(Tone3000Configuration configuration, network::IHttpTransport& transport)
  : mState(std::make_shared<State>())
{
  mState->configuration = std::move(configuration);
  mState->transport = &transport;
}

Tone3000ApiClient::~Tone3000ApiClient() { Shutdown(); }

network::RequestHandle Tone3000ApiClient::ExchangeAuthorizationCode(std::string code, std::string verifier,
                                                                     TokenCallback callback)
{
  auto state = mState;
  network::HttpRequest request;
  request.method = network::HttpMethod::Post;
  request.url = state->configuration.tokenEndpoint;
  request.headers = {{"Accept", "application/json"}, {"Content-Type", "application/x-www-form-urlencoded"},
                     {"User-Agent", state->configuration.userAgent}};
  const auto body = "grant_type=authorization_code&client_id=" + Encode(state->configuration.clientId) +
                    "&redirect_uri=" + Encode(state->configuration.redirectUri) + "&code=" + Encode(code) +
                    "&code_verifier=" + Encode(verifier);
  request.body.assign(body.begin(), body.end());
  request.maximumResponseBytes = 64 * 1024;
  request.maximumRedirects = 0;
  request.category = network::HttpEndpointCategory::OAuthToken;
  request.allowLoopbackHttpForTest = state->configuration.kind == Tone3000ConfigurationKind::Test && state->configuration.allowTestHttp;
  std::weak_ptr<State> weak = state;
  return state->transport->Begin(std::move(request), [weak, callback = std::move(callback)](auto, auto result) mutable {
    const auto alive = weak.lock();
    if (!alive || !alive->alive.load()) return;
    if (!result) { callback(Failed<credentials::OAuthTokenSet>(MapFailure(*result.failure))); return; }
    if (result.value->statusCode < 200 || result.value->statusCode >= 300)
    { callback(Failed<credentials::OAuthTokenSet>(StatusError(*result.value, true))); return; }
    callback(ParseTokenResponse(BodyText(*result.value), std::chrono::system_clock::now()));
  });
}

network::RequestHandle Tone3000ApiClient::Refresh(std::string refreshToken, TokenCallback callback)
{
  auto state = mState;
  network::HttpRequest request;
  request.method = network::HttpMethod::Post;
  request.url = state->configuration.tokenEndpoint;
  request.headers = {{"Accept", "application/json"}, {"Content-Type", "application/x-www-form-urlencoded"},
                     {"User-Agent", state->configuration.userAgent}};
  const auto body = "grant_type=refresh_token&client_id=" + Encode(state->configuration.clientId) +
                    "&refresh_token=" + Encode(refreshToken);
  request.body.assign(body.begin(), body.end());
  request.maximumResponseBytes = 64 * 1024;
  request.maximumRedirects = 0;
  request.category = network::HttpEndpointCategory::OAuthToken;
  request.allowLoopbackHttpForTest = state->configuration.kind == Tone3000ConfigurationKind::Test && state->configuration.allowTestHttp;
  std::weak_ptr<State> weak = state;
  return state->transport->Begin(std::move(request), [weak, callback = std::move(callback)](auto, auto result) mutable {
    const auto alive = weak.lock(); if (!alive || !alive->alive.load()) return;
    if (!result) { callback(Failed<credentials::OAuthTokenSet>(MapFailure(*result.failure))); return; }
    if (result.value->statusCode < 200 || result.value->statusCode >= 300)
    { callback(Failed<credentials::OAuthTokenSet>(StatusError(*result.value, true))); return; }
    callback(ParseTokenResponse(BodyText(*result.value), std::chrono::system_clock::now()));
  });
}

network::RequestHandle Tone3000ApiClient::FetchCurrentUser(std::string accessToken, ProviderUserCallback callback)
{
  auto state = mState; std::weak_ptr<State> weak = state;
  return state->transport->Begin(ApiGet(state->configuration, "/api/v1/user", std::move(accessToken)),
    [weak, callback = std::move(callback)](auto, auto result) mutable {
      const auto alive = weak.lock(); if (!alive || !alive->alive.load()) return;
      if (!result) { callback(Failed<ProviderUser>(MapFailure(*result.failure))); return; }
      if (result.value->statusCode < 200 || result.value->statusCode >= 300)
      { callback(Failed<ProviderUser>(StatusError(*result.value, false))); return; }
      callback(ParseCurrentUser(BodyText(*result.value)));
    });
}

network::RequestHandle Tone3000ApiClient::FetchTone(std::string accessToken, std::string toneId, ToneCallback callback)
{
  auto state = mState; std::weak_ptr<State> weak = state;
  return state->transport->Begin(ApiGet(state->configuration, "/api/v1/tones/" + Encode(toneId), std::move(accessToken)),
    [weak, callback = std::move(callback)](auto, auto result) mutable {
      const auto alive = weak.lock(); if (!alive || !alive->alive.load()) return;
      if (!result) { callback(Failed<RemoteTone>(MapFailure(*result.failure))); return; }
      if (result.value->statusCode < 200 || result.value->statusCode >= 300)
      { callback(Failed<RemoteTone>(StatusError(*result.value, false))); return; }
      callback(ParseTone(BodyText(*result.value)));
    });
}

network::RequestHandle Tone3000ApiClient::FetchModels(std::string accessToken, std::string toneId,
                                                       RemoteContentFormat format, ModelListCallback callback,
                                                       std::size_t page, std::size_t pageSize)
{
  page = (std::max)(std::size_t{1}, page); pageSize = (std::clamp)(pageSize, std::size_t{1}, std::size_t{300});
  auto state = mState; std::weak_ptr<State> weak = state;
  const auto path = "/api/v1/models?tone_id=" + Encode(toneId) + "&page=" + std::to_string(page) +
                    "&page_size=" + std::to_string(pageSize);
  return state->transport->Begin(ApiGet(state->configuration, path, std::move(accessToken)),
    [weak, format, callback = std::move(callback)](auto, auto result) mutable {
      const auto alive = weak.lock(); if (!alive || !alive->alive.load()) return;
      if (!result) { callback(Failed<std::vector<RemoteModel>>(MapFailure(*result.failure))); return; }
      if (result.value->statusCode < 200 || result.value->statusCode >= 300)
      { callback(Failed<std::vector<RemoteModel>>(StatusError(*result.value, false))); return; }
      callback(ParseModelList(BodyText(*result.value), format));
    });
}

network::RequestHandle Tone3000ApiClient::FetchModel(std::string accessToken, std::string modelId,
                                                      RemoteContentFormat format, ModelCallback callback)
{
  auto state = mState; std::weak_ptr<State> weak = state;
  return state->transport->Begin(ApiGet(state->configuration, "/api/v1/models/" + Encode(modelId), std::move(accessToken)),
    [weak, format, callback = std::move(callback)](auto, auto result) mutable {
      const auto alive = weak.lock(); if (!alive || !alive->alive.load()) return;
      if (!result) { callback(Failed<RemoteModel>(MapFailure(*result.failure))); return; }
      if (result.value->statusCode < 200 || result.value->statusCode >= 300)
      { callback(Failed<RemoteModel>(StatusError(*result.value, false))); return; }
      callback(ParseModel(BodyText(*result.value), format));
    });
}

void Tone3000ApiClient::Shutdown()
{
  if (mState) mState->alive.store(false);
}

} // namespace amphibia::providers::tone3000
