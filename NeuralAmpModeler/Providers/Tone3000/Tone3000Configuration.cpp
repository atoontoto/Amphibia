#include "Tone3000Configuration.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string_view>

namespace amphibia::providers::tone3000 {
namespace {

bool BoundedPlain(const std::string& value, std::size_t maximum)
{
  return !value.empty() && value.size() <= maximum &&
         std::none_of(value.begin(), value.end(), [](unsigned char c) { return c < 0x20 || c == 0x7f; });
}

bool Https(const std::string& value)
{
  return value.starts_with("https://") && value.size() <= 8192 &&
         value.find_first_of("\r\n#") == std::string::npos;
}

bool ExactLoopback(const std::string& value)
{
  constexpr std::string_view prefix = "http://127.0.0.1:";
  if (!value.starts_with(prefix) || value.size() > 512 || value.find_first_of("\r\n?#") != std::string::npos)
    return false;
  const auto portEnd = value.find('/', prefix.size());
  if (portEnd == std::string::npos || portEnd == prefix.size()) return false;
  return std::all_of(value.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
                     value.begin() + static_cast<std::ptrdiff_t>(portEnd),
                     [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool CustomUri(const std::string& value)
{
  const auto colon = value.find(':');
  return colon > 1 && colon < 64 && value.size() <= 512 &&
         !value.starts_with("http:") && !value.starts_with("https:") &&
         value.find_first_of("\r\n?#") == std::string::npos;
}

Tone3000ConfigurationValidation Invalid(Tone3000RegistrationState state, const char* message)
{
  return {state, false, false, message};
}

} // namespace

Tone3000ConfigurationValidation ValidateTone3000Configuration(const Tone3000Configuration& configuration) noexcept
{
  if (configuration.kind == Tone3000ConfigurationKind::Unconfigured)
    return {Tone3000RegistrationState::Unconfigured, true, false,
            "TONE3000 integration is not configured in this build."};

  const auto state = configuration.kind == Tone3000ConfigurationKind::Production ?
    Tone3000RegistrationState::ProductionConfigured : Tone3000RegistrationState::DevelopmentConfigured;
  if (!configuration.HasPublishableConfiguration())
    return Invalid(state, "TONE3000 registration fields are incomplete.");
  if (!BoundedPlain(configuration.clientId, 256) || configuration.clientId.find("t3k_cs") != std::string::npos)
    return Invalid(state, "TONE3000 client configuration is invalid.");
  if (!Https(configuration.authorizationEndpoint) || !Https(configuration.tokenEndpoint) ||
      !Https(configuration.apiBaseUrl))
    return Invalid(state, "TONE3000 endpoints must use valid HTTPS URLs.");
  if (configuration.callbackStrategy == Tone3000CallbackStrategy::Unavailable)
    return Invalid(state, "No approved TONE3000 desktop callback strategy is configured.");

  const bool loopback = configuration.callbackStrategy == Tone3000CallbackStrategy::FixedLoopback ||
                        configuration.callbackStrategy == Tone3000CallbackStrategy::RandomLoopback ||
                        configuration.callbackStrategy == Tone3000CallbackStrategy::DevelopmentMock;
  if ((loopback && !ExactLoopback(configuration.redirectUri)) ||
      (configuration.callbackStrategy == Tone3000CallbackStrategy::CustomUri && !CustomUri(configuration.redirectUri)))
    return Invalid(state, "The registered TONE3000 redirect URI is invalid for its callback strategy.");

  if (configuration.kind == Tone3000ConfigurationKind::Production)
  {
    if (configuration.callbackStrategy == Tone3000CallbackStrategy::DevelopmentMock ||
        configuration.allowTestHttp)
      return Invalid(state, "Development callback options are forbidden in production.");
    if (!configuration.clientId.starts_with("t3k_pub"))
      return Invalid(state, "Production requires a publishable TONE3000 client ID.");
    if (!BoundedPlain(configuration.approvedApplicationName, 256) || !configuration.approvedBrandAssets ||
        !Https(configuration.privacyUrl) || !Https(configuration.repositoryUrl) ||
        !BoundedPlain(configuration.supportContact, 512))
      return Invalid(state, "Production TONE3000 branding and public support fields are incomplete.");
  }
  return {state, true, true,
          state == Tone3000RegistrationState::ProductionConfigured ?
            "TONE3000 production registration is configured." :
            "TONE3000 development registration is configured."};
}

bool Tone3000Configuration::IsProductionUsable() const noexcept
{
  return kind == Tone3000ConfigurationKind::Production &&
         ValidateTone3000Configuration(*this).providerUsable && knownTonePrompt.empty();
}

Tone3000Configuration ConfiguredTone3000()
{
  // Registration values are intentionally supplied only by an owner-reviewed
  // build configuration. The repository default contains no client identity,
  // redirect, callback claim, support contact, or brand-asset approval.
  return UnconfiguredTone3000();
}

} // namespace amphibia::providers::tone3000
