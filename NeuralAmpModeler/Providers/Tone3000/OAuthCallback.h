#pragma once

#include "../ProviderTypes.h"

#include <optional>
#include <string>

namespace amphibia::providers::tone3000 {

struct OAuthCallbackValues
{
  std::string state;
  std::optional<std::string> code;
  std::optional<std::string> error;
  std::optional<std::string> toneId;
  bool cancelled{};
};

ProviderResult<OAuthCallbackValues> ParseOAuthCallback(const std::string& callbackUri,
                                                       const std::string& exactRedirectUri);

} // namespace amphibia::providers::tone3000
