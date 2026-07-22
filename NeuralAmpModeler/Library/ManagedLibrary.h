#pragma once

#include "FolderScanner.h"
#include "LibraryIndex.h"
#include "ZipArchive.h"

#include <mutex>
#include <unordered_set>

namespace amphibia::library {

struct LibraryStatistics
{
  std::uint64_t managedBytes{};
  std::size_t namObjects{};
  std::size_t irObjects{};
  std::size_t packs{};
};

struct VerificationIssue
{
  ImportErrorCode code{ImportErrorCode::None};
  std::string objectHash;
  std::string message;
};

struct VerificationReport
{
  std::size_t checkedObjects{};
  std::size_t orphanedFiles{};
  std::vector<VerificationIssue> issues;
};

class ManagedLibrary
{
public:
  explicit ManagedLibrary(std::filesystem::path root);

  void Initialize();
  const std::filesystem::path& Root() const { return mRoot; }
  LibrarySnapshot Snapshot() const;
  LibraryStatistics Statistics() const;
  std::optional<std::filesystem::path> Resolve(const ContentHash& hash, ManagedObjectType expectedType) const;

  ImportSummary ImportFiles(const std::vector<std::filesystem::path>& files, const std::string& displayName,
                            LocalContentSource source = LocalContentSource::ManagedImport,
                            const std::vector<std::string>& relativePaths = {},
                            const CancelCheck& cancelled = {}, const ProgressCallback& progress = {});
  ImportSummary ImportFolderPlan(const ImportPackPlan& plan, const CancelCheck& cancelled = {},
                                 const ProgressCallback& progress = {});
  ImportSummary ImportZipPlan(const ImportPackPlan& plan, const CancelCheck& cancelled = {},
                              const ProgressCallback& progress = {});

  void MarkUsed(const ContentHash& hash);
  void RemovePack(const std::string& packKey);
  std::size_t RemoveUnused(const std::unordered_set<std::string>& protectedHashes = {});
  VerificationReport Verify(bool recomputeHashes, const CancelCheck& cancelled = {},
                            const ProgressCallback& progress = {});
  std::size_t ClearStaging();

  std::filesystem::path ObjectPath(const ContentHash& hash, ManagedObjectType type) const;

private:
  struct PromotedObject
  {
    LibraryObject object;
    bool alreadyPresent{};
  };

  PromotedObject Promote(const std::filesystem::path& source, const std::string& displayName,
                         LocalContentSource sourceKind, const CancelCheck& cancelled,
                         const ProgressCallback& progress);
  ImportSummary ImportPrepared(const std::vector<std::filesystem::path>& files,
                               const std::vector<std::string>& relativePaths, const std::string& displayName,
                               LocalContentSource source, const std::optional<ContentHash>& sourceHash,
                               const CancelCheck& cancelled, const ProgressCallback& progress);
  std::filesystem::path NewStagingDirectory();
  void LoadLocked();

  std::filesystem::path mRoot;
  LibraryIndex mIndex;
  mutable std::mutex mMutex;
  LibrarySnapshot mSnapshot;
};

std::filesystem::path DefaultLibraryPath();

} // namespace amphibia::library
