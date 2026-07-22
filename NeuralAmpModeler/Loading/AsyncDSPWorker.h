#pragma once

#include "LoadTypes.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

namespace amphibia::loading
{

template <typename DSP, typename Metadata> struct PreparedDSP
{
  std::unique_ptr<DSP> processor;
  Metadata metadata{};
};

// One dedicated non-real-time worker prepares and reclaims one DSP stage.
// The audio thread sees only two one-element atomic mailboxes. It never owns a
// smart pointer and never deletes either a processor or an activation record.
template <typename DSP, typename Metadata> class AsyncDSPWorker
{
  static_assert(std::is_trivially_copyable_v<Metadata>, "audio activation metadata must be bounded POD");

public:
  using CancelCheck = std::function<bool()>;
  using ProgressCallback = std::function<void(LoadState, float)>;
  using PrepareFunction = std::function<PreparedDSP<DSP, Metadata>(const LoadRequest&, const CancelCheck&,
                                                                    const ProgressCallback&)>;

  struct ActivationInfo
  {
    bool activated{};
    bool cleared{};
    Metadata metadata{};
    std::uint64_t requestId{};
  };

  explicit AsyncDSPWorker(PrepareFunction prepare)
  : mPrepare(std::move(prepare))
  , mWorker([this] { WorkerMain(); })
  {
  }

  ~AsyncDSPWorker()
  {
    Shutdown();
    // The host must have stopped audio callbacks before destroying the plug-in.
    delete mActive.exchange(nullptr, std::memory_order_acq_rel);
  }

  AsyncDSPWorker(const AsyncDSPWorker&) = delete;
  AsyncDSPWorker& operator=(const AsyncDSPWorker&) = delete;

  std::uint64_t Submit(std::string path, const ProcessingConfiguration& configuration, double userValue = 0.0)
  {
    LoadRequest request;
    request.requestId = mRequestCounter.fetch_add(1, std::memory_order_relaxed) + 1;
    request.path = std::move(path);
    request.displayName = DisplayName(request.path);
    request.configuration = configuration;
    request.userValue = userValue;

    PublishNewRequest(request);
    return request.requestId;
  }

  std::uint64_t SubmitClear(const ProcessingConfiguration& configuration)
  {
    LoadRequest request;
    request.requestId = mRequestCounter.fetch_add(1, std::memory_order_relaxed) + 1;
    request.displayName = "None";
    request.configuration = configuration;
    request.clear = true;

    PublishNewRequest(request);
    return request.requestId;
  }

  std::uint64_t Reconfigure(const ProcessingConfiguration& configuration, double userValue = 0.0)
  {
    std::string path;
    bool clear = false;
    bool haveSelection = false;
    {
      std::lock_guard<std::mutex> lock(mMutex);
      const bool requestPending = mStatus.state == LoadState::Inspecting || mStatus.state == LoadState::Preparing
                                  || mStatus.state == LoadState::ReadyToActivate;
      if (requestPending)
      {
        path = mRequestedPath;
        clear = mRequestedClear;
        haveSelection = true;
      }
      else if (!mActivePath.empty())
      {
        path = mActivePath;
        haveSelection = true;
      }
    }

    mCompatible.store(mActive.load(std::memory_order_acquire) == nullptr, std::memory_order_release);
    mCurrentGeneration.store(configuration.generation, std::memory_order_release);
    if (!haveSelection)
      return 0;
    return clear ? SubmitClear(configuration) : Submit(std::move(path), configuration, userValue);
  }

  // Rebuild the newest selected processor without invalidating the compatible
  // active processor. Used for non-configuration changes such as Slim so a
  // failed refresh preserves the currently working sound.
  std::uint64_t Reprepare(const ProcessingConfiguration& configuration, double userValue = 0.0)
  {
    std::string path;
    bool clear = false;
    bool haveSelection = false;
    {
      std::lock_guard<std::mutex> lock(mMutex);
      const bool requestPending = mStatus.state == LoadState::Inspecting || mStatus.state == LoadState::Preparing
                                  || mStatus.state == LoadState::ReadyToActivate;
      if (requestPending)
      {
        path = mRequestedPath;
        clear = mRequestedClear;
        haveSelection = true;
      }
      else if (!mActivePath.empty())
      {
        path = mActivePath;
        haveSelection = true;
      }
    }

    if (!haveSelection)
      return 0;
    return clear ? SubmitClear(configuration) : Submit(std::move(path), configuration, userValue);
  }

  void Cancel()
  {
    const auto requestId = mRequestCounter.fetch_add(1, std::memory_order_relaxed) + 1;
    mCurrentRequest.store(requestId, std::memory_order_release);
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mPending.reset();
      mRequestedPath = mActivePath;
      mRequestedClear = false;
      mStatus = {requestId, LoadState::Cancelled, 0.0f, {}, "Load cancelled", LoadErrorCode::Cancelled};
    }
    mWake.notify_one();
  }

  // Audio-thread method. Exactly one pointer CAS, pointer assignment, and
  // publication occur on success. A full completion mailbox defers activation.
  ActivationInfo ActivateAtBlockBoundary() noexcept
  {
    ActivationInfo info;
    if (mCompleted.load(std::memory_order_acquire) != nullptr)
      return info;

    ActivationRecord* record = mReady.load(std::memory_order_acquire);
    if (record == nullptr)
      return info;
    if (record->request.requestId != mCurrentRequest.load(std::memory_order_acquire)
        || record->request.configuration.generation != mCurrentGeneration.load(std::memory_order_acquire))
      return info;
    if (!mReady.compare_exchange_strong(record, nullptr, std::memory_order_acq_rel, std::memory_order_acquire))
      return info;

    DSP* previous = mActive.exchange(record->prepared, std::memory_order_acq_rel);
    record->prepared = nullptr;
    record->retired = previous;
    if (previous != nullptr)
      mPeakRetiredDepth.store(1, std::memory_order_relaxed);
    mCompatible.store(true, std::memory_order_release);

    info.activated = true;
    info.cleared = record->request.clear;
    info.metadata = record->metadata;
    info.requestId = record->request.requestId;
    mCompleted.store(record, std::memory_order_release);
    return info;
  }

  DSP* ActiveForAudio() noexcept
  {
    return mCompatible.load(std::memory_order_acquire) ? mActive.load(std::memory_order_acquire) : nullptr;
  }
  const DSP* ActiveForAudio() const noexcept
  {
    return mCompatible.load(std::memory_order_acquire) ? mActive.load(std::memory_order_acquire) : nullptr;
  }

  bool HasRetainedProcessor() const noexcept { return mActive.load(std::memory_order_acquire) != nullptr; }
  bool IsCompatible() const noexcept { return mCompatible.load(std::memory_order_acquire); }

  LoadStatus Status() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mStatus;
  }

  std::string ActivePath() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mActivePath;
  }

  Metadata ActiveMetadata() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mActiveMetadata;
  }

  std::size_t PeakRetiredDepth() const noexcept { return mPeakRetiredDepth.load(std::memory_order_relaxed); }

  void Shutdown()
  {
    bool expected = false;
    if (!mStopping.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
      return;
    mCurrentRequest.fetch_add(1, std::memory_order_acq_rel);
    mWake.notify_one();
    if (mWorker.joinable())
      mWorker.join();
  }

private:
  struct ActivationRecord
  {
    LoadRequest request;
    DSP* prepared{};
    DSP* retired{};
    Metadata metadata{};
  };

  static_assert(std::atomic<ActivationRecord*>::is_always_lock_free,
                "the real-time activation mailboxes require lock-free pointer atomics");

  static std::string DisplayName(const std::string& path)
  {
    try
    {
      const auto name = std::filesystem::u8path(path).filename().u8string();
      return std::string(name.begin(), name.end());
    }
    catch (...)
    {
      return "selected file";
    }
  }

  void PublishNewRequest(const LoadRequest& request)
  {
    mCurrentGeneration.store(request.configuration.generation, std::memory_order_release);
    mCurrentRequest.store(request.requestId, std::memory_order_release);
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mPending = request; // bounded newest-only queue
      mRequestedPath = request.path;
      mRequestedClear = request.clear;
      mStatus = {request.requestId, LoadState::Inspecting, 0.05f, request.displayName,
                 request.clear ? "Clearing" : "Inspecting", std::nullopt};
    }
    mWake.notify_one();
  }

  bool IsCurrent(const LoadRequest& request) const noexcept
  {
    return !mStopping.load(std::memory_order_acquire)
           && request.requestId == mCurrentRequest.load(std::memory_order_acquire)
           && request.configuration.generation == mCurrentGeneration.load(std::memory_order_acquire);
  }

  void UpdateProgress(const LoadRequest& request, LoadState state, float progress)
  {
    if (!IsCurrent(request))
      return;
    std::lock_guard<std::mutex> lock(mMutex);
    if (!IsCurrent(request))
      return;
    mStatus = {request.requestId, state, progress, request.displayName,
               state == LoadState::Inspecting ? "Inspecting" : "Preparing", std::nullopt};
  }

  void Fail(const LoadRequest& request, LoadErrorCode code, const std::string& message)
  {
    if (!IsCurrent(request))
      return;
    std::lock_guard<std::mutex> lock(mMutex);
    if (!IsCurrent(request))
      return;
    mStatus = {request.requestId, LoadState::Failed, 0.0f, request.displayName, message, code};
  }

  void DrainCompleted()
  {
    ActivationRecord* record = mCompleted.exchange(nullptr, std::memory_order_acq_rel);
    if (record == nullptr)
      return;

    delete record->retired;
    record->retired = nullptr;
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mActivePath = record->request.clear ? std::string{} : record->request.path;
      mActiveMetadata = record->metadata;
      if (IsCurrent(record->request))
      {
        mStatus = {record->request.requestId, LoadState::Active, 1.0f, record->request.displayName,
                   record->request.clear ? "Cleared" : "Active", std::nullopt};
      }
    }
    delete record;
  }

  void DiscardStaleReady()
  {
    ActivationRecord* record = mReady.load(std::memory_order_acquire);
    if (record == nullptr || IsCurrent(record->request))
      return;
    if (mReady.compare_exchange_strong(record, nullptr, std::memory_order_acq_rel, std::memory_order_acquire))
    {
      delete record->prepared;
      delete record;
    }
  }

  void PublishPrepared(std::unique_ptr<ActivationRecord> record)
  {
    while (IsCurrent(record->request))
    {
      DrainCompleted();
      DiscardStaleReady();
      ActivationRecord* expected = nullptr;
      if (mReady.compare_exchange_strong(expected, record.get(), std::memory_order_release, std::memory_order_acquire))
      {
        {
          std::lock_guard<std::mutex> lock(mMutex);
          if (IsCurrent(record->request))
            mStatus = {record->request.requestId, LoadState::ReadyToActivate, 1.0f,
                       record->request.displayName, "Ready to activate", std::nullopt};
        }
        record.release();
        return;
      }
      std::unique_lock<std::mutex> lock(mMutex);
      mWake.wait_for(lock, std::chrono::milliseconds(2));
    }
  }

  void Prepare(const LoadRequest& request)
  {
    auto record = std::make_unique<ActivationRecord>();
    record->request = request;

    if (!request.clear)
    {
      auto cancel = [this, request] { return !IsCurrent(request); };
      auto progress = [this, request](LoadState state, float value) { UpdateProgress(request, state, value); };
      auto prepared = mPrepare(request, cancel, progress);
      if (!IsCurrent(request))
        return;
      record->prepared = prepared.processor.release();
      record->metadata = prepared.metadata;
    }
    PublishPrepared(std::move(record));
  }

  void WorkerMain()
  {
    while (!mStopping.load(std::memory_order_acquire))
    {
      DrainCompleted();
      DiscardStaleReady();

      std::optional<LoadRequest> request;
      {
        std::unique_lock<std::mutex> lock(mMutex);
        if (!mPending)
          mWake.wait_for(lock, std::chrono::milliseconds(2));
        if (mPending)
        {
          request = std::move(mPending);
          mPending.reset();
        }
      }
      if (!request || !IsCurrent(*request))
        continue;

      try
      {
        Prepare(*request);
      }
      catch (const LoadException& error)
      {
        Fail(*request, error.Code(), error.what());
      }
      catch (const std::bad_alloc&)
      {
        Fail(*request, LoadErrorCode::OutOfMemory, "Not enough memory to prepare the file");
      }
      catch (const std::exception&)
      {
        Fail(*request, LoadErrorCode::PreparationFailed, "The file could not be prepared");
      }
      catch (...)
      {
        Fail(*request, LoadErrorCode::InternalError, "An internal loading error occurred");
      }
    }

    if (ActivationRecord* record = mReady.exchange(nullptr, std::memory_order_acq_rel))
    {
      delete record->prepared;
      delete record;
    }
    DrainCompleted();
    std::lock_guard<std::mutex> lock(mMutex);
    mPending.reset();
  }

  PrepareFunction mPrepare;
  mutable std::mutex mMutex;
  std::condition_variable mWake;
  std::optional<LoadRequest> mPending;
  LoadStatus mStatus;
  std::string mRequestedPath;
  std::string mActivePath;
  bool mRequestedClear{};
  Metadata mActiveMetadata{};

  std::atomic<bool> mStopping{false};
  std::atomic<std::uint64_t> mRequestCounter{0};
  std::atomic<std::uint64_t> mCurrentRequest{0};
  std::atomic<std::uint64_t> mCurrentGeneration{0};
  std::atomic<bool> mCompatible{true};
  std::atomic<ActivationRecord*> mReady{nullptr};
  std::atomic<ActivationRecord*> mCompleted{nullptr};
  std::atomic<std::size_t> mPeakRetiredDepth{0};
  std::atomic<DSP*> mActive{nullptr}; // ownership transfers only at an audio-block boundary
  std::thread mWorker;
};

} // namespace amphibia::loading
