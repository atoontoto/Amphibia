#pragma once

#include "HttpTypes.h"

#include <chrono>
#include <optional>
#include <string>

namespace amphibia::network {

struct ParsedUrl
{
  std::string scheme;
  std::string host;
  std::optional<std::uint16_t> port;
  std::string pathAndQuery;
};

HttpResult<ParsedUrl> ParseAndValidateUrl(const std::string& url, bool allowLoopbackHttpForTest = false);
std::string Origin(const ParsedUrl& url);
std::vector<HttpHeader> HeadersForRedirect(const std::vector<HttpHeader>& headers, bool sameOrigin);
std::optional<std::chrono::seconds> ParseRetryAfter(const std::string& value,
                                                   std::chrono::system_clock::time_point now);
bool IsRetryableStatus(int statusCode) noexcept;
std::string RedactUrl(const std::string& url);
std::string RedactDiagnostic(const std::string& text);

} // namespace amphibia::network
