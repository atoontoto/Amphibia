#include "FolderScanner.h"

#include "PathSafety.h"

#include <system_error>
#include <unordered_set>

#if defined(_WIN32)
#include <windows.h>
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

std::uint64_t LimitFor(ImportedFileType type, const ImportLimits& limits)
{
  if (type == ImportedFileType::NamModel) return limits.maximumNamBytes;
  if (type == ImportedFileType::WaveImpulseResponse) return limits.maximumWaveBytes;
  if (type == ImportedFileType::MetadataText) return limits.maximumMetadataBytes;
  return 0;
}

} // namespace

ImportPackPlan ScanFolder(const std::filesystem::path& root, const ImportLimits& limits, const CancelCheck& cancelled,
                          const ProgressCallback& progress)
{
  std::error_code error;
  if (!std::filesystem::exists(root, error) || error)
    throw ImportException(ImportErrorCode::SourceNotFound, "Import folder does not exist");
  if (!std::filesystem::is_directory(root, error) || error)
    throw ImportException(ImportErrorCode::UnsupportedSource, "Import source is not a directory");
  if (IsLinkOrReparsePoint(root))
    throw ImportException(ImportErrorCode::UnsupportedSource, "Import root must not be a link or reparse point");

  ImportPackPlan plan;
  plan.displayName = PathToUtf8(root.filename());
  plan.sourcePath = root;
  std::size_t visited = 0;
  std::size_t metadataFiles = 0;
  std::uint64_t scannedBytes = 0;

  std::filesystem::recursive_directory_iterator iterator(
    root, std::filesystem::directory_options::skip_permission_denied, error);
  const std::filesystem::recursive_directory_iterator end;
  while (iterator != end)
  {
    if (cancelled && cancelled()) throw ImportException(ImportErrorCode::Cancelled, "Folder scan cancelled");
    if (error)
    {
      error.clear();
      iterator.increment(error);
      continue;
    }
    if (++visited > limits.maximumEntries)
      throw ImportException(ImportErrorCode::TooManyEntries, "Folder exceeds the file-count limit");

    const auto path = iterator->path();
    if (IsLinkOrReparsePoint(path))
    {
      if (iterator->is_directory(error)) iterator.disable_recursion_pending();
      iterator.increment(error);
      continue;
    }
    if (iterator.depth() + 1 > static_cast<int>(limits.maximumDepth))
    {
      if (iterator->is_directory(error)) iterator.disable_recursion_pending();
      throw ImportException(ImportErrorCode::ExcessiveDepth, "Folder exceeds the directory-depth limit");
    }
    if (!iterator->is_regular_file(error))
    {
      iterator.increment(error);
      continue;
    }

    const auto relative = PathToUtf8(std::filesystem::relative(path, root, error));
    if (error)
    {
      error.clear();
      iterator.increment(error);
      continue;
    }
    std::string diagnostic;
    auto normalized = NormalizeImportPath(relative, limits, diagnostic);
    if (!normalized)
    {
      iterator.increment(error);
      continue;
    }

    ArchiveEntryDescriptor descriptor;
    descriptor.archiveIndex = plan.entries.size();
    descriptor.originalPath = relative;
    descriptor.normalizedRelativePath = normalized->value;
    descriptor.parentGroups.assign(normalized->components.begin(), normalized->components.end() - 1);
    descriptor.displayName = normalized->components.back();
    descriptor.fileType = ClassifyImportPath(descriptor.normalizedRelativePath);
    descriptor.uncompressedSize = std::filesystem::file_size(path, error);
    descriptor.compressedSize = descriptor.uncompressedSize;
    if (error)
    {
      descriptor.status = ImportEntryStatus::Invalid;
      descriptor.diagnostic = "File disappeared or became unreadable during scan";
      error.clear();
    }
    else if (IsIgnoredPlatformPath(descriptor.normalizedRelativePath))
    {
      descriptor.status = ImportEntryStatus::Ignored;
    }
    else if (descriptor.fileType == ImportedFileType::Image || descriptor.fileType == ImportedFileType::Unknown)
    {
      descriptor.status = ImportEntryStatus::Unsupported;
      ++plan.unsupportedEntries;
    }
    else if (descriptor.fileType == ImportedFileType::MetadataText && ++metadataFiles > limits.maximumMetadataFiles)
    {
      descriptor.status = ImportEntryStatus::TooLarge;
      descriptor.diagnostic = "Too many metadata files";
    }
    else if (descriptor.uncompressedSize > LimitFor(descriptor.fileType, limits))
    {
      descriptor.status = ImportEntryStatus::TooLarge;
      descriptor.diagnostic = "File exceeds the type-specific size limit";
    }
    else
    {
      descriptor.status = ImportEntryStatus::Candidate;
      descriptor.selected = descriptor.fileType == ImportedFileType::NamModel ||
                            descriptor.fileType == ImportedFileType::WaveImpulseResponse;
    }
    if (descriptor.uncompressedSize > limits.maximumExtractedBytes ||
        scannedBytes > limits.maximumExtractedBytes - descriptor.uncompressedSize)
      throw ImportException(ImportErrorCode::ArchiveTooLarge, "Folder exceeds the total scanned-byte limit");
    scannedBytes += descriptor.uncompressedSize;
    plan.declaredUncompressedBytes += descriptor.uncompressedSize;
    plan.declaredCompressedBytes += descriptor.compressedSize;
    plan.entries.push_back(std::move(descriptor));

    if (progress)
      progress({0, ImportTaskState::Scanning, scannedBytes, 0, plan.entries.size(), 0,
                plan.entries.back().displayName, std::nullopt});
    iterator.increment(error);
  }
  return plan;
}

} // namespace amphibia::library
