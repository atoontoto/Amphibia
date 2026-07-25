#include "CredentialRefreshLock.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#endif

namespace amphibia::credentials {

CredentialRefreshLock::CredentialRefreshLock(const CredentialKey& key, std::chrono::milliseconds timeout)
{
  const auto identity = key.serviceNamespace + "/" + key.account;
  const auto hash = std::to_string(std::hash<std::string>{}(identity));
#if defined(_WIN32)
  const auto name = std::wstring(L"Local\\Amphibia-TONE3000-refresh-") + std::wstring(hash.begin(), hash.end());
  mHandle = CreateMutexW(nullptr, FALSE, name.c_str());
  if (!mHandle) return;
  const auto wait = WaitForSingleObject(static_cast<HANDLE>(mHandle), static_cast<DWORD>((std::max)(timeout.count(), std::int64_t{0})));
  mLocked = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
#else
  const auto* base = getenv("HOME");
  if (!base || !*base) return;
  const std::string cache = std::string(base) + "/Library/Caches/Amphibia";
  const std::string locks = cache + "/locks";
  mkdir(cache.c_str(), 0700); mkdir(locks.c_str(), 0700);
  mDescriptor = open((locks + "/tone3000-refresh-" + hash + ".lock").c_str(), O_CREAT | O_RDWR | O_NOFOLLOW, 0600);
  if (mDescriptor < 0) return;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do
  {
    if (flock(mDescriptor, LOCK_EX | LOCK_NB) == 0) { mLocked = true; break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  } while (std::chrono::steady_clock::now() < deadline);
#endif
}

CredentialRefreshLock::~CredentialRefreshLock()
{
#if defined(_WIN32)
  if (mHandle) { if (mLocked) ReleaseMutex(static_cast<HANDLE>(mHandle)); CloseHandle(static_cast<HANDLE>(mHandle)); }
#else
  if (mDescriptor >= 0) { if (mLocked) flock(mDescriptor, LOCK_UN); close(mDescriptor); }
#endif
}

} // namespace amphibia::credentials
