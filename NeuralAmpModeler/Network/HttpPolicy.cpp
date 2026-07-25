#include "HttpPolicy.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace amphibia::network {
namespace {

HttpFailure Failure(providers::ProviderErrorCode code, const char* message) { return {code, message, std::nullopt}; }

std::string Lower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool IsPrivateIpv4(const std::string& host)
{
  std::array<unsigned, 4> parts{};
  std::size_t start = 0;
  for (std::size_t index = 0; index < parts.size(); ++index)
  {
    const auto end = host.find('.', start);
    const auto text = host.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (text.empty() || text.size() > 3 || (text.size() > 1 && text.front() == '0')) return false;
    unsigned value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value > 255) return false;
    parts[index] = value;
    if (index < 3 && end == std::string::npos) return false;
    if (index == 3 && end != std::string::npos) return false;
    start = end + 1;
  }
  return parts[0] == 0 || parts[0] == 10 || parts[0] == 127 || parts[0] >= 224 ||
         (parts[0] == 100 && parts[1] >= 64 && parts[1] <= 127) ||
         (parts[0] == 169 && parts[1] == 254) || (parts[0] == 172 && parts[1] >= 16 && parts[1] <= 31) ||
         (parts[0] == 192 && parts[1] == 168) || (parts[0] == 198 && (parts[1] == 18 || parts[1] == 19));
}

bool LooksLikeIpv4(const std::string& host)
{
  return !host.empty() && std::all_of(host.begin(), host.end(), [](unsigned char c) { return std::isdigit(c) || c == '.'; });
}

bool IsLoopbackIpv4(const std::string& host)
{
  const auto dot = host.find('.');
  return dot != std::string::npos && host.substr(0, dot) == "127";
}

bool ForbiddenHost(const std::string& original, bool allowLoopback)
{
  const auto host = Lower(original);
  if (host == "localhost" || host.ends_with(".localhost") || host.ends_with(".local")) return !allowLoopback;
  if (LooksLikeIpv4(host)) return IsPrivateIpv4(host) && !(allowLoopback && IsLoopbackIpv4(host));
  if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
  {
    const auto address = host.substr(1, host.size() - 2);
    const bool loopback = address == "::1";
    const bool privateAddress = loopback || address == "::" || address.starts_with("fe8") ||
      address.starts_with("fe9") || address.starts_with("fea") || address.starts_with("feb") ||
      address.starts_with("fc") || address.starts_with("fd") || address.starts_with("ff") ||
      address.starts_with("::ffff:");
    return privateAddress && !(allowLoopback && loopback);
  }
  return host.empty();
}

bool SensitiveHeader(const std::string& name)
{
  const auto lower = Lower(name);
  return lower == "authorization" || lower == "cookie" || lower == "proxy-authorization" ||
         lower == "x-api-key";
}

} // namespace

HttpResult<ParsedUrl> ParseAndValidateUrl(const std::string& url, bool allowLoopbackHttpForTest)
{
  HttpResult<ParsedUrl> result;
  if (url.empty() || url.size() > 8192 || std::any_of(url.begin(), url.end(), [](unsigned char c) { return c < 0x20 || c == 0x7f; }))
  {
    result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "URL was malformed"); return result;
  }
  const auto schemeEnd = url.find("://");
  if (schemeEnd == std::string::npos)
  {
    result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "URL scheme was missing"); return result;
  }
  ParsedUrl parsed;
  parsed.scheme = Lower(url.substr(0, schemeEnd));
  if (parsed.scheme != "https" && !(allowLoopbackHttpForTest && parsed.scheme == "http"))
  {
    result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "Provider URL must use HTTPS"); return result;
  }
  if (url.find('#', schemeEnd + 3) != std::string::npos)
  {
    result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "URL fragment is not allowed"); return result;
  }
  const auto authorityEnd = url.find_first_of("/?", schemeEnd + 3);
  const auto authority = url.substr(schemeEnd + 3, authorityEnd - (schemeEnd + 3));
  if (authority.empty() || authority.find('@') != std::string::npos)
  {
    result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "URL authority was invalid"); return result;
  }
  if (authority.front() == '[')
  {
    const auto close = authority.find(']');
    if (close == std::string::npos) { result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "IPv6 URL was invalid"); return result; }
    parsed.host = authority.substr(0, close + 1);
    if (close + 1 < authority.size())
    {
      if (authority[close + 1] != ':') { result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "URL port was invalid"); return result; }
      unsigned port = 0; const auto value = authority.substr(close + 2);
      const auto converted = std::from_chars(value.data(), value.data() + value.size(), port);
      if (converted.ec != std::errc{} || converted.ptr != value.data() + value.size() || port == 0 || port > 65535)
      { result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "URL port was invalid"); return result; }
      parsed.port = static_cast<std::uint16_t>(port);
    }
  }
  else
  {
    const auto colon = authority.rfind(':');
    parsed.host = colon == std::string::npos ? authority : authority.substr(0, colon);
    if (colon != std::string::npos)
    {
      unsigned port = 0; const auto value = authority.substr(colon + 1);
      const auto converted = std::from_chars(value.data(), value.data() + value.size(), port);
      if (converted.ec != std::errc{} || converted.ptr != value.data() + value.size() || port == 0 || port > 65535)
      { result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "URL port was invalid"); return result; }
      parsed.port = static_cast<std::uint16_t>(port);
    }
  }
  const bool allowHost = allowLoopbackHttpForTest && parsed.scheme == "http";
  if (ForbiddenHost(parsed.host, allowHost))
  {
    result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "URL destination was not public"); return result;
  }
  if (!std::all_of(parsed.host.begin(), parsed.host.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '-' || c == '[' || c == ']' || c == ':';
      }))
  { result.failure = Failure(providers::ProviderErrorCode::InvalidRedirect, "URL host was invalid"); return result; }
  parsed.host = Lower(parsed.host);
  parsed.pathAndQuery = authorityEnd == std::string::npos ? "/" : url.substr(authorityEnd);
  if (parsed.pathAndQuery.empty() || parsed.pathAndQuery.front() != '/') parsed.pathAndQuery.insert(parsed.pathAndQuery.begin(), '/');
  result.value = std::move(parsed);
  return result;
}

std::string Origin(const ParsedUrl& url)
{
  std::string result = url.scheme + "://" + url.host;
  if (url.port) result += ":" + std::to_string(*url.port);
  return result;
}

std::vector<HttpHeader> HeadersForRedirect(const std::vector<HttpHeader>& headers, bool sameOrigin)
{
  std::vector<HttpHeader> result;
  result.reserve(headers.size());
  for (const auto& header : headers)
    if (sameOrigin || !SensitiveHeader(header.name)) result.push_back(header);
  return result;
}

std::optional<std::chrono::seconds> ParseRetryAfter(const std::string& value,
                                                   std::chrono::system_clock::time_point now)
{
  if (value.empty() || value.size() > 128) return std::nullopt;
  std::uint64_t seconds = 0;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seconds);
  if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size())
    return seconds <= 24 * 60 * 60 ? std::optional(std::chrono::seconds(seconds)) : std::nullopt;
  std::tm time{};
  std::istringstream stream(value);
  stream.imbue(std::locale::classic());
  stream >> std::get_time(&time, "%a, %d %b %Y %H:%M:%S GMT");
  if (stream.fail()) return std::nullopt;
#if defined(_WIN32)
  const auto raw = _mkgmtime(&time);
#else
  const auto raw = timegm(&time);
#endif
  if (raw < 0) return std::nullopt;
  const auto target = std::chrono::system_clock::from_time_t(raw);
  if (target <= now) return std::chrono::seconds(0);
  const auto delay = std::chrono::duration_cast<std::chrono::seconds>(target - now);
  return delay <= std::chrono::hours(24) ? std::optional(delay) : std::nullopt;
}

bool IsRetryableStatus(int statusCode) noexcept
{
  return statusCode == 429 || statusCode == 500 || statusCode == 502 || statusCode == 503 || statusCode == 504;
}

std::string RedactUrl(const std::string& url)
{
  const auto query = url.find('?');
  return query == std::string::npos ? url : url.substr(0, query) + "?[redacted]";
}

std::string RedactDiagnostic(const std::string& text)
{
  if (text.size() > 2048) return "[diagnostic omitted: oversized]";
  std::istringstream lines(text);
  std::string line, output;
  while (std::getline(lines, line))
  {
    const auto colon = line.find(':');
    if (colon != std::string::npos && SensitiveHeader(line.substr(0, colon))) line = line.substr(0, colon + 1) + " [redacted]";
    const auto scheme = line.find("https://");
    if (scheme != std::string::npos)
    {
      const auto end = line.find_first_of(" \t", scheme);
      const auto original = line.substr(scheme, end - scheme);
      line.replace(scheme, original.size(), RedactUrl(original));
    }
    if (!output.empty()) output.push_back('\n');
    output += line;
  }
  return output;
}

} // namespace amphibia::network
