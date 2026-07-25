#pragma once

#include "../Providers/Tone3000/Pkce.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace amphibia::credentials {

struct OAuthTokenSet
{
  std::string accessToken;
  std::optional<std::string> refreshToken;
  std::string tokenType;
  std::chrono::system_clock::time_point accessTokenExpiry;
  std::optional<std::string> scope;
  std::uint64_t rotationGeneration{};

  OAuthTokenSet() = default;
  OAuthTokenSet(const OAuthTokenSet&) = delete;
  OAuthTokenSet& operator=(const OAuthTokenSet&) = delete;
  OAuthTokenSet(OAuthTokenSet&&) noexcept = default;
  OAuthTokenSet& operator=(OAuthTokenSet&&) noexcept = default;
  ~OAuthTokenSet()
  {
    providers::tone3000::SecureClear(accessToken);
    if (refreshToken) providers::tone3000::SecureClear(*refreshToken);
  }
};

struct SecureCredential
{
  OAuthTokenSet tokens;
  std::optional<std::string> providerUserId;
  std::optional<std::string> username;
  std::int64_t verifiedAt{};

  SecureCredential() = default;
  SecureCredential(const SecureCredential&) = delete;
  SecureCredential& operator=(const SecureCredential&) = delete;
  SecureCredential(SecureCredential&&) noexcept = default;
  SecureCredential& operator=(SecureCredential&&) noexcept = default;
};

struct CredentialKey
{
  std::string serviceNamespace{"org.amphibiaaudio.Amphibia.TONE3000"};
  std::string account{"default"};
};

enum class CredentialError { None, NotFound, Unavailable, InvalidKey, InvalidValue, Corrupt, AccessDenied, PlatformError };

struct CredentialResult
{
  CredentialError error{CredentialError::None};
  std::string safeMessage;
  explicit operator bool() const noexcept { return error == CredentialError::None; }
};

} // namespace amphibia::credentials
