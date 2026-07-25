#include "OAuthCallback.h"

#include <cctype>
#include <unordered_map>

namespace amphibia::providers::tone3000 {
namespace {

ProviderError Failure(ProviderErrorCode code, const char* message)
{
  return {code, message, std::nullopt, std::nullopt, std::nullopt};
}

bool Hex(unsigned char value, unsigned char& decoded)
{
  if (value >= '0' && value <= '9') decoded = value - '0';
  else if (value >= 'a' && value <= 'f') decoded = value - 'a' + 10;
  else if (value >= 'A' && value <= 'F') decoded = value - 'A' + 10;
  else return false;
  return true;
}

bool PercentDecode(const std::string& value, std::string& decoded)
{
  if (value.size() > 2048) return false;
  decoded.clear();
  decoded.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index)
  {
    if (value[index] == '%')
    {
      unsigned char high = 0, low = 0;
      if (index + 2 >= value.size() || !Hex(value[index + 1], high) || !Hex(value[index + 2], low)) return false;
      const auto byte = static_cast<unsigned char>((high << 4) | low);
      if (byte == 0 || byte < 0x20) return false;
      decoded.push_back(static_cast<char>(byte));
      index += 2;
    }
    else if (value[index] == '+') decoded.push_back(' ');
    else
    {
      if (static_cast<unsigned char>(value[index]) < 0x20) return false;
      decoded.push_back(value[index]);
    }
  }
  return true;
}

} // namespace

ProviderResult<OAuthCallbackValues> ParseOAuthCallback(const std::string& callbackUri,
                                                       const std::string& exactRedirectUri)
{
  ProviderResult<OAuthCallbackValues> result;
  if (callbackUri.size() > 4096 || exactRedirectUri.empty() || exactRedirectUri.size() > 2048)
  {
    result.error = Failure(ProviderErrorCode::InvalidResponse, "Authorization callback was too large");
    return result;
  }
  const auto callbackQuery = callbackUri.find('?');
  const auto redirectQuery = exactRedirectUri.find('?');
  const auto callbackBase = callbackUri.substr(0, callbackQuery);
  const auto redirectBase = exactRedirectUri.substr(0, redirectQuery);
  if (callbackBase != redirectBase || redirectQuery != std::string::npos)
  {
    result.error = Failure(ProviderErrorCode::InvalidRedirect, "Authorization callback route did not match");
    return result;
  }
  if (callbackQuery == std::string::npos || callbackQuery + 1 == callbackUri.size())
  {
    result.error = Failure(ProviderErrorCode::InvalidResponse, "Authorization callback was missing query values");
    return result;
  }

  std::unordered_map<std::string, std::string> fields;
  std::size_t start = callbackQuery + 1;
  while (start <= callbackUri.size())
  {
    const auto end = callbackUri.find('&', start);
    const auto item = callbackUri.substr(start, end == std::string::npos ? std::string::npos : end - start);
    const auto separator = item.find('=');
    if (separator == std::string::npos)
    {
      result.error = Failure(ProviderErrorCode::InvalidResponse, "Authorization callback was malformed");
      return result;
    }
    std::string key, value;
    if (!PercentDecode(item.substr(0, separator), key) || !PercentDecode(item.substr(separator + 1), value) ||
        key.empty() || !fields.emplace(key, value).second)
    {
      result.error = Failure(ProviderErrorCode::InvalidResponse, "Authorization callback contained invalid fields");
      return result;
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }

  const auto state = fields.find("state");
  if (state == fields.end() || state->second.empty() || state->second.size() > 256)
  {
    result.error = Failure(ProviderErrorCode::InvalidOAuthState, "Authorization state was missing");
    return result;
  }
  const auto code = fields.find("code");
  const auto error = fields.find("error");
  if ((code == fields.end()) == (error == fields.end()))
  {
    const bool cancelledWithoutCode = fields.contains("canceled") && fields.at("canceled") == "true" &&
                                      code == fields.end() && error == fields.end();
    if (!cancelledWithoutCode)
    {
      result.error = Failure(ProviderErrorCode::InvalidResponse, "Authorization callback result was ambiguous");
      return result;
    }
  }
  if (code != fields.end() && (code->second.empty() || code->second.size() > 2048))
  {
    result.error = Failure(ProviderErrorCode::InvalidResponse, "Authorization code was invalid");
    return result;
  }

  OAuthCallbackValues values;
  values.state = state->second;
  if (code != fields.end()) values.code = code->second;
  if (error != fields.end()) values.error = error->second.substr(0, 256);
  if (const auto tone = fields.find("tone_id"); tone != fields.end() && !tone->second.empty() && tone->second.size() <= 256)
    values.toneId = tone->second;
  values.cancelled = fields.contains("canceled") && fields.at("canceled") == "true";
  result.value = std::move(values);
  return result;
}

} // namespace amphibia::providers::tone3000
