#pragma once

#include "HttpPolicy.h"

#include <deque>
#include <mutex>

namespace amphibia::network {

std::optional<std::chrono::milliseconds> RetryDelay(
  const HttpRequest& request, int completedAttempts, const std::optional<HttpResponse>& response,
  const std::optional<HttpFailure>& failure, std::chrono::system_clock::time_point wallNow,
  std::uint32_t jitterValue) noexcept;

class SlidingWindowRateLimiter
{
public:
  explicit SlidingWindowRateLimiter(std::size_t requestsPerMinute = 100,
                                    std::size_t maximumQueued = 32);
  std::optional<std::chrono::steady_clock::time_point> Reserve(
    std::chrono::steady_clock::time_point now);
  void ApplyServerDelay(std::chrono::steady_clock::time_point until);
  std::size_t Outstanding() const;

private:
  mutable std::mutex mMutex;
  std::deque<std::chrono::steady_clock::time_point> mReservations;
  std::chrono::steady_clock::time_point mServerBlockedUntil{};
  std::size_t mLimit;
  std::size_t mMaximumQueued;
};

} // namespace amphibia::network
