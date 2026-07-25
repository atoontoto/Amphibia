#pragma once

#include "../Providers/ProviderTypes.h"

#include <string>

namespace amphibia::platform {

providers::ProviderError ValidateExternalAuthorizationUrl(const std::string& url);

class ISystemBrowser
{
public:
  virtual ~ISystemBrowser() = default;
  virtual providers::ProviderError OpenExplicit(const std::string& validatedUrl) = 0;
};

class PlatformSystemBrowser final : public ISystemBrowser
{
public:
  providers::ProviderError OpenExplicit(const std::string& validatedUrl) override;
};

} // namespace amphibia::platform
