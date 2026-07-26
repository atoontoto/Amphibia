#pragma once

#include "../../Credentials/CredentialTypes.h"
#include "../ProviderTypes.h"

#include <chrono>
#include <cstddef>
#include <string_view>

namespace amphibia::providers::tone3000 {

struct JsonLimits
{
  std::size_t maximumBytes{4 * 1024 * 1024};
  std::size_t maximumDepth{32};
  std::size_t maximumNodes{20000};
  std::size_t maximumStringBytes{8192};
  std::size_t maximumTags{256};
  std::size_t maximumImages{64};
  std::size_t maximumModels{10000};
};

ProviderResult<credentials::OAuthTokenSet> ParseTokenResponse(
  std::string_view body, std::chrono::system_clock::time_point now,
  const JsonLimits& limits = JsonLimits{});
ProviderResult<ProviderUser> ParseCurrentUser(std::string_view body,
                                              const JsonLimits& limits = JsonLimits{});
ProviderResult<RemoteTone> ParseTone(std::string_view body,
                                     const JsonLimits& limits = JsonLimits{});
ProviderResult<RemoteModel> ParseModel(std::string_view body, RemoteContentFormat expectedFormat,
                                       const JsonLimits& limits = JsonLimits{});
ProviderResult<std::vector<RemoteModel>> ParseModelList(
  std::string_view body, RemoteContentFormat expectedFormat,
  const JsonLimits& limits = JsonLimits{});
ProviderResult<RemoteModelPage> ParseModelPage(
  std::string_view body, RemoteContentFormat expectedFormat,
  const JsonLimits& limits = JsonLimits{});

} // namespace amphibia::providers::tone3000
