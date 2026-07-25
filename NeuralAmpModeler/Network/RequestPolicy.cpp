#include "RequestPolicy.h"

#include <algorithm>
#include <limits>

namespace amphibia::network {

std::optional<std::chrono::milliseconds> RetryDelay(
  const HttpRequest& request, int completedAttempts, const std::optional<HttpResponse>& response,
  const std::optional<HttpFailure>& failure, std::chrono::system_clock::time_point wallNow,
  std::uint32_t jitterValue) noexcept
{
  if (!request.idempotent || completedAttempts <= 0 || completedAttempts >= request.retry.maximumAttempts)
    return std::nullopt;
  if (response && !IsRetryableStatus(response->statusCode)) return std::nullopt;
  if (failure)
  {
    const auto code = failure->code;
    if (code != providers::ProviderErrorCode::Offline && code != providers::ProviderErrorCode::DnsFailure &&
        code != providers::ProviderErrorCode::ConnectTimeout && code != providers::ProviderErrorCode::RequestTimeout)
      return std::nullopt;
  }
  if (!response && !failure) return std::nullopt;
  if (response && response->statusCode == 429)
  {
    for (const auto& header : response->headers)
      if (header.name == "Retry-After" || header.name == "retry-after")
        if (const auto delay = ParseRetryAfter(header.value, wallNow))
          return std::chrono::duration_cast<std::chrono::milliseconds>(*delay);
  }
  const auto shift = (std::min)(completedAttempts - 1, 20);
  const auto multiplier = std::uint64_t{1} << shift;
  const auto base = static_cast<std::uint64_t>((std::max)(request.retry.baseDelay.count(), std::int64_t{0}));
  const auto maximum = static_cast<std::uint64_t>((std::max)(request.retry.maximumDelay.count(), std::int64_t{0}));
  const auto capped = (std::min)(maximum, base > 0 && multiplier > maximum / base ? maximum : base * multiplier);
  const auto jitterRange = capped / 4 + 1;
  return std::chrono::milliseconds((std::min)(maximum, capped + jitterValue % jitterRange));
}

SlidingWindowRateLimiter::SlidingWindowRateLimiter(std::size_t requestsPerMinute, std::size_t maximumQueued)
  : mLimit((std::max)(std::size_t{1}, requestsPerMinute)),
    mMaximumQueued((std::max)(std::size_t{1}, maximumQueued))
{
}

std::optional<std::chrono::steady_clock::time_point> SlidingWindowRateLimiter::Reserve(
  std::chrono::steady_clock::time_point now)
{
  std::lock_guard lock(mMutex);
  const auto cutoff = now - std::chrono::minutes(1);
  while (!mReservations.empty() && mReservations.front() <= cutoff) mReservations.pop_front();
  if (mReservations.size() >= mLimit + mMaximumQueued) return std::nullopt;
  auto at = (std::max)(now, mServerBlockedUntil);
  if (mReservations.size() >= mLimit)
    at = (std::max)(at, mReservations[mReservations.size() - mLimit] + std::chrono::minutes(1));
  mReservations.push_back(at);
  return at;
}

void SlidingWindowRateLimiter::ApplyServerDelay(std::chrono::steady_clock::time_point until)
{
  std::lock_guard lock(mMutex);
  mServerBlockedUntil = (std::max)(mServerBlockedUntil, until);
}

std::size_t SlidingWindowRateLimiter::Outstanding() const
{
  std::lock_guard lock(mMutex);
  return mReservations.size();
}

} // namespace amphibia::network
