#include "ImportWorker.h"

#include "PathSafety.h"

#include <algorithm>
#include <cctype>

namespace amphibia::library {
namespace {

std::string LowerExtension(const std::filesystem::path& path)
{
  auto value = PathToUtf8(path.extension());
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

} // namespace

ImportWorker::ImportWorker(ManagedLibrary& library) : mLibrary(library), mThread([this] { Run(); }) {}
ImportWorker::~ImportWorker() { Shutdown(); }

std::uint64_t ImportWorker::Scan(std::vector<std::filesystem::path> sources)
{
  std::lock_guard<std::mutex> guard(mMutex);
  const auto taskId = mNextTaskId++;
  mCurrentTaskId = taskId;
  mPending = Work{WorkKind::Scan, taskId, std::move(sources), {}};
  mPlan.reset();
  mSummary.reset();
  mProgress = {taskId, ImportTaskState::Scanning};
  mCondition.notify_one();
  return taskId;
}

std::uint64_t ImportWorker::ImportSelected(const std::vector<std::size_t>& selectedIndices)
{
  std::lock_guard<std::mutex> guard(mMutex);
  if (!mPlan) throw ImportException(ImportErrorCode::InternalError, "No completed import plan is available");
  const auto taskId = mNextTaskId++;
  mCurrentTaskId = taskId;
  mPending = Work{WorkKind::Import, taskId, {}, selectedIndices};
  mSummary.reset();
  mProgress = {taskId, ImportTaskState::Extracting, 0, mPlan->declaredUncompressedBytes, 0,
               selectedIndices.size()};
  mCondition.notify_one();
  return taskId;
}

std::uint64_t ImportWorker::VerifyLibrary()
{
  std::lock_guard<std::mutex> guard(mMutex);
  const auto taskId = mNextTaskId++;
  mCurrentTaskId = taskId;
  mPending = Work{WorkKind::Verify, taskId, {}, {}};
  mSummary.reset();
  mVerification.reset();
  mProgress = {taskId, ImportTaskState::Validating, 0, 0, 0, 0, {}, "Verifying managed library"};
  mCondition.notify_one();
  return taskId;
}

std::uint64_t ImportWorker::ClearStaging()
{
  std::lock_guard<std::mutex> guard(mMutex);
  const auto taskId = mNextTaskId++;
  mCurrentTaskId = taskId;
  mPending = Work{WorkKind::ClearStaging, taskId, {}, {}};
  mSummary.reset();
  mProgress = {taskId, ImportTaskState::Committing, 0, 0, 0, 0, {}, "Clearing safe staging remnants"};
  mCondition.notify_one();
  return taskId;
}

std::uint64_t ImportWorker::RemoveUnused(std::vector<std::string> protectedHashes)
{
  std::lock_guard<std::mutex> guard(mMutex);
  const auto taskId = mNextTaskId++;
  mCurrentTaskId = taskId;
  mPending = Work{WorkKind::RemoveUnused, taskId, {}, {}};
  mSummary.reset();
  mPending->sources.reserve(protectedHashes.size());
  for (auto& hash : protectedHashes) mPending->sources.emplace_back(std::move(hash));
  mProgress = {taskId, ImportTaskState::Committing, 0, 0, 0, 0, {}, "Removing unassociated managed objects"};
  mCondition.notify_one();
  return taskId;
}

void ImportWorker::Cancel()
{
  std::lock_guard<std::mutex> guard(mMutex);
  mCurrentTaskId = mNextTaskId++;
  mPending.reset();
  mProgress.state = ImportTaskState::Cancelled;
  mProgress.message = "Cancelled by user";
}

void ImportWorker::Shutdown()
{
  {
    std::lock_guard<std::mutex> guard(mMutex);
    if (mStopping) return;
    mStopping = true;
    mCurrentTaskId = mNextTaskId++;
    mPending.reset();
  }
  mCondition.notify_one();
  if (mThread.joinable()) mThread.join();
}

ImportProgress ImportWorker::Progress() const
{
  std::lock_guard<std::mutex> guard(mMutex);
  return mProgress;
}

std::optional<ImportPackPlan> ImportWorker::Plan() const
{
  std::lock_guard<std::mutex> guard(mMutex);
  return mPlan;
}

std::optional<ImportSummary> ImportWorker::Summary() const
{
  std::lock_guard<std::mutex> guard(mMutex);
  return mSummary;
}

std::optional<VerificationReport> ImportWorker::Verification() const
{
  std::lock_guard<std::mutex> guard(mMutex);
  return mVerification;
}

bool ImportWorker::Cancelled(std::uint64_t taskId) const
{
  std::lock_guard<std::mutex> guard(mMutex);
  return mStopping || taskId != mCurrentTaskId;
}

void ImportWorker::Publish(ImportProgress progress)
{
  std::lock_guard<std::mutex> guard(mMutex);
  if (progress.taskId == mCurrentTaskId) mProgress = std::move(progress);
}

void ImportWorker::Run()
{
  for (;;)
  {
    std::optional<Work> work;
    {
      std::unique_lock<std::mutex> lock(mMutex);
      mCondition.wait(lock, [&] { return mStopping || mPending.has_value(); });
      if (mStopping) return;
      work = std::move(mPending);
      mPending.reset();
    }
    try
    {
      if (work->kind == WorkKind::Scan) RunScan(*work);
      else if (work->kind == WorkKind::Import) RunImport(*work);
      else RunMaintenance(*work);
    }
    catch (const ImportException& exception)
    {
      Publish({work->taskId, exception.Code() == ImportErrorCode::Cancelled ? ImportTaskState::Cancelled
                                                                           : ImportTaskState::Failed,
               0, 0, 0, 0, {}, exception.what()});
    }
    catch (const std::exception& exception)
    {
      Publish({work->taskId, ImportTaskState::Failed, 0, 0, 0, 0, {}, exception.what()});
    }
  }
}

void ImportWorker::RunMaintenance(const Work& work)
{
  const auto cancelled = [this, id = work.taskId] { return Cancelled(id); };
  const auto update = [this, id = work.taskId](const ImportProgress& value) {
    auto progress = value;
    progress.taskId = id;
    Publish(std::move(progress));
  };
  std::string message;
  if (work.kind == WorkKind::Verify)
  {
    auto report = mLibrary.Verify(true, cancelled, update);
    message = "Verified " + std::to_string(report.checkedObjects) + " object(s); " +
              std::to_string(report.issues.size()) + " issue(s), " +
              std::to_string(report.orphanedFiles) + " orphan(s)";
    std::lock_guard<std::mutex> guard(mMutex);
    if (work.taskId != mCurrentTaskId) return;
    mVerification = std::move(report);
  }
  else if (work.kind == WorkKind::ClearStaging)
    message = "Cleared " + std::to_string(mLibrary.ClearStaging()) + " staging item(s)";
  else
  {
    std::unordered_set<std::string> protectedHashes;
    for (const auto& hash : work.sources) protectedHashes.insert(PathToUtf8(hash));
    message = "Removed " + std::to_string(mLibrary.RemoveUnused(protectedHashes)) + " unused object(s)";
  }
  Publish({work.taskId, ImportTaskState::Completed, 0, 0, 0, 0, {}, std::move(message)});
}

void ImportWorker::RunScan(const Work& work)
{
  if (work.sources.empty()) throw ImportException(ImportErrorCode::UnsupportedSource, "No import source was selected");
  const auto cancelled = [this, id = work.taskId] { return Cancelled(id); };
  const auto update = [this, id = work.taskId](const ImportProgress& value) {
    auto progress = value;
    progress.taskId = id;
    Publish(std::move(progress));
  };

  ImportPackPlan plan;
  std::vector<std::filesystem::path> scannedFiles;
  if (work.sources.size() == 1 && std::filesystem::is_directory(work.sources.front()))
    plan = ScanFolder(work.sources.front(), {}, cancelled, update);
  else if (work.sources.size() == 1 && LowerExtension(work.sources.front()) == ".zip")
    plan = ScanZipArchive(work.sources.front(), {}, cancelled, update);
  else
  {
    plan.displayName = work.sources.size() == 1 ? PathToUtf8(work.sources.front().stem()) : "Imported files";
    for (std::size_t index = 0; index < work.sources.size(); ++index)
    {
      if (cancelled()) throw ImportException(ImportErrorCode::Cancelled, "File scan cancelled");
      const auto& path = work.sources[index];
      ArchiveEntryDescriptor entry;
      entry.archiveIndex = index;
      entry.originalPath = PathToUtf8(path.filename());
      entry.normalizedRelativePath = entry.originalPath;
      entry.displayName = PathToUtf8(path.filename());
      entry.fileType = ClassifyImportPath(entry.originalPath);
      std::error_code error;
      entry.uncompressedSize = std::filesystem::file_size(path, error);
      entry.compressedSize = entry.uncompressedSize;
      if (error || !std::filesystem::is_regular_file(path))
      {
        entry.status = ImportEntryStatus::Invalid;
        entry.diagnostic = "File is missing or unreadable";
      }
      else if (entry.fileType != ImportedFileType::NamModel &&
               entry.fileType != ImportedFileType::WaveImpulseResponse)
      {
        entry.status = ImportEntryStatus::Unsupported;
        ++plan.unsupportedEntries;
      }
      else
      {
        entry.status = ImportEntryStatus::Candidate;
        entry.selected = true;
      }
      plan.declaredCompressedBytes += entry.compressedSize;
      plan.declaredUncompressedBytes += entry.uncompressedSize;
      plan.entries.push_back(std::move(entry));
      scannedFiles.push_back(path);
      update({work.taskId, ImportTaskState::Scanning, plan.declaredUncompressedBytes,
              plan.declaredUncompressedBytes, plan.entries.size(), work.sources.size(),
              plan.entries.back().displayName, std::nullopt});
    }
  }

  {
    std::lock_guard<std::mutex> guard(mMutex);
    if (work.taskId != mCurrentTaskId) return;
    mPlan = std::move(plan);
    mScannedFiles = std::move(scannedFiles);
    mProgress = {work.taskId, ImportTaskState::AwaitingSelection, 0, mPlan->declaredUncompressedBytes, 0,
                 mPlan->entries.size(), {}, "Review and select valid entries"};
  }
}

void ImportWorker::RunImport(const Work& work)
{
  ImportPackPlan plan;
  std::vector<std::filesystem::path> files;
  {
    std::lock_guard<std::mutex> guard(mMutex);
    if (!mPlan || work.taskId != mCurrentTaskId) return;
    plan = *mPlan;
    files = mScannedFiles;
  }
  for (auto& entry : plan.entries) entry.selected = false;
  for (const auto index : work.selectedIndices)
    if (index < plan.entries.size() && plan.entries[index].status == ImportEntryStatus::Candidate)
      plan.entries[index].selected = true;

  const auto cancelled = [this, id = work.taskId] { return Cancelled(id); };
  const auto update = [this, id = work.taskId](const ImportProgress& value) {
    auto progress = value;
    progress.taskId = id;
    Publish(std::move(progress));
  };
  ImportSummary summary;
  if (plan.sourcePath && std::filesystem::is_directory(*plan.sourcePath))
    summary = mLibrary.ImportFolderPlan(plan, cancelled, update);
  else if (plan.sourcePath && LowerExtension(*plan.sourcePath) == ".zip")
    summary = mLibrary.ImportZipPlan(plan, cancelled, update);
  else
  {
    std::vector<std::filesystem::path> chosenFiles;
    std::vector<std::string> relative;
    for (const auto index : work.selectedIndices)
      if (index < files.size() && index < plan.entries.size() && plan.entries[index].status == ImportEntryStatus::Candidate)
      {
        chosenFiles.push_back(files[index]);
        relative.push_back(plan.entries[index].normalizedRelativePath);
      }
    summary = mLibrary.ImportFiles(chosenFiles, plan.displayName, LocalContentSource::ManagedImport, relative,
                                   cancelled, update);
  }
  {
    std::lock_guard<std::mutex> guard(mMutex);
    if (work.taskId != mCurrentTaskId) return;
    mSummary = summary;
    mProgress = {work.taskId, ImportTaskState::Completed, 0, 0, summary.objects.size(), summary.objects.size(), {},
                 "Import completed"};
  }
}

} // namespace amphibia::library
