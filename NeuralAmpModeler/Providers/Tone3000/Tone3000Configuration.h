#pragma once

#include <optional>
#include <string>

namespace amphibia::providers::tone3000 {

enum class Tone3000ConfigurationKind { Unconfigured, Development, Production, Test };
enum class Tone3000RegistrationState { Unconfigured, DevelopmentConfigured, ProductionConfigured };
enum class Tone3000CallbackStrategy
{
  Unavailable,
  DevelopmentMock,
  FixedLoopback,
  RandomLoopback,
  CustomUri
};

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
  std::string approvedApplicationName;
  std::string privacyUrl;
  std::string repositoryUrl;
  std::string supportContact;
  Tone3000ConfigurationKind kind{Tone3000ConfigurationKind::Unconfigured};
  Tone3000CallbackStrategy callbackStrategy{Tone3000CallbackStrategy::Unavailable};
  bool approvedBrandAssets{};
  bool allowTestHttp{};

  bool HasPublishableConfiguration() const noexcept
  {
    return !clientId.empty() && !authorizationEndpoint.empty() && !tokenEndpoint.empty() &&
           !apiBaseUrl.empty() && !redirectUri.empty() && !credentialServiceNamespace.empty() &&
           !credentialAccount.empty();
  }

  bool IsProductionUsable() const noexcept;
};

struct Tone3000ConfigurationValidation
{
  Tone3000RegistrationState state{Tone3000RegistrationState::Unconfigured};
  bool valid{};
  bool providerUsable{};
  std::string userMessage;
};

Tone3000ConfigurationValidation ValidateTone3000Configuration(const Tone3000Configuration&) noexcept;
Tone3000Configuration ConfiguredTone3000();

inline Tone3000Configuration UnconfiguredTone3000()
{
  return {};
}

} // namespace amphibia::providers::tone3000
