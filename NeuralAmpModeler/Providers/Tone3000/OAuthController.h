#pragma once

#include "OAuthCallback.h"
#include "Pkce.h"
#include "Tone3000Configuration.h"

#include <chrono>
#include <mutex>
#include <optional>

namespace amphibia::providers::tone3000 {

struct PendingAuthorization
{
  std::uint64_t flowId{};
  AuthorizationRequest request;
  std::string state;
  std::string verifier;
  std::chrono::steady_clock::time_point deadline;

  PendingAuthorization() = default;
  PendingAuthorization(const PendingAuthorization&) = delete;
  PendingAuthorization& operator=(const PendingAuthorization&) = delete;
  PendingAuthorization(PendingAuthorization&&) = default;
  PendingAuthorization& operator=(PendingAuthorization&&) = default;
  ~PendingAuthorization() { SecureClear(verifier); SecureClear(state); }
};

struct ConsumedAuthorization
{
  std::uint64_t flowId{};
  AuthorizationRequest request;
  std::string authorizationCode;
  std::string verifier;
  std::optional<std::string> selectedToneId;

  ConsumedAuthorization() = default;
  ConsumedAuthorization(const ConsumedAuthorization&) = delete;
  ConsumedAuthorization& operator=(const ConsumedAuthorization&) = delete;
  ConsumedAuthorization(ConsumedAuthorization&&) = default;
  ConsumedAuthorization& operator=(ConsumedAuthorization&&) = default;
  ~ConsumedAuthorization() { SecureClear(authorizationCode); SecureClear(verifier); }
};

struct AuthorizationStart
{
  std::uint64_t flowId{};
  std::string authorizationUrl;
};

class OAuthController
{
public:
  explicit OAuthController(Tone3000Configuration configuration,
                           std::chrono::seconds timeout = std::chrono::minutes(5));
  ~OAuthController();

  ProviderResult<AuthorizationStart> Begin(const AuthorizationRequest& request);
  ProviderResult<ConsumedAuthorization> HandleCallback(const std::string& callbackUri);
  void Cancel();
  void Shutdown();
  bool IsPending() const;

private:
  ProviderError ValidateConfigurationAndRequest(const AuthorizationRequest&) const;
  std::string BuildUrl(const AuthorizationRequest&, const PkcePair&, const std::string& state) const;

  Tone3000Configuration mConfiguration;
  std::chrono::seconds mTimeout;
  mutable std::mutex mMutex;
  std::optional<PendingAuthorization> mPending;
  std::uint64_t mNextFlowId{};
  bool mShutdown{};
};

} // namespace amphibia::providers::tone3000
