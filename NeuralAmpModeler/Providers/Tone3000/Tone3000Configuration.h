#pragma once

#include <string>

namespace amphibia::providers::tone3000 {

enum class Tone3000ConfigurationKind { Unconfigured, Development, Production, Test };

struct Tone3000Configuration
{
  std::string clientId;
  std::string authorizationEndpoint{"https://www.tone3000.com/api/v1/oauth/authorize"};
  std::string tokenEndpoint{"https://www.tone3000.com/api/v1/oauth/token"};
  std::string apiBaseUrl{"https://www.tone3000.com"};
  std::string redirectUri;
  std::string requestedScopes;
  std::string userAgent{"Amphibia/0.1.0"};
  std::string knownTonePrompt;
  std::string credentialServiceNamespace{"org.amphibiaaudio.Amphibia.TONE3000"};
  std::string credentialAccount{"oauth-v1"};
  Tone3000ConfigurationKind kind{Tone3000ConfigurationKind::Unconfigured};
  bool allowTestHttp{};

  bool HasPublishableConfiguration() const noexcept
  {
    return !clientId.empty() && !authorizationEndpoint.empty() && !tokenEndpoint.empty() &&
           !apiBaseUrl.empty() && !redirectUri.empty() && !credentialServiceNamespace.empty() &&
           !credentialAccount.empty();
  }

  bool IsProductionUsable() const noexcept
  {
    return kind == Tone3000ConfigurationKind::Production && HasPublishableConfiguration() &&
           knownTonePrompt.empty();
  }
};

inline Tone3000Configuration UnconfiguredTone3000()
{
  return {};
}

} // namespace amphibia::providers::tone3000
