#pragma once

#include "../Providers/ProviderTypes.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace amphibia::network {

enum class HttpMethod { Get, Post };
enum class HttpEndpointCategory { OAuthToken, ProviderApi, ModelDownload, TestOnly };

struct HttpRetryPolicy
{
  int maximumAttempts{3};
  std::chrono::milliseconds baseDelay{250};
  std::chrono::milliseconds maximumDelay{5000};
};

struct HttpHeader { std::string name; std::string value; };

struct HttpRequest
{
  HttpMethod method{HttpMethod::Get};
  std::string url;
  std::vector<HttpHeader> headers;
  std::vector<std::uint8_t> body;
  std::chrono::milliseconds connectTimeout{10000};
  std::chrono::milliseconds totalTimeout{30000};
  std::size_t maximumResponseBytes{4 * 1024 * 1024};
  std::size_t maximumHeaderBytes{64 * 1024};
  int maximumRedirects{3};
  HttpEndpointCategory category{HttpEndpointCategory::ProviderApi};
  HttpRetryPolicy retry;
  bool idempotent{};
  bool allowLoopbackHttpForTest{};
};

struct HttpResponse
{
  int statusCode{};
  std::vector<HttpHeader> headers;
  std::vector<std::uint8_t> body;
  std::string finalUrl;
};

struct HttpFailure
{
  providers::ProviderErrorCode code{providers::ProviderErrorCode::InternalError};
  std::string safeMessage;
  std::optional<int> platformCode;
};

template <typename T> struct HttpResult
{
  std::optional<T> value;
  std::optional<HttpFailure> failure;
  explicit operator bool() const noexcept { return value.has_value() && !failure; }
};

struct RequestHandle { std::uint64_t value{}; friend bool operator==(const RequestHandle&, const RequestHandle&) = default; };
using HttpCompletion = std::function<void(RequestHandle, HttpResult<HttpResponse>)>;

struct StreamingDownloadRequest
{
  HttpRequest request;
  std::filesystem::path destination;
  std::uint64_t maximumBytes{};
  std::uint64_t minimumFreeSpaceReserve{256ull * 1024 * 1024};
};

struct StreamingDownloadResponse
{
  int statusCode{};
  std::vector<HttpHeader> headers;
  std::string finalUrl;
  std::uint64_t bytes{};
};

using DownloadProgress = std::function<void(RequestHandle, std::uint64_t, std::optional<std::uint64_t>)>;
using DownloadCompletion = std::function<void(RequestHandle, HttpResult<StreamingDownloadResponse>)>;

} // namespace amphibia::network
