#pragma once

#include "CredentialTypes.h"

namespace amphibia::credentials {

class ISecureCredentialStore
{
public:
  virtual ~ISecureCredentialStore() = default;
  virtual CredentialResult Store(const CredentialKey&, const SecureCredential&) = 0;
  virtual CredentialResult Load(const CredentialKey&, SecureCredential&) = 0;
  virtual CredentialResult Remove(const CredentialKey&) = 0;
  virtual bool IsAvailable() const noexcept = 0;
};

class PlatformSecureCredentialStore final : public ISecureCredentialStore
{
public:
  CredentialResult Store(const CredentialKey&, const SecureCredential&) override;
  CredentialResult Load(const CredentialKey&, SecureCredential&) override;
  CredentialResult Remove(const CredentialKey&) override;
  bool IsAvailable() const noexcept override;
};

} // namespace amphibia::credentials
