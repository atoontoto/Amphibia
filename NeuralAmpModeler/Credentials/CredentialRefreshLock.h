#pragma once

#include "CredentialTypes.h"

#include <chrono>

namespace amphibia::credentials {

class CredentialRefreshLock final
{
public:
  CredentialRefreshLock(const CredentialKey& key, std::chrono::milliseconds timeout);
  ~CredentialRefreshLock();
  CredentialRefreshLock(const CredentialRefreshLock&) = delete;
  CredentialRefreshLock& operator=(const CredentialRefreshLock&) = delete;
  bool IsLocked() const noexcept { return mLocked; }

private:
  bool mLocked{};
#if defined(_WIN32)
  void* mHandle{};
#else
  int mDescriptor{-1};
#endif
};

} // namespace amphibia::credentials
