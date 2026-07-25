#include "PlatformHttpTransport.h"

#include "HttpPolicy.h"
#include "RequestPolicy.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>
#include <ws2tcpip.h>
#endif

namespace amphibia::network {

#if defined(_WIN32)
namespace {

struct Handle
{
  HINTERNET value{};
  Handle() = default;
  explicit Handle(HINTERNET handle) : value(handle) {}
  ~Handle() { if (value) WinHttpCloseHandle(value); }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  Handle(Handle&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
  Handle& operator=(Handle&& other) noexcept
  {
    if (this != &other) { if (value) WinHttpCloseHandle(value); value = std::exchange(other.value, nullptr); }
    return *this;
  }
};

std::wstring Wide(std::string_view value)
{
  if (value.empty()) return {};
  const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) return {};
  std::wstring output(static_cast<std::size_t>(size), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), output.data(), size) != size) return {};
  return output;
}

std::string Utf8(std::wstring_view value)
{
  if (value.empty()) return {};
  const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string output(static_cast<std::size_t>(size), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), output.data(), size, nullptr, nullptr) != size) return {};
  return output;
}

HttpFailure Failure(providers::ProviderErrorCode code, const char* message, DWORD platform = 0)
{
  return {code, message, platform ? std::optional<int>(static_cast<int>(platform)) : std::nullopt};
}

providers::ProviderErrorCode WindowsErrorCode(DWORD error)
{
  switch (error)
  {
    case ERROR_WINHTTP_TIMEOUT: return providers::ProviderErrorCode::RequestTimeout;
    case ERROR_WINHTTP_NAME_NOT_RESOLVED: return providers::ProviderErrorCode::DnsFailure;
    case ERROR_WINHTTP_SECURE_FAILURE:
    case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED: return providers::ProviderErrorCode::TlsFailure;
    case ERROR_WINHTTP_OPERATION_CANCELLED: return providers::ProviderErrorCode::Cancelled;
    default: return providers::ProviderErrorCode::Offline;
  }
}

bool PrivateAddress(const sockaddr* address)
{
  if (address->sa_family == AF_INET)
  {
    const auto raw = ntohl(reinterpret_cast<const sockaddr_in*>(address)->sin_addr.s_addr);
    const auto first = raw >> 24, second = (raw >> 16) & 255;
    return first == 0 || first == 10 || first == 127 || first >= 224 ||
           (first == 100 && second >= 64 && second <= 127) || (first == 169 && second == 254) ||
           (first == 172 && second >= 16 && second <= 31) || (first == 192 && second == 168) ||
           (first == 198 && (second == 18 || second == 19));
  }
  if (address->sa_family == AF_INET6)
  {
    const auto* bytes = reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr.u.Byte;
    const bool loopback = std::all_of(bytes, bytes + 15, [](unsigned char value) { return value == 0; }) && bytes[15] == 1;
    const bool unspecified = std::all_of(bytes, bytes + 16, [](unsigned char value) { return value == 0; });
    return loopback || unspecified || bytes[0] == 0xff || (bytes[0] & 0xfe) == 0xfc ||
           (bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80);
  }
  return true;
}

bool ResolveAllowed(const ParsedUrl& url, bool testLoopback, HttpFailure& failure)
{
  auto host = url.host;
  if (host.size() > 2 && host.front() == '[' && host.back() == ']') host = host.substr(1, host.size() - 2);
  const auto wide = Wide(host);
  ADDRINFOW hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
  PADDRINFOW result = nullptr;
  const auto status = GetAddrInfoW(wide.c_str(), nullptr, &hints, &result);
  if (status != 0 || !result) { failure = Failure(providers::ProviderErrorCode::DnsFailure, "Provider host could not be resolved", status); return false; }
  bool any = false, forbidden = false;
  for (auto* item = result; item; item = item->ai_next)
  {
    any = true;
    if (PrivateAddress(item->ai_addr))
    {
      const bool loopback = url.scheme == "http" && testLoopback &&
                            (url.host == "localhost" || url.host == "127.0.0.1" || url.host == "[::1]");
      if (!loopback) forbidden = true;
    }
  }
  FreeAddrInfoW(result);
  if (!any || forbidden) { failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "Provider host did not resolve exclusively to public addresses"); return false; }
  return true;
}

std::string Trim(std::string value)
{
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.pop_back();
  return value;
}

std::optional<std::string> HeaderValue(const std::vector<HttpHeader>& headers, std::string_view name)
{
  for (const auto& header : headers)
    if (_stricmp(header.name.c_str(), std::string(name).c_str()) == 0) return header.value;
  return std::nullopt;
}

bool RedirectStatus(int status) { return status == 301 || status == 302 || status == 303 || status == 307 || status == 308; }

std::string ResolveRedirect(const ParsedUrl& current, const std::string& location)
{
  if (location.starts_with("https://") || location.starts_with("http://")) return location;
  if (!location.empty() && location.front() == '/') return Origin(current) + location;
  auto base = current.pathAndQuery.substr(0, current.pathAndQuery.find('?'));
  const auto slash = base.rfind('/');
  base = slash == std::string::npos ? "/" : base.substr(0, slash + 1);
  return Origin(current) + base + location;
}

std::optional<std::uint64_t> ContentLength(const std::vector<HttpHeader>& headers, bool& invalid)
{
  invalid = false; std::optional<std::uint64_t> found;
  for (const auto& header : headers)
  {
    if (_stricmp(header.name.c_str(), "Content-Length") != 0) continue;
    std::uint64_t value = 0;
    const auto converted = std::from_chars(header.value.data(), header.value.data() + header.value.size(), value);
    if (converted.ec != std::errc{} || converted.ptr != header.value.data() + header.value.size() || (found && *found != value))
    { invalid = true; return std::nullopt; }
    found = value;
  }
  return found;
}

} // namespace

struct PlatformHttpTransport::Impl
{
  struct Task
  {
    RequestHandle id;
    std::atomic<bool> cancelled{};
    std::atomic<bool> completed{};
    std::atomic<HINTERNET> nativeRequest{};
    std::thread worker;
  };

  Handle session;
  std::atomic<std::uint64_t> nextId{};
  std::atomic<bool> shuttingDown{};
  std::mutex mutex;
  std::map<std::uint64_t, std::shared_ptr<Task>> tasks;
  SlidingWindowRateLimiter limiter;

  Impl()
    : session(WinHttpOpen(L"Amphibia/0.1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0))
  {
  }

  ~Impl() { Shutdown(); }

  void CloseNative(const std::shared_ptr<Task>& task)
  {
    if (const auto request = task->nativeRequest.exchange(nullptr)) WinHttpCloseHandle(request);
  }

  void Reap()
  {
    std::vector<std::shared_ptr<Task>> finished;
    {
      std::lock_guard lock(mutex);
      for (auto iterator = tasks.begin(); iterator != tasks.end();)
      {
        if (iterator->second->completed.load()) { finished.push_back(iterator->second); iterator = tasks.erase(iterator); }
        else ++iterator;
      }
    }
    for (const auto& task : finished) if (task->worker.joinable()) task->worker.join();
  }

  bool WaitUntil(const std::shared_ptr<Task>& task, std::chrono::steady_clock::time_point deadline)
  {
    while (std::chrono::steady_clock::now() < deadline)
    {
      if (task->cancelled.load() || shuttingDown.load()) return false;
      std::this_thread::sleep_for((std::min)(std::chrono::milliseconds(25),
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now())));
    }
    return !task->cancelled.load() && !shuttingDown.load();
  }

  std::uint32_t Jitter()
  {
    std::uint32_t value = 0;
    BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&value), sizeof(value), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return value;
  }

  template <typename Result, typename Operation>
  Result ExecuteWithPolicy(const std::shared_ptr<Task>& task, const HttpRequest& request, Operation operation)
  {
    const auto reservation = limiter.Reserve(std::chrono::steady_clock::now());
    if (!reservation)
    {
      Result rejected; rejected.failure = Failure(providers::ProviderErrorCode::RateLimited, "HTTP request queue is full"); return rejected;
    }
    if (!WaitUntil(task, *reservation))
    {
      Result cancelled; cancelled.failure = Failure(providers::ProviderErrorCode::Cancelled, "Request was cancelled"); return cancelled;
    }
    for (int attempt = 1;; ++attempt)
    {
      auto result = operation();
      std::optional<HttpResponse> policyResponse;
      if (result.value)
      {
        policyResponse.emplace(); policyResponse->statusCode = result.value->statusCode;
        policyResponse->headers = result.value->headers;
      }
      const auto delay = RetryDelay(request, attempt, policyResponse, result.failure,
                                    std::chrono::system_clock::now(), Jitter());
      if (!delay) return result;
      if (result.value && result.value->statusCode == 429) limiter.ApplyServerDelay(std::chrono::steady_clock::now() + *delay);
      if (!WaitUntil(task, std::chrono::steady_clock::now() + *delay))
      { Result cancelled; cancelled.failure = Failure(providers::ProviderErrorCode::Cancelled, "Request was cancelled"); return cancelled; }
    }
  }

  std::shared_ptr<Task> NewTask()
  {
    if (shuttingDown.load() || !session.value) return {};
    Reap();
    auto task = std::make_shared<Task>(); task->id = {++nextId};
    std::lock_guard lock(mutex); tasks.emplace(task->id.value, task); return task;
  }

  bool OpenResponse(const std::shared_ptr<Task>& task, const HttpRequest& request, const ParsedUrl& parsed,
                    const std::vector<HttpHeader>& headers, Handle& connection, HINTERNET& native,
                    int& status, std::vector<HttpHeader>& responseHeaders, HttpFailure& failure)
  {
    if (task->cancelled.load()) { failure = Failure(providers::ProviderErrorCode::Cancelled, "Request was cancelled"); return false; }
    if (!ResolveAllowed(parsed, request.allowLoopbackHttpForTest, failure)) return false;
    auto host = parsed.host; if (host.front() == '[') host = host.substr(1, host.size() - 2);
    const auto wideHost = Wide(host), path = Wide(parsed.pathAndQuery);
    const auto port = parsed.port.value_or(parsed.scheme == "https" ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);
    connection = Handle(WinHttpConnect(session.value, wideHost.c_str(), port, 0));
    if (!connection.value) { const auto error = GetLastError(); failure = Failure(WindowsErrorCode(error), "Provider connection could not be opened", error); return false; }
    const wchar_t* verb = request.method == HttpMethod::Post ? L"POST" : L"GET";
    native = WinHttpOpenRequest(connection.value, verb, path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                WINHTTP_DEFAULT_ACCEPT_TYPES, parsed.scheme == "https" ? WINHTTP_FLAG_SECURE : 0);
    if (!native) { const auto error = GetLastError(); failure = Failure(WindowsErrorCode(error), "Provider request could not be opened", error); return false; }
    task->nativeRequest.store(native);
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES;
    WinHttpSetOption(native, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled));
    WinHttpSetTimeouts(native, static_cast<int>(request.connectTimeout.count()), static_cast<int>(request.connectTimeout.count()),
                       static_cast<int>(request.totalTimeout.count()), static_cast<int>(request.totalTimeout.count()));
    std::wstring headerBlock;
    for (const auto& header : headers) headerBlock += Wide(header.name + ": " + header.value + "\r\n");
    const auto bodySize = static_cast<DWORD>(request.body.size());
    if (request.body.size() > 1024 * 1024)
    { failure = Failure(providers::ProviderErrorCode::ContentTooLarge, "Request body exceeded limits"); CloseNative(task); return false; }
    if (!WinHttpSendRequest(native, headerBlock.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerBlock.c_str(),
                            headerBlock.empty() ? 0 : static_cast<DWORD>(headerBlock.size()),
                            request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<std::uint8_t*>(request.body.data()),
                            bodySize, bodySize, 0) || !WinHttpReceiveResponse(native, nullptr))
    {
      const auto error = GetLastError(); failure = Failure(task->cancelled.load() ? providers::ProviderErrorCode::Cancelled : WindowsErrorCode(error),
                                                           task->cancelled.load() ? "Request was cancelled" : "Provider request failed", error);
      CloseNative(task); return false;
    }
    DWORD code = 0, codeSize = sizeof(code);
    if (!WinHttpQueryHeaders(native, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &code, &codeSize, WINHTTP_NO_HEADER_INDEX))
    { const auto error = GetLastError(); failure = Failure(WindowsErrorCode(error), "Provider status was unavailable", error); CloseNative(task); return false; }
    status = static_cast<int>(code);
    DWORD rawSize = 0;
    WinHttpQueryHeaders(native, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &rawSize, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || rawSize > request.maximumHeaderBytes * sizeof(wchar_t))
    { failure = Failure(providers::ProviderErrorCode::ContentTooLarge, "Provider headers exceeded limits"); CloseNative(task); return false; }
    std::wstring raw(rawSize / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(native, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &rawSize, WINHTTP_NO_HEADER_INDEX))
    { const auto error = GetLastError(); failure = Failure(WindowsErrorCode(error), "Provider headers were unavailable", error); CloseNative(task); return false; }
    std::size_t start = raw.find(L"\r\n") + 2;
    while (start < raw.size())
    {
      const auto end = raw.find(L"\r\n", start); if (end == std::wstring::npos || end == start) break;
      const auto line = Utf8(std::wstring_view(raw).substr(start, end - start)); const auto colon = line.find(':');
      if (colon != std::string::npos) responseHeaders.push_back({Trim(line.substr(0, colon)), Trim(line.substr(colon + 1))});
      start = end + 2;
    }
    return true;
  }

  HttpResult<HttpResponse> Buffered(const std::shared_ptr<Task>& task, HttpRequest request)
  {
    HttpResult<HttpResponse> result; auto url = request.url; auto headers = request.headers;
    for (int redirects = 0;; ++redirects)
    {
      const auto parsedResult = ParseAndValidateUrl(url, request.allowLoopbackHttpForTest);
      if (!parsedResult) { result.failure = *parsedResult.failure; return result; }
      Handle connection; HINTERNET native = nullptr; int status = 0; std::vector<HttpHeader> responseHeaders; HttpFailure failure;
      if (!OpenResponse(task, request, *parsedResult.value, headers, connection, native, status, responseHeaders, failure))
      { result.failure = std::move(failure); return result; }
      if (RedirectStatus(status))
      {
        const auto location = HeaderValue(responseHeaders, "Location"); CloseNative(task);
        if (!location || redirects >= request.maximumRedirects)
        { result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "Provider redirect was rejected"); return result; }
        const auto next = ResolveRedirect(*parsedResult.value, *location);
        const auto nextParsed = ParseAndValidateUrl(next, request.allowLoopbackHttpForTest);
        if (!nextParsed || (parsedResult.value->scheme == "https" && nextParsed.value->scheme != "https"))
        { result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "Provider redirect downgraded or was unsafe"); return result; }
        headers = HeadersForRedirect(headers, Origin(*parsedResult.value) == Origin(*nextParsed.value)); url = next; continue;
      }
      bool invalidLength = false; const auto length = ContentLength(responseHeaders, invalidLength);
      if (invalidLength || (length && *length > request.maximumResponseBytes))
      { CloseNative(task); result.failure = Failure(providers::ProviderErrorCode::ContentTooLarge, "Provider response exceeded limits"); return result; }
      HttpResponse response; response.statusCode = status; response.headers = std::move(responseHeaders); response.finalUrl = url;
      std::array<std::uint8_t, 64 * 1024> buffer{};
      for (;;)
      {
        if (task->cancelled.load()) { CloseNative(task); result.failure = Failure(providers::ProviderErrorCode::Cancelled, "Request was cancelled"); return result; }
        DWORD read = 0;
        if (!WinHttpReadData(native, buffer.data(), static_cast<DWORD>(buffer.size()), &read))
        { const auto error = GetLastError(); CloseNative(task); result.failure = Failure(WindowsErrorCode(error), "Provider response could not be read", error); return result; }
        if (!read) break;
        if (response.body.size() > request.maximumResponseBytes - read)
        { CloseNative(task); result.failure = Failure(providers::ProviderErrorCode::ContentTooLarge, "Provider response exceeded limits"); return result; }
        response.body.insert(response.body.end(), buffer.begin(), buffer.begin() + read);
      }
      CloseNative(task);
      if (length && *length != response.body.size()) { result.failure = Failure(providers::ProviderErrorCode::InvalidResponse, "Provider response length did not match"); return result; }
      result.value = std::move(response); return result;
    }
  }

  HttpResult<StreamingDownloadResponse> Download(const std::shared_ptr<Task>& task,
                                                  StreamingDownloadRequest download,
                                                  const DownloadProgress& progress)
  {
    HttpResult<StreamingDownloadResponse> result;
    if (download.maximumBytes == 0 || download.destination.extension() != L".part")
    { result.failure = Failure(providers::ProviderErrorCode::StorageFailed, "Download destination or limit was invalid"); return result; }
    std::error_code fileError; const auto space = std::filesystem::space(download.destination.parent_path(), fileError);
    if (fileError || space.available < download.minimumFreeSpaceReserve ||
        space.available - download.minimumFreeSpaceReserve < download.maximumBytes)
    { result.failure = Failure(providers::ProviderErrorCode::StorageFailed, "Insufficient free space for download"); return result; }
    const HANDLE file = CreateFileW(download.destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    { result.failure = Failure(providers::ProviderErrorCode::StorageFailed, "Download staging file could not be created", GetLastError()); return result; }
    struct FileCleanup { HANDLE file; std::filesystem::path path; bool keep{}; ~FileCleanup(){ CloseHandle(file); if (!keep) { std::error_code e; std::filesystem::remove(path, e); } } } cleanup{file, download.destination};
    auto url = download.request.url; auto headers = download.request.headers;
    for (int redirects = 0;; ++redirects)
    {
      const auto parsedResult = ParseAndValidateUrl(url, download.request.allowLoopbackHttpForTest);
      if (!parsedResult) { result.failure = *parsedResult.failure; return result; }
      Handle connection; HINTERNET native = nullptr; int status = 0; std::vector<HttpHeader> responseHeaders; HttpFailure failure;
      if (!OpenResponse(task, download.request, *parsedResult.value, headers, connection, native, status, responseHeaders, failure))
      { result.failure = std::move(failure); return result; }
      if (RedirectStatus(status))
      {
        const auto location = HeaderValue(responseHeaders, "Location"); CloseNative(task);
        if (!location || redirects >= download.request.maximumRedirects)
        { result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "Download redirect was rejected"); return result; }
        const auto next = ResolveRedirect(*parsedResult.value, *location);
        const auto nextParsed = ParseAndValidateUrl(next, download.request.allowLoopbackHttpForTest);
        if (!nextParsed || (parsedResult.value->scheme == "https" && nextParsed.value->scheme != "https"))
        { result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "Download redirect downgraded or was unsafe"); return result; }
        headers = HeadersForRedirect(headers, Origin(*parsedResult.value) == Origin(*nextParsed.value)); url = next; continue;
      }
      bool invalidLength = false; const auto declared = ContentLength(responseHeaders, invalidLength);
      if (invalidLength || (declared && *declared > download.maximumBytes))
      { CloseNative(task); result.failure = Failure(providers::ProviderErrorCode::ContentTooLarge, "Download exceeded its declared limit"); return result; }
      if (status < 200 || status >= 300)
      { CloseNative(task); StreamingDownloadResponse response{status, std::move(responseHeaders), url, 0}; result.value = std::move(response); return result; }
      std::uint64_t bytes = 0; std::array<std::uint8_t, 64 * 1024> buffer{};
      for (;;)
      {
        if (task->cancelled.load()) { CloseNative(task); result.failure = Failure(providers::ProviderErrorCode::Cancelled, "Download was cancelled"); return result; }
        DWORD read = 0;
        if (!WinHttpReadData(native, buffer.data(), static_cast<DWORD>(buffer.size()), &read))
        { const auto error = GetLastError(); CloseNative(task); result.failure = Failure(WindowsErrorCode(error), "Download stream failed", error); return result; }
        if (!read) break;
        if (bytes > download.maximumBytes - read)
        { CloseNative(task); result.failure = Failure(providers::ProviderErrorCode::ContentTooLarge, "Download exceeded its byte limit"); return result; }
        DWORD written = 0;
        if (!WriteFile(file, buffer.data(), read, &written, nullptr) || written != read)
        { CloseNative(task); result.failure = Failure(providers::ProviderErrorCode::StorageFailed, "Download staging write failed", GetLastError()); return result; }
        bytes += read; if (progress) progress(task->id, bytes, declared);
      }
      CloseNative(task);
      if ((declared && *declared != bytes) || !FlushFileBuffers(file))
      { result.failure = Failure(declared && *declared != bytes ? providers::ProviderErrorCode::InvalidResponse : providers::ProviderErrorCode::StorageFailed,
                                 declared && *declared != bytes ? "Download length did not match" : "Download staging flush failed", GetLastError()); return result; }
      cleanup.keep = true; result.value = StreamingDownloadResponse{status, std::move(responseHeaders), url, bytes}; return result;
    }
  }

  void Cancel(RequestHandle handle)
  {
    std::shared_ptr<Task> task; { std::lock_guard lock(mutex); const auto found = tasks.find(handle.value); if (found != tasks.end()) task = found->second; }
    if (task) { task->cancelled.store(true); CloseNative(task); }
  }

  void Shutdown()
  {
    if (shuttingDown.exchange(true)) return;
    std::vector<std::shared_ptr<Task>> pending; { std::lock_guard lock(mutex); for (const auto& [id, task] : tasks) pending.push_back(task); }
    for (const auto& task : pending) { task->cancelled.store(true); CloseNative(task); }
    for (const auto& task : pending) if (task->worker.joinable()) task->worker.join();
    { std::lock_guard lock(mutex); tasks.clear(); }
  }
};

PlatformHttpTransport::PlatformHttpTransport() : mImpl(std::make_unique<Impl>()) {}
PlatformHttpTransport::~PlatformHttpTransport() { Shutdown(); }

RequestHandle PlatformHttpTransport::Begin(HttpRequest request, HttpCompletion completion)
{
  auto task = mImpl->NewTask(); if (!task) { if (completion) completion({}, {{}, Failure(providers::ProviderErrorCode::InternalError, "HTTP transport is unavailable")}); return {}; }
  auto* impl = mImpl.get();
  task->worker = std::thread([impl, task, request = std::move(request), completion = std::move(completion)]() mutable {
    auto result = impl->ExecuteWithPolicy<HttpResult<HttpResponse>>(task, request, [&] { return impl->Buffered(task, request); });
    if (!impl->shuttingDown.load() && completion) completion(task->id, std::move(result));
    task->completed.store(true);
  });
  return task->id;
}

RequestHandle PlatformHttpTransport::BeginDownload(StreamingDownloadRequest request, DownloadProgress progress,
                                                    DownloadCompletion completion)
{
  auto task = mImpl->NewTask(); if (!task) { if (completion) completion({}, {{}, Failure(providers::ProviderErrorCode::InternalError, "HTTP transport is unavailable")}); return {}; }
  auto* impl = mImpl.get();
  task->worker = std::thread([impl, task, request = std::move(request), progress = std::move(progress), completion = std::move(completion)]() mutable {
    auto result = impl->ExecuteWithPolicy<HttpResult<StreamingDownloadResponse>>(task, request.request,
      [&] { return impl->Download(task, request, progress); });
    if (!impl->shuttingDown.load() && completion) completion(task->id, std::move(result));
    task->completed.store(true);
  });
  return task->id;
}

void PlatformHttpTransport::Cancel(RequestHandle handle) { mImpl->Cancel(handle); }
void PlatformHttpTransport::Shutdown() { if (mImpl) mImpl->Shutdown(); }

#else

struct PlatformHttpTransport::Impl {};
PlatformHttpTransport::PlatformHttpTransport() : mImpl(std::make_unique<Impl>()) {}
PlatformHttpTransport::~PlatformHttpTransport() = default;
RequestHandle PlatformHttpTransport::Begin(HttpRequest, HttpCompletion completion)
{
  if (completion) completion({}, {{}, HttpFailure{providers::ProviderErrorCode::InternalError, "Platform HTTP transport is unavailable"}}); return {};
}
RequestHandle PlatformHttpTransport::BeginDownload(StreamingDownloadRequest, DownloadProgress, DownloadCompletion completion)
{
  if (completion) completion({}, {{}, HttpFailure{providers::ProviderErrorCode::InternalError, "Platform HTTP transport is unavailable"}}); return {};
}
void PlatformHttpTransport::Cancel(RequestHandle) {}
void PlatformHttpTransport::Shutdown() {}

#endif

} // namespace amphibia::network
