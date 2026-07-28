#include "FixedLoopbackCallbackReceiver.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace amphibia::platform {
namespace {

constexpr char kRegisteredRedirect[] = "http://127.0.0.1:43821/oauth/callback";
constexpr char kCallbackPath[] = "/oauth/callback";
constexpr char kExpectedHost[] = "127.0.0.1:43821";
constexpr std::uint16_t kCallbackPort = 43821;
constexpr std::size_t kMaximumRequestLine = 4096;
constexpr std::size_t kMaximumHeaders = 8192;

providers::ProviderError Failure(providers::ProviderErrorCode code, const char* message,
                                 const char* diagnostic = nullptr)
{
  providers::ProviderError result{code, message};
  if (diagnostic) result.redactedDiagnostic = diagnostic;
  return result;
}

std::string LowerAscii(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

std::string TrimHttpWhitespace(std::string value)
{
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
  return value;
}

#if defined(_WIN32)
void CloseSocket(SOCKET socket)
{
  if (socket != INVALID_SOCKET)
  {
    shutdown(socket, SD_BOTH);
    closesocket(socket);
  }
}

bool SendResponse(SOCKET socket, int status, const char* reason, const char* body)
{
  const std::string content(body);
  const std::string response =
    "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "Pragma: no-cache\r\n"
    "X-Content-Type-Options: nosniff\r\n"
    "Referrer-Policy: no-referrer\r\n"
    "Connection: close\r\n"
    "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
  std::size_t sent = 0;
  while (sent < response.size())
  {
    const int count = send(socket, response.data() + sent,
                           static_cast<int>((std::min)(response.size() - sent, std::size_t{4096})), 0);
    if (count <= 0) return false;
    sent += static_cast<std::size_t>(count);
  }
  return true;
}
#endif

} // namespace

struct FixedLoopbackCallbackReceiver::State
{
  mutable std::mutex mutex;
  std::thread worker;
  providers::HostedCallbackCompletion completion;
  std::chrono::steady_clock::time_point deadline;
  std::uint64_t generation{};
  std::atomic<bool> stop{false};
  bool active{};
  bool shutdown{};
#if defined(_WIN32)
  SOCKET listener{INVALID_SOCKET};
  bool winsockReady{};
#endif
};

FixedLoopbackCallbackReceiver::FixedLoopbackCallbackReceiver()
: mState(std::make_shared<State>())
{
#if defined(_WIN32)
  WSADATA data{};
  mState->winsockReady = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#endif
}

FixedLoopbackCallbackReceiver::~FixedLoopbackCallbackReceiver()
{
  Shutdown();
#if defined(_WIN32)
  if (mState && mState->winsockReady) WSACleanup();
#endif
}

providers::ProviderError FixedLoopbackCallbackReceiver::Start(
  const std::string& exactRegisteredRedirect,
  std::chrono::steady_clock::time_point deadline,
  providers::HostedCallbackCompletion completion)
{
  if (exactRegisteredRedirect != kRegisteredRedirect)
    return Failure(providers::ProviderErrorCode::InvalidRedirect,
                   "The registered TONE3000 callback URI does not match this build.");
  if (!completion || deadline <= std::chrono::steady_clock::now())
    return Failure(providers::ProviderErrorCode::InvalidResponse,
                   "The TONE3000 authorization deadline is invalid.");
#if !defined(_WIN32)
  (void)deadline;
  (void)completion;
  return Failure(providers::ProviderErrorCode::Unconfigured,
                 "The fixed TONE3000 callback receiver is unavailable on this platform.");
#else
  std::thread previous;
  {
    std::lock_guard lock(mState->mutex);
    if (mState->shutdown)
      return Failure(providers::ProviderErrorCode::Cancelled,
                     "The TONE3000 callback receiver is shutting down.");
    if (mState->active)
      return Failure(providers::ProviderErrorCode::Superseded,
                     "A TONE3000 callback listener is already active.");
    if (!mState->winsockReady)
      return Failure(providers::ProviderErrorCode::InternalError,
                     "Windows networking is unavailable for the local callback.");
    if (mState->worker.joinable()) previous = std::move(mState->worker);
  }
  if (previous.joinable()) previous.join();

  SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET)
    return Failure(providers::ProviderErrorCode::InternalError,
                   "The local callback socket could not be created.", "socket_create");
  BOOL exclusive = TRUE;
  if (setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                 reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR)
  {
    CloseSocket(listener);
    return Failure(providers::ProviderErrorCode::InternalError,
                   "The local callback socket could not be made exclusive.", "exclusive_address");
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(kCallbackPort);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
  {
    const int error = WSAGetLastError();
    CloseSocket(listener);
    if (error == WSAEADDRINUSE || error == WSAEACCES)
      return Failure(providers::ProviderErrorCode::LocalPortInUse,
        "TONE3000 authorization could not start because local callback port 43821 is already in use.\n\n"
        "Close the application using that port and try again.", "127.0.0.1:43821 bind_in_use");
    return Failure(providers::ProviderErrorCode::InternalError,
                   "The local TONE3000 callback listener could not bind.", "127.0.0.1:43821 bind_failed");
  }
  if (listen(listener, 1) == SOCKET_ERROR)
  {
    CloseSocket(listener);
    return Failure(providers::ProviderErrorCode::InternalError,
                   "The local TONE3000 callback listener could not start.", "listen_failed");
  }

  std::uint64_t generation{};
  {
    std::lock_guard lock(mState->mutex);
    mState->stop.store(false);
    mState->active = true;
    mState->listener = listener;
    mState->deadline = deadline;
    mState->completion = std::move(completion);
    generation = ++mState->generation;
  }

  const auto state = mState;
  state->worker = std::thread([state, generation, listener] {
    providers::ProviderResult<std::string> result;
    for (;;)
    {
      if (state->stop.load()) return;
      if (std::chrono::steady_clock::now() >= state->deadline)
      {
        result.error = Failure(providers::ProviderErrorCode::RequestTimeout,
                               "TONE3000 authorization timed out.");
        break;
      }
      fd_set readSet;
      FD_ZERO(&readSet);
      FD_SET(listener, &readSet);
      timeval timeout{0, 100000};
      const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
      if (ready == 0) continue;
      if (ready == SOCKET_ERROR)
      {
        if (state->stop.load()) return;
        result.error = Failure(providers::ProviderErrorCode::InternalError,
                               "The local TONE3000 callback listener failed.", "select_failed");
        break;
      }

      sockaddr_in peer{};
      int peerSize = sizeof(peer);
      SOCKET connection = accept(listener, reinterpret_cast<sockaddr*>(&peer), &peerSize);
      if (connection == INVALID_SOCKET)
      {
        if (state->stop.load()) return;
        result.error = Failure(providers::ProviderErrorCode::InvalidResponse,
                               "The local authorization callback could not be accepted.");
        break;
      }
      if (peer.sin_family != AF_INET || ntohl(peer.sin_addr.s_addr) != INADDR_LOOPBACK)
      {
        SendResponse(connection, 403, "Forbidden", "Authorization callback rejected.");
        CloseSocket(connection);
        result.error = Failure(providers::ProviderErrorCode::InvalidRedirect,
                               "The authorization callback was not received from IPv4 loopback.");
        break;
      }

      std::string request;
      std::array<char, 1024> buffer{};
      bool complete = false;
      while (request.size() <= kMaximumHeaders)
      {
        const int count = recv(connection, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (count <= 0) break;
        request.append(buffer.data(), static_cast<std::size_t>(count));
        if (request.find("\r\n\r\n") != std::string::npos)
        {
          complete = true;
          break;
        }
      }
      const auto lineEnd = request.find("\r\n");
      if (!complete || lineEnd == std::string::npos || lineEnd > kMaximumRequestLine ||
          request.size() > kMaximumHeaders)
      {
        SendResponse(connection, 431, "Request Header Fields Too Large", "Authorization callback rejected.");
        CloseSocket(connection);
        result.error = Failure(providers::ProviderErrorCode::InvalidResponse,
                               "The authorization callback request was oversized or incomplete.");
        break;
      }
      const std::string requestLine = request.substr(0, lineEnd);
      const auto firstSpace = requestLine.find(' ');
      const auto secondSpace = firstSpace == std::string::npos ? std::string::npos :
                               requestLine.find(' ', firstSpace + 1);
      if (firstSpace == std::string::npos || secondSpace == std::string::npos ||
          requestLine.substr(0, firstSpace) != "GET" ||
          requestLine.substr(secondSpace + 1) != "HTTP/1.1")
      {
        SendResponse(connection, 405, "Method Not Allowed", "Authorization callback rejected.");
        CloseSocket(connection);
        result.error = Failure(providers::ProviderErrorCode::InvalidResponse,
                               "The authorization callback HTTP method was invalid.");
        break;
      }
      bool hostSeen = false;
      bool validHost = true;
      auto headerAt = lineEnd + 2;
      while (headerAt < request.size())
      {
        const auto headerEnd = request.find("\r\n", headerAt);
        if (headerEnd == std::string::npos || headerEnd == headerAt) break;
        const auto colon = request.find(':', headerAt);
        if (colon == std::string::npos || colon >= headerEnd)
        {
          validHost = false;
          break;
        }
        const auto name = LowerAscii(request.substr(headerAt, colon - headerAt));
        if (name == "host")
        {
          if (hostSeen || TrimHttpWhitespace(request.substr(colon + 1, headerEnd - colon - 1)) != kExpectedHost)
          {
            validHost = false;
            break;
          }
          hostSeen = true;
        }
        headerAt = headerEnd + 2;
      }
      if (!hostSeen || !validHost)
      {
        SendResponse(connection, 400, "Bad Request", "Authorization callback rejected.");
        CloseSocket(connection);
        result.error = Failure(providers::ProviderErrorCode::InvalidRedirect,
                               "The authorization callback Host header did not match.");
        break;
      }
      const std::string target = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
      if (target != kCallbackPath && !target.starts_with(std::string(kCallbackPath) + "?"))
      {
        SendResponse(connection, 404, "Not Found", "Authorization callback rejected.");
        CloseSocket(connection);
        result.error = Failure(providers::ProviderErrorCode::InvalidRedirect,
                               "The authorization callback path did not match.");
        break;
      }
      if (target == kCallbackPath)
      {
        SendResponse(connection, 400, "Bad Request", "Authorization callback rejected.");
        CloseSocket(connection);
        result.error = Failure(providers::ProviderErrorCode::InvalidResponse,
                               "The authorization callback was missing query values.");
        break;
      }
      SendResponse(connection, 200, "OK", "Authorization received. You may return to Amphibia.");
      CloseSocket(connection);
      result.value = std::string("http://127.0.0.1:43821") + target;
      break;
    }

    providers::HostedCallbackCompletion completion;
    SOCKET listenerToClose = INVALID_SOCKET;
    {
      std::lock_guard lock(state->mutex);
      if (state->generation != generation || state->stop.load()) return;
      listenerToClose = state->listener;
      state->listener = INVALID_SOCKET;
      state->active = false;
      completion = std::move(state->completion);
    }
    CloseSocket(listenerToClose);
    if (completion) completion(std::move(result));
  });
  return {};
#endif
}

void FixedLoopbackCallbackReceiver::Cancel()
{
  if (!mState) return;
#if defined(_WIN32)
  std::thread worker;
  SOCKET listener = INVALID_SOCKET;
  {
    std::lock_guard lock(mState->mutex);
    mState->stop.store(true);
    ++mState->generation;
    mState->active = false;
    mState->completion = {};
    listener = mState->listener;
    mState->listener = INVALID_SOCKET;
    if (mState->worker.joinable() && mState->worker.get_id() != std::this_thread::get_id())
      worker = std::move(mState->worker);
  }
  CloseSocket(listener);
  if (worker.joinable()) worker.join();
#endif
}

void FixedLoopbackCallbackReceiver::Shutdown()
{
  if (!mState) return;
  {
    std::lock_guard lock(mState->mutex);
    if (mState->shutdown) return;
    mState->shutdown = true;
  }
  Cancel();
}

bool FixedLoopbackCallbackReceiver::IsActive() const
{
  std::lock_guard lock(mState->mutex);
  return mState->active;
}

} // namespace amphibia::platform
