#include "HostedSelectionLease.h"

#include <cstdlib>
#include <functional>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace amphibia::providers::tone3000 {

HostedSelectionLease::HostedSelectionLease(const std::string& serviceNamespace, const std::string& account)
{
  const auto identity = serviceNamespace + "/" + account;
  const auto hash = std::to_string(std::hash<std::string>{}(identity));
#if defined(_WIN32)
  const auto name = std::wstring(L"Local\\Amphibia-TONE3000-selection-") + std::wstring(hash.begin(), hash.end());
  mHandle = CreateSemaphoreW(nullptr, 1, 1, name.c_str());
  if (mHandle) mHeld = WaitForSingleObject(static_cast<HANDLE>(mHandle), 0) == WAIT_OBJECT_0;
#else
  const auto* base = getenv("HOME");
  if (!base || !*base) return;
  const std::string cache = std::string(base) + "/Library/Caches/Amphibia";
  const std::string locks = cache + "/locks";
  mkdir(cache.c_str(), 0700);
  mkdir(locks.c_str(), 0700);
  mDescriptor = open((locks + "/tone3000-selection-" + hash + ".lock").c_str(),
                     O_CREAT | O_RDWR | O_NOFOLLOW, 0600);
  if (mDescriptor >= 0) mHeld = flock(mDescriptor, LOCK_EX | LOCK_NB) == 0;
#endif
}

HostedSelectionLease::~HostedSelectionLease()
{
#if defined(_WIN32)
  if (mHandle)
  {
    if (mHeld) ReleaseSemaphore(static_cast<HANDLE>(mHandle), 1, nullptr);
    CloseHandle(static_cast<HANDLE>(mHandle));
  }
#else
  if (mDescriptor >= 0)
  {
    if (mHeld) flock(mDescriptor, LOCK_UN);
    close(mDescriptor);
  }
#endif
}

} // namespace amphibia::providers::tone3000
