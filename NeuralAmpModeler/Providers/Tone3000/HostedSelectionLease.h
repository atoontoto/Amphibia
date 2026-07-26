#pragma once

#include <string>

namespace amphibia::providers::tone3000 {

// A non-blocking, cross-process lease for the single browser/callback flow.
// Unlike a mutex, the lease may be released by an asynchronous callback thread.
class HostedSelectionLease final
{
public:
  HostedSelectionLease(const std::string& serviceNamespace, const std::string& account);
  ~HostedSelectionLease();
  HostedSelectionLease(const HostedSelectionLease&) = delete;
  HostedSelectionLease& operator=(const HostedSelectionLease&) = delete;
  bool IsHeld() const noexcept { return mHeld; }

private:
  bool mHeld{};
#if defined(_WIN32)
  void* mHandle{};
#else
  int mDescriptor{-1};
#endif
};

} // namespace amphibia::providers::tone3000
