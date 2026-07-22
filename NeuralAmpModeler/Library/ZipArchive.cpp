#include "ZipArchive.h"

#include "ContentHash.h"
#include "PathSafety.h"

#include "mz.h"
#include "mz_strm.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"

#include <array>
#include <fstream>
#include <limits>
#include <unordered_map>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace amphibia::library {
namespace {

class ZipReader
{
public:
  explicit ZipReader(const std::filesystem::path& path)
  {
    mHandle = mz_zip_reader_create();
    if (mHandle == nullptr) throw ImportException(ImportErrorCode::InternalError, "ZIP reader allocation failed");
    const auto utf8 = path.u8string();
    if (mz_zip_reader_open_file(mHandle, reinterpret_cast<const char*>(utf8.c_str())) != MZ_OK)
      throw ImportException(ImportErrorCode::InvalidArchive, "ZIP archive could not be opened");
    mOpen = true;
  }

  ~ZipReader()
  {
    if (mOpen) mz_zip_reader_close(mHandle);
    if (mHandle != nullptr) mz_zip_reader_delete(&mHandle);
  }

  void* Get() const { return mHandle; }

private:
  void* mHandle{};
  bool mOpen{};
};

class SecureOutput
{
public:
  explicit SecureOutput(const std::filesystem::path& path)
  {
#if defined(_WIN32)
    mHandle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                          FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (mHandle == INVALID_HANDLE_VALUE)
      throw ImportException(ImportErrorCode::ExtractionFailure, "Could not create a unique staging file");
#else
    mHandle = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (mHandle < 0)
      throw ImportException(ImportErrorCode::ExtractionFailure, "Could not create a unique staging file");
#endif
  }

  ~SecureOutput()
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
      throw ImportException(ImportErrorCode::ExtractionFailure, "Staging-file write failed");
#else
    const auto written = write(mHandle, data, size);
    if (written < 0 || static_cast<std::size_t>(written) != size)
      throw ImportException(ImportErrorCode::ExtractionFailure, "Staging-file write failed");
#endif
  }

  void Flush()
  {
#if defined(_WIN32)
    if (!FlushFileBuffers(mHandle)) throw ImportException(ImportErrorCode::ExtractionFailure, "Staging flush failed");
#else
    if (fsync(mHandle) != 0) throw ImportException(ImportErrorCode::ExtractionFailure, "Staging flush failed");
#endif
  }

private:
#if defined(_WIN32)
  HANDLE mHandle{INVALID_HANDLE_VALUE};
#else
  int mHandle{-1};
#endif
};

std::uint64_t TypeLimit(ImportedFileType type, const ImportLimits& limits)
{
  if (type == ImportedFileType::NamModel) return limits.maximumNamBytes;
  if (type == ImportedFileType::WaveImpulseResponse) return limits.maximumWaveBytes;
  if (type == ImportedFileType::MetadataText) return limits.maximumMetadataBytes;
  return 0;
}

bool IsSpecialEntry(const mz_zip_file& info, bool isDirectory)
{
  if (info.linkname != nullptr && info.linkname[0] != 0) return true;
  const auto host = MZ_HOST_SYSTEM(info.version_madeby);
  if (host == MZ_HOST_SYSTEM_UNIX || host == MZ_HOST_SYSTEM_OSX_DARWIN)
  {
    const std::uint32_t mode = info.external_fa >> 16;
    const std::uint32_t kind = mode & 0170000u;
    if (kind != 0 && kind != 0100000u && !(isDirectory && kind == 0040000u)) return true;
  }
  if (host == MZ_HOST_SYSTEM_WINDOWS_NTFS && (info.external_fa & 0x400u) != 0) return true;
  return false;
}

void MarkCollision(ImportPackPlan& plan, std::size_t previous, ArchiveEntryDescriptor& current,
                   const std::string& message)
{
  auto& prior = plan.entries[previous];
  if (prior.status != ImportEntryStatus::UnsafePath) ++plan.unsafeEntries;
  prior.status = ImportEntryStatus::UnsafePath;
  prior.selected = false;
  prior.diagnostic = message;
  current.status = ImportEntryStatus::UnsafePath;
  current.selected = false;
  current.diagnostic = message;
  ++plan.unsafeEntries;
}

} // namespace

ImportPackPlan ScanZipArchive(const std::filesystem::path& archive, const ImportLimits& limits,
                              const CancelCheck& cancelled, const ProgressCallback& progress)
{
  std::error_code error;
  if (!std::filesystem::exists(archive, error) || error)
    throw ImportException(ImportErrorCode::SourceNotFound, "ZIP source does not exist");
  if (std::filesystem::is_symlink(std::filesystem::symlink_status(archive, error)) || error)
    throw ImportException(ImportErrorCode::UnsupportedSource, "ZIP source must not be a symbolic link");
#if defined(_WIN32)
  const DWORD archiveAttributes = GetFileAttributesW(archive.c_str());
  if (archiveAttributes != INVALID_FILE_ATTRIBUTES && (archiveAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
    throw ImportException(ImportErrorCode::UnsupportedSource, "ZIP source must not be a reparse point");
#endif
  if (!std::filesystem::is_regular_file(archive, error) || error)
    throw ImportException(ImportErrorCode::UnsupportedSource, "ZIP source is not a regular file");
  const auto archiveBytes = std::filesystem::file_size(archive, error);
  if (error) throw ImportException(ImportErrorCode::SourceUnreadable, "ZIP size could not be read");
  if (archiveBytes > limits.maximumArchiveBytes)
    throw ImportException(ImportErrorCode::ArchiveTooLarge, "ZIP exceeds the input-size limit");

  ZipReader reader(archive);
  ImportPackPlan plan;
  plan.displayName = PathToUtf8(archive.stem());
  plan.sourcePath = archive;
  plan.sourceArchiveHash = HashFile(archive, cancelled);
  std::unordered_map<std::string, std::size_t> exactPaths;
  std::unordered_map<std::string, std::size_t> casePaths;
  std::size_t metadataFiles = 0;

  int32_t result = mz_zip_reader_goto_first_entry(reader.Get());
  if (result == MZ_END_OF_LIST) return plan;
  if (result != MZ_OK) throw ImportException(ImportErrorCode::InvalidArchive, "ZIP central directory is invalid");

  std::size_t index = 0;
  while (result == MZ_OK)
  {
    if (cancelled && cancelled()) throw ImportException(ImportErrorCode::Cancelled, "ZIP scan cancelled");
    if (index >= limits.maximumEntries)
      throw ImportException(ImportErrorCode::TooManyEntries, "ZIP exceeds the entry-count limit");

    mz_zip_file* info = nullptr;
    if (mz_zip_reader_entry_get_info(reader.Get(), &info) != MZ_OK || info == nullptr || info->filename == nullptr)
      throw ImportException(ImportErrorCode::InvalidArchive, "ZIP entry metadata is invalid");

    ArchiveEntryDescriptor descriptor;
    descriptor.archiveIndex = index;
    descriptor.originalPath = info->filename;
    descriptor.compressedSize = info->compressed_size < 0 ? 0 : static_cast<std::uint64_t>(info->compressed_size);
    descriptor.uncompressedSize = info->uncompressed_size < 0 ? 0 : static_cast<std::uint64_t>(info->uncompressed_size);
    descriptor.crc32 = info->crc;
    descriptor.compressionMethod = info->compression_method;
    descriptor.zip64 = info->zip64 != 0;
    descriptor.isDirectory = mz_zip_reader_entry_is_dir(reader.Get()) == MZ_OK;

    std::string diagnostic;
    const auto normalized = NormalizeImportPath(descriptor.originalPath, limits, diagnostic);
    if (!normalized)
    {
      descriptor.status = ImportEntryStatus::UnsafePath;
      descriptor.diagnostic = diagnostic;
      ++plan.unsafeEntries;
    }
    else
    {
      descriptor.normalizedRelativePath = normalized->value;
      descriptor.parentGroups.assign(normalized->components.begin(), normalized->components.end() - 1);
      descriptor.displayName = normalized->components.back();
      descriptor.fileType = ClassifyImportPath(descriptor.normalizedRelativePath);
      if (descriptor.isDirectory)
      {
        descriptor.status = ImportEntryStatus::Ignored;
      }
      else if (info->disk_number != 0)
      {
        descriptor.status = ImportEntryStatus::Invalid;
        descriptor.diagnostic = "Multi-disk ZIP entries are unsupported";
      }
      else if (IsSpecialEntry(*info, descriptor.isDirectory))
      {
        descriptor.status = ImportEntryStatus::UnsafePath;
        descriptor.diagnostic = "Links and special archive entries are rejected";
        ++plan.unsafeEntries;
      }
      else if ((info->flag & MZ_ZIP_FLAG_ENCRYPTED) != 0 || info->aes_version != 0)
      {
        descriptor.status = ImportEntryStatus::Encrypted;
        descriptor.diagnostic = "Encrypted ZIP entries are unsupported";
      }
      else if (info->compression_method != MZ_COMPRESS_METHOD_STORE &&
               info->compression_method != MZ_COMPRESS_METHOD_DEFLATE)
      {
        descriptor.status = ImportEntryStatus::Unsupported;
        descriptor.diagnostic = "ZIP compression method is unsupported";
        ++plan.unsupportedEntries;
      }
      else if (info->compressed_size < 0 || info->uncompressed_size < 0)
      {
        descriptor.status = ImportEntryStatus::Invalid;
        descriptor.diagnostic = "ZIP entry contains a negative size";
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
      else if (descriptor.uncompressedSize > TypeLimit(descriptor.fileType, limits))
      {
        descriptor.status = ImportEntryStatus::TooLarge;
        descriptor.diagnostic = "ZIP entry exceeds the type-specific size limit";
      }
      else if (descriptor.uncompressedSize > 0 &&
               (descriptor.compressedSize == 0 ||
                static_cast<double>(descriptor.uncompressedSize) / descriptor.compressedSize >
                  limits.maximumCompressionRatio))
      {
        descriptor.status = ImportEntryStatus::TooLarge;
        descriptor.diagnostic = "ZIP entry exceeds the compression-ratio limit";
      }
      else
      {
        descriptor.status = ImportEntryStatus::Candidate;
        descriptor.selected = descriptor.fileType == ImportedFileType::NamModel ||
                              descriptor.fileType == ImportedFileType::WaveImpulseResponse;
      }

      const auto exact = exactPaths.find(descriptor.normalizedRelativePath);
      const auto caseKey = CaseCollisionKey(descriptor.normalizedRelativePath);
      const auto folded = casePaths.find(caseKey);
      if (exact != exactPaths.end())
        MarkCollision(plan, exact->second, descriptor, "Duplicate normalized ZIP path");
      else if (folded != casePaths.end())
        MarkCollision(plan, folded->second, descriptor, "Case-colliding ZIP path");
      else
      {
        exactPaths.emplace(descriptor.normalizedRelativePath, plan.entries.size());
        casePaths.emplace(caseKey, plan.entries.size());
      }
    }

    if (descriptor.uncompressedSize > limits.maximumExtractedBytes ||
        plan.declaredUncompressedBytes > limits.maximumExtractedBytes - descriptor.uncompressedSize)
      throw ImportException(ImportErrorCode::ArchiveTooLarge, "ZIP exceeds the total uncompressed-size limit");
    if (plan.declaredCompressedBytes > (std::numeric_limits<std::uint64_t>::max)() - descriptor.compressedSize)
      throw ImportException(ImportErrorCode::InvalidArchive, "ZIP compressed-size total overflows");
    plan.declaredCompressedBytes += descriptor.compressedSize;
    plan.declaredUncompressedBytes += descriptor.uncompressedSize;
    plan.entries.push_back(std::move(descriptor));
    if (progress)
      progress({0, ImportTaskState::Scanning, plan.declaredCompressedBytes, archiveBytes, plan.entries.size(), 0,
                plan.entries.back().displayName, std::nullopt});

    ++index;
    result = mz_zip_reader_goto_next_entry(reader.Get());
  }
  if (result != MZ_END_OF_LIST)
    throw ImportException(ImportErrorCode::InvalidArchive, "ZIP central-directory traversal failed");
  return plan;
}

std::vector<ExtractedEntry> ExtractSelectedZipEntries(const ImportPackPlan& plan,
                                                      const std::filesystem::path& stagingRoot,
                                                      const ImportLimits& limits, const CancelCheck& cancelled,
                                                      const ProgressCallback& progress)
{
  if (!plan.sourcePath) throw ImportException(ImportErrorCode::SourceNotFound, "ZIP plan has no source path");
  std::error_code error;
  std::filesystem::create_directories(stagingRoot, error);
  if (error || !std::filesystem::is_directory(stagingRoot, error))
    throw ImportException(ImportErrorCode::LibraryUnavailable, "Staging directory is unavailable");

  ZipReader reader(*plan.sourcePath);
  std::vector<ExtractedEntry> extracted;
  std::uint64_t totalActual = 0;
  std::size_t completed = 0;
  std::unordered_map<std::size_t, const ArchiveEntryDescriptor*> selected;
  for (const auto& descriptor : plan.entries)
    if (descriptor.selected && descriptor.status == ImportEntryStatus::Candidate)
      selected.emplace(descriptor.archiveIndex, &descriptor);

  std::uint64_t selectedBytes = 0;
  for (const auto& [index, descriptor] : selected)
  {
    if (descriptor->uncompressedSize > limits.maximumExtractedBytes ||
        selectedBytes > limits.maximumExtractedBytes - descriptor->uncompressedSize)
      throw ImportException(ImportErrorCode::ArchiveTooLarge, "Selected entries exceed extraction limits");
    selectedBytes += descriptor->uncompressedSize;
  }
  const auto capacity = std::filesystem::space(stagingRoot, error);
  if (error || capacity.available < selectedBytes)
    throw ImportException(ImportErrorCode::InsufficientDiskSpace, "Insufficient free space for selected entries");

  int32_t result = mz_zip_reader_goto_first_entry(reader.Get());
  std::size_t index = 0;
  while (result == MZ_OK)
  {
    if (cancelled && cancelled()) throw ImportException(ImportErrorCode::Cancelled, "ZIP extraction cancelled");
    const auto chosen = selected.find(index);
    if (chosen != selected.end())
    {
      const auto& descriptor = *chosen->second;
      const auto suffix = descriptor.fileType == ImportedFileType::NamModel ? ".nam" : ".wav";
      const auto outputPath = stagingRoot / ("entry-" + std::to_string(index) + suffix);
      SecureOutput output(outputPath);
      if (mz_zip_reader_entry_open(reader.Get()) != MZ_OK)
        throw ImportException(ImportErrorCode::ExtractionFailure, "Selected ZIP entry could not be opened");

      std::array<std::uint8_t, 256 * 1024> buffer{};
      std::uint64_t entryActual = 0;
      for (;;)
      {
        if (cancelled && cancelled())
        {
          mz_zip_reader_entry_close(reader.Get());
          throw ImportException(ImportErrorCode::Cancelled, "ZIP extraction cancelled");
        }
        const int32_t count = mz_zip_reader_entry_read(reader.Get(), buffer.data(), static_cast<int32_t>(buffer.size()));
        if (count < 0)
        {
          mz_zip_reader_entry_close(reader.Get());
          throw ImportException(ImportErrorCode::ExtractionFailure, "ZIP entry decompression failed");
        }
        if (count == 0) break;
        const auto bytes = static_cast<std::uint64_t>(count);
        const auto typeLimit = TypeLimit(descriptor.fileType, limits);
        if (bytes > typeLimit || entryActual > typeLimit - bytes ||
            bytes > limits.maximumExtractedBytes || totalActual > limits.maximumExtractedBytes - bytes)
        {
          mz_zip_reader_entry_close(reader.Get());
          throw ImportException(ImportErrorCode::EntryTooLarge, "Actual extracted bytes exceed policy limits");
        }
        output.Write(buffer.data(), static_cast<std::size_t>(count));
        entryActual += bytes;
        totalActual += bytes;
        if (progress)
          progress({0, ImportTaskState::Extracting, totalActual, plan.declaredUncompressedBytes, completed,
                    selected.size(), descriptor.displayName, std::nullopt});
      }
      if (mz_zip_reader_entry_close(reader.Get()) != MZ_OK)
        throw ImportException(ImportErrorCode::ExtractionFailure, "ZIP entry CRC or size validation failed");
      if (entryActual != descriptor.uncompressedSize)
        throw ImportException(ImportErrorCode::ExtractionFailure, "ZIP entry actual size differs from its declaration");
      output.Flush();
      extracted.push_back({descriptor, outputPath});
      ++completed;
    }
    ++index;
    result = mz_zip_reader_goto_next_entry(reader.Get());
  }
  if (result != MZ_END_OF_LIST)
    throw ImportException(ImportErrorCode::InvalidArchive, "ZIP traversal failed during extraction");
  return extracted;
}

} // namespace amphibia::library
