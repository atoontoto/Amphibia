#pragma once

#include "ManagedLibrary.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace amphibia::library {

class ImportWorker
{
public:
  explicit ImportWorker(ManagedLibrary& library);
  ~ImportWorker();

  std::uint64_t Scan(std::vector<std::filesystem::path> sources);
  std::uint64_t ImportSelected(const std::vector<std::size_t>& selectedIndices);
  std::uint64_t VerifyLibrary();
  std::uint64_t ClearStaging();
  std::uint64_t RemoveUnused(std::vector<std::string> protectedHashes = {});
  void Cancel();
  void Shutdown();

  ImportProgress Progress() const;
  std::optional<ImportPackPlan> Plan() const;
  std::optional<ImportSummary> Summary() const;
  std::optional<VerificationReport> Verification() const;

private:
  enum class WorkKind { Scan, Import, Verify, ClearStaging, RemoveUnused };
  struct Work
  {
    WorkKind kind{WorkKind::Scan};
    std::uint64_t taskId{};
    std::vector<std::filesystem::path> sources;
    std::vector<std::size_t> selectedIndices;
  };

  void Run();
  void RunScan(const Work& work);
  void RunImport(const Work& work);
  void RunMaintenance(const Work& work);
  bool Cancelled(std::uint64_t taskId) const;
  void Publish(ImportProgress progress);

  ManagedLibrary& mLibrary;
  mutable std::mutex mMutex;
  std::condition_variable mCondition;
  std::thread mThread;
  bool mStopping{};
  std::uint64_t mNextTaskId{1};
  std::uint64_t mCurrentTaskId{};
  std::optional<Work> mPending;
  ImportProgress mProgress;
  std::optional<ImportPackPlan> mPlan;
  std::vector<std::filesystem::path> mScannedFiles;
  std::optional<ImportSummary> mSummary;
  std::optional<VerificationReport> mVerification;
};

} // namespace amphibia::library
