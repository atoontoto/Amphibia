#include "ManagedLibrary.h"

#include "ContentHash.h"
#include "PathSafety.h"
#include "../Loading/FileInspection.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <unordered_set>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <stdlib.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace amphibia::library {
namespace {

bool IsLinkOrReparsePoint(const std::filesystem::path& path)
{
  std::error_code error;
  if (std::filesystem::is_symlink(std::filesystem::symlink_status(path, error))) return true;
#if defined(_WIN32)
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
  return false;
#endif
}

void RemoveManagedTree(const std::filesystem::path& path, std::error_code& error)
{
  if (IsLinkOrReparsePoint(path)) std::filesystem::remove(path, error);
  else std::filesystem::remove_all(path, error);
}

class SecureCopyOutput
{
public:
  explicit SecureCopyOutput(const std::filesystem::path& path)
  {
#if defined(_WIN32)
    mHandle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                          FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (mHandle == INVALID_HANDLE_VALUE)
      throw ImportException(ImportErrorCode::CopyFailure, "Unique managed staging file could not be created");
#else
    mHandle = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (mHandle < 0)
      throw ImportException(ImportErrorCode::CopyFailure, "Unique managed staging file could not be created");
#endif
  }

  ~SecureCopyOutput()
  {
#if defined(_WIN32)
    if (mHandle != INVALID_HANDLE_VALUE) CloseHandle(mHandle);
#else
    if (mHandle >= 0) close(mHandle);
#endif
  }

  void Write(const void* data, std::size_t size)
  {
#if defined(_WIN32)
    DWORD written = 0;
    if (size > MAXDWORD || !WriteFile(mHandle, data, static_cast<DWORD>(size), &written, nullptr) || written != size)
      throw ImportException(ImportErrorCode::CopyFailure, "Managed staging write failed");
#else
    const auto* current = static_cast<const std::uint8_t*>(data);
    while (size != 0)
    {
      const auto written = write(mHandle, current, size);
      if (written <= 0) throw ImportException(ImportErrorCode::CopyFailure, "Managed staging write failed");
      current += written;
      size -= static_cast<std::size_t>(written);
    }
#endif
  }

  void Flush()
  {
#if defined(_WIN32)
    if (!FlushFileBuffers(mHandle)) throw ImportException(ImportErrorCode::CopyFailure, "Managed staging flush failed");
#else
    if (fsync(mHandle) != 0) throw ImportException(ImportErrorCode::CopyFailure, "Managed staging flush failed");
#endif
  }

private:
#if defined(_WIN32)
  HANDLE mHandle{INVALID_HANDLE_VALUE};
#else
  int mHandle{-1};
#endif
};

class ProcessWriteLock
{
public:
  explicit ProcessWriteLock(const std::filesystem::path& metadataRoot)
  {
    std::error_code error;
    std::filesystem::create_directories(metadataRoot, error);
    if (error) throw ImportException(ImportErrorCode::LibraryUnavailable, "Library lock directory is unavailable");
    const auto path = metadataRoot / "write.lock";
#if defined(_WIN32)
    mHandle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                          FILE_ATTRIBUTE_HIDDEN, nullptr);
    if (mHandle == INVALID_HANDLE_VALUE)
      throw ImportException(ImportErrorCode::LibraryUnavailable, "Library write lock could not be opened");
    OVERLAPPED overlap{};
    if (!LockFileEx(mHandle, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &overlap))
      throw ImportException(ImportErrorCode::LibraryUnavailable, "Library is busy in another process");
    mLocked = true;
#else
    mHandle = open(path.c_str(), O_CREAT | O_RDWR, 0600);
    if (mHandle < 0 || flock(mHandle, LOCK_EX) != 0)
      throw ImportException(ImportErrorCode::LibraryUnavailable, "Library is busy in another process");
#endif
  }

  ~ProcessWriteLock()
  {
#if defined(_WIN32)
    if (mLocked)
    {
      OVERLAPPED overlap{};
      UnlockFileEx(mHandle, 0, MAXDWORD, MAXDWORD, &overlap);
    }
    if (mHandle != INVALID_HANDLE_VALUE) CloseHandle(mHandle);
#else
    if (mHandle >= 0)
    {
      flock(mHandle, LOCK_UN);
      close(mHandle);
    }
#endif
  }

private:
#if defined(_WIN32)
  HANDLE mHandle{INVALID_HANDLE_VALUE};
  bool mLocked{};
#else
  int mHandle{-1};
#endif
};

std::string RandomKey()
{
  std::array<std::uint8_t, 16> bytes{};
#if defined(_WIN32)
  if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    throw ImportException(ImportErrorCode::InternalError, "Secure random generation failed");
#else
  arc4random_buf(bytes.data(), bytes.size());
#endif
  ContentHash expanded{};
  std::copy(bytes.begin(), bytes.end(), expanded.bytes.begin());
  return ToHex(expanded).substr(0, bytes.size() * 2);
}

ManagedObjectType ObjectTypeForPath(const std::filesystem::path& path)
{
  std::string extension = PathToUtf8(path.extension());
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (extension == ".nam")
    return ManagedObjectType::NamModel;
  if (extension == ".wav")
    return ManagedObjectType::WaveImpulseResponse;
  throw ImportException(ImportErrorCode::UnsupportedSource, "Only NAM and WAV files can enter managed storage");
}

std::string TypeDirectory(ManagedObjectType type)
{
  return type == ManagedObjectType::NamModel ? "nam" : "ir";
}

std::string TypeExtension(ManagedObjectType type)
{
  return type == ManagedObjectType::NamModel ? ".nam" : ".wav";
}

std::optional<std::string> ValidateFile(const std::filesystem::path& path, ManagedObjectType type)
{
  try
  {
    if (type == ManagedObjectType::NamModel)
    {
      loading::InspectRegularFile(path, ".nam", loading::kMaximumNamBytes);
      std::ifstream input(path, std::ios::binary);
      auto document = nlohmann::json::parse(input, nullptr, false);
      if (document.is_discarded() || !document.is_object() || !document.contains("architecture") ||
          !document.at("architecture").is_string() || !document.contains("config") || !document.contains("weights"))
        throw ImportException(ImportErrorCode::InvalidNam, "NAM JSON or required model fields are invalid");
      return document.at("architecture").get<std::string>();
    }
    else
      loading::InspectWaveFile(path);
  }
  catch (const loading::LoadException& exception)
  {
    throw ImportException(type == ManagedObjectType::NamModel ? ImportErrorCode::InvalidNam : ImportErrorCode::InvalidWave,
                          exception.what());
  }
  return std::nullopt;
}

void CopyFileBounded(const std::filesystem::path& source, const std::filesystem::path& destination,
                     const CancelCheck& cancelled, const ProgressCallback& progress)
{
  std::ifstream input(source, std::ios::binary);
  if (!input) throw ImportException(ImportErrorCode::CopyFailure, "Managed source could not be opened");
  SecureCopyOutput output(destination);
  std::vector<char> buffer(1024 * 1024);
  std::uint64_t completed = 0;
  for (;;)
  {
    if (cancelled && cancelled()) throw ImportException(ImportErrorCode::Cancelled, "Import copy cancelled");
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = input.gcount();
    if (count > 0)
    {
      output.Write(buffer.data(), static_cast<std::size_t>(count));
      completed += static_cast<std::uint64_t>(count);
      if (progress) progress({0, ImportTaskState::Hashing, completed, 0, 0, 0,
                              PathToUtf8(source.filename()), std::nullopt});
    }
    if (input.eof()) break;
    if (!input) throw ImportException(ImportErrorCode::CopyFailure, "Managed source read failed");
  }
  output.Flush();
}

} // namespace

ManagedLibrary::ManagedLibrary(std::filesystem::path root)
: mRoot(std::move(root)), mIndex(mRoot / "metadata")
{
}

void ManagedLibrary::Initialize()
{
  std::lock_guard<std::mutex> guard(mMutex);
  std::error_code error;
  if (std::filesystem::exists(mRoot, error) && IsLinkOrReparsePoint(mRoot))
    throw ImportException(ImportErrorCode::LibraryUnavailable,
                          "Managed library root must not be a link or reparse point");
  error.clear();
  std::filesystem::create_directories(mRoot, error);
  if (error || !std::filesystem::is_directory(mRoot) || IsLinkOrReparsePoint(mRoot))
    throw ImportException(ImportErrorCode::LibraryUnavailable,
                          "Managed library root is unavailable or redirected");
  for (const auto* name : {"objects/nam", "objects/ir", "packs", "metadata", "staging", "quarantine", "logs"})
  {
    const auto directory = mRoot / name;
    std::filesystem::create_directories(directory, error);
    if (error || !IsPathWithin(mRoot, directory) || IsLinkOrReparsePoint(directory))
      throw ImportException(ImportErrorCode::LibraryUnavailable,
                            "Managed library directory is unavailable or redirected");
  }
  ProcessWriteLock processLock(mRoot / "metadata");
  LoadLocked();
  if (!std::filesystem::exists(mIndex.IndexPath())) mIndex.Commit(mSnapshot);
  std::ofstream schema(mRoot / "metadata" / "schema-version", std::ios::binary | std::ios::trunc);
  schema << kLibrarySchemaVersion << '\n';
  for (const auto& entry : std::filesystem::directory_iterator(mRoot / "staging", error))
  {
    if (error) break;
    if (entry.path().parent_path() != mRoot / "staging") continue;
    RemoveManagedTree(entry.path(), error);
    error.clear();
  }
}

void ManagedLibrary::LoadLocked() { mSnapshot = mIndex.Load(); }

LibrarySnapshot ManagedLibrary::Snapshot() const
{
  std::lock_guard<std::mutex> guard(mMutex);
  return mSnapshot;
}

LibraryStatistics ManagedLibrary::Statistics() const
{
  std::lock_guard<std::mutex> guard(mMutex);
  LibraryStatistics result;
  result.packs = mSnapshot.packs.size();
  for (const auto& [key, object] : mSnapshot.objects)
  {
    result.managedBytes += object.fileSize;
    if (object.type == ManagedObjectType::NamModel) ++result.namObjects;
    else ++result.irObjects;
  }
  return result;
}

std::filesystem::path ManagedLibrary::ObjectPath(const ContentHash& hash, ManagedObjectType type) const
{
  const auto text = ToHex(hash);
  return mRoot / "objects" / TypeDirectory(type) / text.substr(0, 2) / (text + TypeExtension(type));
}

std::optional<std::filesystem::path> ManagedLibrary::Resolve(const ContentHash& hash, ManagedObjectType expectedType) const
{
  std::lock_guard<std::mutex> guard(mMutex);
  const auto iterator = mSnapshot.objects.find(ToHex(hash));
  if (iterator == mSnapshot.objects.end() || iterator->second.type != expectedType) return std::nullopt;
  const auto path = mRoot / std::filesystem::u8path(iterator->second.libraryRelativePath);
  if (!IsPathWithin(mRoot, path) || !std::filesystem::is_regular_file(path)) return std::nullopt;
  return path;
}

std::filesystem::path ManagedLibrary::NewStagingDirectory()
{
  const auto path = mRoot / "staging" / ("task-" + RandomKey());
  std::error_code error;
  if (!IsPathWithin(mRoot, path.parent_path()) || IsLinkOrReparsePoint(path.parent_path()) ||
      !std::filesystem::create_directory(path, error) || error || !IsPathWithin(mRoot, path))
    throw ImportException(ImportErrorCode::LibraryUnavailable, "Unique staging directory could not be created");
  return path;
}

ManagedLibrary::PromotedObject ManagedLibrary::Promote(const std::filesystem::path& source,
                                                       const std::string& displayName,
                                                       LocalContentSource sourceKind,
                                                       const CancelCheck& cancelled,
                                                       const ProgressCallback& progress)
{
  const auto type = ObjectTypeForPath(source);
  const auto architecture = ValidateFile(source, type);
  const auto stagingDirectory = NewStagingDirectory();
  const auto stagingPath = stagingDirectory / ("object" + TypeExtension(type));
  try
  {
    const auto sourceHash = HashFile(source, cancelled);
    CopyFileBounded(source, stagingPath, cancelled, progress);
    ValidateFile(stagingPath, type);
    const auto copiedHash = HashFile(stagingPath, cancelled);
    if (!(sourceHash == copiedHash))
      throw ImportException(ImportErrorCode::HashFailure, "Managed copy hash does not match its source");
    const auto destination = ObjectPath(copiedHash, type);
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error || !IsPathWithin(mRoot, destination) || IsLinkOrReparsePoint(destination.parent_path()))
      throw ImportException(ImportErrorCode::LibraryUnavailable,
                            "Managed object directory is unavailable or redirected");

    bool alreadyPresent = std::filesystem::exists(destination, error) && !error;
    if (alreadyPresent)
    {
      if (std::filesystem::file_size(destination, error) != std::filesystem::file_size(stagingPath) || error ||
          !(HashFile(destination, cancelled) == copiedHash))
        throw ImportException(ImportErrorCode::LibraryCorrupt, "Existing managed object does not match its identity");
      std::filesystem::remove(stagingPath, error);
    }
    else
    {
      std::filesystem::rename(stagingPath, destination, error);
      if (error)
      {
        if (std::filesystem::exists(destination) && HashFile(destination, cancelled) == copiedHash)
          alreadyPresent = true;
        else
          throw ImportException(ImportErrorCode::TransactionFailure, "Managed object publication failed");
      }
    }
    RemoveManagedTree(stagingDirectory, error);

    LibraryObject object;
    object.hash = copiedHash;
    object.type = type;
    object.libraryRelativePath = PathToUtf8(std::filesystem::relative(destination, mRoot));
    object.fileSize = std::filesystem::file_size(destination);
    object.originalDisplayName = displayName;
    object.source = sourceKind;
    object.importedAt = UnixTimeNow();
    object.lastUsedAt = object.importedAt;
    object.architecture = architecture;
    if (type == ManagedObjectType::WaveImpulseResponse)
    {
      const auto inspection = loading::InspectWaveFile(destination);
      object.waveSampleRate = inspection.sampleRate;
      object.waveChannels = inspection.channels;
    }
    return {std::move(object), alreadyPresent};
  }
  catch (...)
  {
    std::error_code error;
    RemoveManagedTree(stagingDirectory, error);
    throw;
  }
}

ImportSummary ManagedLibrary::ImportPrepared(const std::vector<std::filesystem::path>& files,
                                             const std::vector<std::string>& relativePaths,
                                             const std::string& displayName, LocalContentSource source,
                                             const std::optional<ContentHash>& sourceHash,
                                             const CancelCheck& cancelled, const ProgressCallback& progress)
{
  ImportSummary summary;
  std::lock_guard<std::mutex> guard(mMutex);
  ProcessWriteLock processLock(mRoot / "metadata");
  LoadLocked();
  auto next = mSnapshot;
  const auto packKey = RandomKey();
  PackRecord pack;
  pack.packKey = packKey;
  pack.source = source;
  pack.sourceHash = sourceHash;
  pack.displayName = displayName;
  pack.importedAt = UnixTimeNow();

  for (std::size_t index = 0; index < files.size(); ++index)
  {
    if (cancelled && cancelled()) throw ImportException(ImportErrorCode::Cancelled, "Import cancelled");
    try
    {
      auto promoted = Promote(files[index], PathToUtf8(files[index].filename()), source, cancelled, progress);
      const auto hashText = ToHex(promoted.object.hash);
      if (!next.objects.contains(hashText)) next.objects.emplace(hashText, promoted.object);
      PackEntryRecord association;
      association.packKey = packKey;
      association.objectHash = promoted.object.hash;
      association.relativePath = relativePaths.empty() ? PathToUtf8(files[index].filename()) : relativePaths[index];
      association.displayName = PathToUtf8(std::filesystem::u8path(association.relativePath).filename());
      association.type = promoted.object.type;
      association.sortKey = CaseCollisionKey(association.relativePath);
      next.packEntries.push_back(std::move(association));
      summary.objects.push_back(promoted.object.hash);
      if (promoted.alreadyPresent) ++summary.alreadyPresent;
      else ++summary.imported;
    }
    catch (const ImportException& exception)
    {
      if (exception.Code() == ImportErrorCode::Cancelled) throw;
      summary.failures.push_back({exception.Code(), PathToUtf8(files[index].filename()), exception.what()});
    }
  }
  if (!summary.objects.empty())
  {
    next.packs.emplace(packKey, std::move(pack));
    ++next.revision;
    mIndex.Commit(next);
    mSnapshot = std::move(next);
  }
  return summary;
}

ImportSummary ManagedLibrary::ImportFiles(const std::vector<std::filesystem::path>& files,
                                          const std::string& displayName, LocalContentSource source,
                                          const std::vector<std::string>& relativePaths,
                                          const CancelCheck& cancelled, const ProgressCallback& progress)
{
  if (!relativePaths.empty() && relativePaths.size() != files.size())
    throw ImportException(ImportErrorCode::InternalError, "Import relative-path count does not match file count");
  return ImportPrepared(files, relativePaths, displayName, source, std::nullopt, cancelled, progress);
}

ImportSummary ManagedLibrary::ImportFolderPlan(const ImportPackPlan& plan, const CancelCheck& cancelled,
                                               const ProgressCallback& progress)
{
  if (!plan.sourcePath) throw ImportException(ImportErrorCode::SourceNotFound, "Folder plan has no source path");
  std::vector<std::filesystem::path> files;
  std::vector<std::string> relative;
  for (const auto& entry : plan.entries)
  {
    if (!entry.selected || entry.status != ImportEntryStatus::Candidate ||
        (entry.fileType != ImportedFileType::NamModel && entry.fileType != ImportedFileType::WaveImpulseResponse))
      continue;
    const auto path = *plan.sourcePath / std::filesystem::u8path(entry.normalizedRelativePath);
    if (!IsPathWithin(*plan.sourcePath, path))
      throw ImportException(ImportErrorCode::UnsafeArchivePath, "Folder entry escaped its source root");
    files.push_back(path);
    relative.push_back(entry.normalizedRelativePath);
  }
  return ImportPrepared(files, relative, plan.displayName, LocalContentSource::ManagedImport, std::nullopt, cancelled,
                        progress);
}

ImportSummary ManagedLibrary::ImportZipPlan(const ImportPackPlan& plan, const CancelCheck& cancelled,
                                            const ProgressCallback& progress)
{
  const auto staging = NewStagingDirectory();
  try
  {
    auto extracted = ExtractSelectedZipEntries(plan, staging, {}, cancelled, progress);
    std::vector<std::filesystem::path> files;
    std::vector<std::string> relative;
    for (const auto& entry : extracted)
    {
      files.push_back(entry.stagingPath);
      relative.push_back(entry.descriptor.normalizedRelativePath);
    }
    auto summary = ImportPrepared(files, relative, plan.displayName, LocalContentSource::ManagedArchiveImport,
                                  plan.sourceArchiveHash, cancelled, progress);
    std::error_code error;
    RemoveManagedTree(staging, error);
    return summary;
  }
  catch (...)
  {
    std::error_code error;
    RemoveManagedTree(staging, error);
    throw;
  }
}

void ManagedLibrary::MarkUsed(const ContentHash& hash)
{
  std::lock_guard<std::mutex> guard(mMutex);
  ProcessWriteLock processLock(mRoot / "metadata");
  LoadLocked();
  auto iterator = mSnapshot.objects.find(ToHex(hash));
  if (iterator == mSnapshot.objects.end()) return;
  iterator->second.lastUsedAt = UnixTimeNow();
  ++mSnapshot.revision;
  mIndex.Commit(mSnapshot);
}

void ManagedLibrary::RemovePack(const std::string& packKey)
{
  std::lock_guard<std::mutex> guard(mMutex);
  ProcessWriteLock processLock(mRoot / "metadata");
  LoadLocked();
  if (mSnapshot.packs.erase(packKey) == 0) return;
  std::erase_if(mSnapshot.packEntries, [&](const PackEntryRecord& entry) { return entry.packKey == packKey; });
  ++mSnapshot.revision;
  mIndex.Commit(mSnapshot);
}

std::size_t ManagedLibrary::RemoveUnused(const std::unordered_set<std::string>& protectedHashes)
{
  std::lock_guard<std::mutex> guard(mMutex);
  ProcessWriteLock processLock(mRoot / "metadata");
  LoadLocked();
  std::unordered_set<std::string> associated;
  for (const auto& entry : mSnapshot.packEntries) associated.insert(ToHex(entry.objectHash));
  std::vector<std::pair<std::string, std::filesystem::path>> removals;
  auto next = mSnapshot;
  for (const auto& [key, object] : mSnapshot.objects)
  {
    if (associated.contains(key) || protectedHashes.contains(key) || object.pinned) continue;
    const auto path = mRoot / std::filesystem::u8path(object.libraryRelativePath);
    if (IsPathWithin(mRoot, path)) removals.emplace_back(key, path);
  }
  for (const auto& [key, path] : removals) next.objects.erase(key);
  if (removals.empty()) return 0;
  ++next.revision;
  mIndex.Commit(next);
  mSnapshot = std::move(next);
  std::size_t removed = 0;
  for (const auto& [key, path] : removals)
  {
    std::error_code error;
    if (std::filesystem::remove(path, error) && !error) ++removed;
  }
  return removed;
}

VerificationReport ManagedLibrary::Verify(bool recomputeHashes, const CancelCheck& cancelled,
                                          const ProgressCallback& progress)
{
  std::lock_guard<std::mutex> guard(mMutex);
  VerificationReport report;
  std::size_t completed = 0;
  for (const auto& [key, object] : mSnapshot.objects)
  {
    if (cancelled && cancelled()) throw ImportException(ImportErrorCode::Cancelled, "Library verification cancelled");
    const auto path = mRoot / std::filesystem::u8path(object.libraryRelativePath);
    if (!IsPathWithin(mRoot, path))
      report.issues.push_back({ImportErrorCode::LibraryCorrupt, key, "Object path is outside the library root"});
    else if (!std::filesystem::is_regular_file(path))
      report.issues.push_back({ImportErrorCode::SourceNotFound, key, "Managed object is missing"});
    else if (std::filesystem::file_size(path) != object.fileSize)
      report.issues.push_back({ImportErrorCode::LibraryCorrupt, key, "Managed object size differs from metadata"});
    else
    {
      try
      {
        ValidateFile(path, object.type);
        if (recomputeHashes && !(HashFile(path, cancelled) == object.hash))
          report.issues.push_back({ImportErrorCode::HashFailure, key, "Managed object hash mismatch"});
      }
      catch (const ImportException& exception)
      {
        if (exception.Code() == ImportErrorCode::Cancelled) throw;
        report.issues.push_back({exception.Code(), key, exception.what()});
      }
    }
    ++report.checkedObjects;
    ++completed;
    if (progress)
      progress({0, ImportTaskState::Validating, completed, mSnapshot.objects.size(), completed,
                mSnapshot.objects.size(), object.originalDisplayName, std::nullopt});
  }
  std::unordered_set<std::string> recordedPaths;
  for (const auto& [key, object] : mSnapshot.objects)
    recordedPaths.insert(CaseCollisionKey(object.libraryRelativePath));
  for (const auto* typeDirectory : {"objects/nam", "objects/ir"})
  {
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator iterator(mRoot / typeDirectory, error), end;
         iterator != end && !error; iterator.increment(error))
    {
      if (!iterator->is_regular_file(error) || error) continue;
      const auto relative = PathToUtf8(std::filesystem::relative(iterator->path(), mRoot, error));
      if (!error && !recordedPaths.contains(CaseCollisionKey(relative))) ++report.orphanedFiles;
    }
  }
  for (const auto& entry : mSnapshot.packEntries)
  {
    if (!mSnapshot.packs.contains(entry.packKey) || !mSnapshot.objects.contains(ToHex(entry.objectHash)))
      report.issues.push_back({ImportErrorCode::LibraryCorrupt, ToHex(entry.objectHash),
                               "Pack association references missing metadata"});
  }
  return report;
}

std::size_t ManagedLibrary::ClearStaging()
{
  std::lock_guard<std::mutex> guard(mMutex);
  std::size_t removed = 0;
  std::error_code error;
  const auto staging = mRoot / "staging";
  for (const auto& entry : std::filesystem::directory_iterator(staging, error))
  {
    if (error) break;
    if (entry.path().parent_path() != staging) continue;
    RemoveManagedTree(entry.path(), error);
    if (!error) ++removed;
    error.clear();
  }
  return removed;
}

} // namespace amphibia::library
