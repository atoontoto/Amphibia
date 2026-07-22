#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace amphibia::library {

enum class LocalContentSource
{
  ReferencedFile,
  ReferencedFolder,
  ManagedImport,
  ManagedArchiveImport
};

enum class ManagedObjectType { NamModel, WaveImpulseResponse };
enum class ImportedFileType { NamModel, WaveImpulseResponse, MetadataText, Image, Unknown };
enum class ImportEntryStatus
{
  Candidate,
  Selected,
  Valid,
  DuplicateContent,
  Unsupported,
  UnsafePath,
  TooLarge,
  Encrypted,
  Invalid,
  Ignored,
  Cancelled
};

enum class ImportTaskState
{
  Idle,
  Scanning,
  AwaitingSelection,
  Extracting,
  Validating,
  Hashing,
  Committing,
  Completed,
  Failed,
  Cancelled
};

enum class ImportErrorCode
{
  None,
  SourceNotFound,
  SourceUnreadable,
  UnsupportedSource,
  InvalidArchive,
  EncryptedArchive,
  UnsafeArchivePath,
  DuplicateArchivePath,
  CaseCollision,
  UnsupportedCompression,
  EntryTooLarge,
  ArchiveTooLarge,
  TooManyEntries,
  ExcessiveDepth,
  CompressionRatioExceeded,
  InsufficientDiskSpace,
  InvalidNam,
  InvalidWave,
  HashFailure,
  CopyFailure,
  ExtractionFailure,
  MetadataFailure,
  TransactionFailure,
  LibraryUnavailable,
  LibraryCorrupt,
  Cancelled,
  InternalError
};

enum class CaptureRole
{
  AmpHead,
  AmpAndCab,
  Pedal,
  CabinetIR,
  SpaceIR,
  Outboard,
  Experimental,
  Unknown
};

struct ContentHash
{
  std::array<std::uint8_t, 32> bytes{};
  friend bool operator==(const ContentHash&, const ContentHash&) = default;
};

struct ImportLimits
{
  std::uint64_t maximumArchiveBytes{2ull * 1024 * 1024 * 1024};
  std::size_t maximumEntries{10000};
  std::size_t maximumDepth{32};
  std::size_t maximumPathBytes{512};
  std::size_t maximumComponentBytes{255};
  std::uint64_t maximumNamBytes{512ull * 1024 * 1024};
  std::uint64_t maximumWaveBytes{64ull * 1024 * 1024};
  std::uint64_t maximumMetadataBytes{10ull * 1024 * 1024};
  std::uint64_t maximumExtractedBytes{8ull * 1024 * 1024 * 1024};
  double maximumCompressionRatio{200.0};
  std::size_t maximumMetadataFiles{32};
};

struct ArchiveEntryDescriptor
{
  std::size_t archiveIndex{};
  std::string originalPath;
  std::string normalizedRelativePath;
  std::vector<std::string> parentGroups;
  std::string displayName;
  std::uint64_t compressedSize{};
  std::uint64_t uncompressedSize{};
  std::uint32_t crc32{};
  std::uint16_t compressionMethod{};
  ImportedFileType fileType{ImportedFileType::Unknown};
  ImportEntryStatus status{ImportEntryStatus::Candidate};
  bool isDirectory{};
  bool selected{};
  bool zip64{};
  std::optional<std::string> diagnostic;
};

struct ImportPackPlan
{
  std::string displayName;
  std::optional<std::filesystem::path> sourcePath;
  std::optional<ContentHash> sourceArchiveHash;
  std::vector<ArchiveEntryDescriptor> entries;
  std::optional<std::string> detectedCreator;
  std::optional<std::string> detectedLicense;
  std::optional<std::string> detectedDescription;
  std::uint64_t declaredCompressedBytes{};
  std::uint64_t declaredUncompressedBytes{};
  std::size_t unsafeEntries{};
  std::size_t unsupportedEntries{};
};

struct ImportProgress
{
  std::uint64_t taskId{};
  ImportTaskState state{ImportTaskState::Idle};
  std::uint64_t completedBytes{};
  std::uint64_t totalBytes{};
  std::size_t completedEntries{};
  std::size_t totalEntries{};
  std::string currentDisplayName;
  std::optional<std::string> message;
};

struct LibraryObject
{
  ContentHash hash;
  ManagedObjectType type{ManagedObjectType::NamModel};
  std::string libraryRelativePath;
  std::uint64_t fileSize{};
  std::string originalDisplayName;
  LocalContentSource source{LocalContentSource::ManagedImport};
  std::optional<std::string> architecture;
  std::optional<CaptureRole> captureRole;
  std::optional<double> waveSampleRate;
  std::optional<std::uint32_t> waveChannels;
  std::int64_t importedAt{};
  std::int64_t lastUsedAt{};
  std::uint32_t validationVersion{1};
  bool pinned{};
  bool missing{};
  bool corrupt{};
};

struct PackRecord
{
  std::string packKey;
  LocalContentSource source{LocalContentSource::ManagedImport};
  std::optional<ContentHash> sourceHash;
  std::string displayName;
  std::optional<std::string> sourceDisplayName;
  std::optional<std::string> creator;
  std::optional<std::string> license;
  std::optional<std::string> description;
  std::int64_t importedAt{};
};

struct PackEntryRecord
{
  std::string packKey;
  ContentHash objectHash;
  std::string relativePath;
  std::string displayName;
  ManagedObjectType type{ManagedObjectType::NamModel};
  std::string sortKey;
};

struct ImportFailure
{
  ImportErrorCode code{ImportErrorCode::None};
  std::string displayName;
  std::string message;
};

struct ImportSummary
{
  std::size_t imported{};
  std::size_t alreadyPresent{};
  std::size_t skipped{};
  std::vector<ImportFailure> failures;
  std::vector<ContentHash> objects;
};

using CancelCheck = std::function<bool()>;
using ProgressCallback = std::function<void(const ImportProgress&)>;

class ImportException : public std::runtime_error
{
public:
  ImportException(ImportErrorCode code, const std::string& message) : std::runtime_error(message), mCode(code) {}
  ImportErrorCode Code() const noexcept { return mCode; }

private:
  ImportErrorCode mCode;
};

inline std::int64_t UnixTimeNow()
{
  return std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::system_clock::now().time_since_epoch())
    .count();
}

} // namespace amphibia::library
