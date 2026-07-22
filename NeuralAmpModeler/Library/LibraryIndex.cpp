#include "LibraryIndex.h"

#include "ContentHash.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#endif

namespace amphibia::library {
namespace {

using Json = nlohmann::json;

const char* TypeName(ManagedObjectType type)
{
  return type == ManagedObjectType::NamModel ? "nam" : "ir";
}

ManagedObjectType ParseType(const std::string& value)
{
  if (value == "nam") return ManagedObjectType::NamModel;
  if (value == "ir") return ManagedObjectType::WaveImpulseResponse;
  throw ImportException(ImportErrorCode::LibraryCorrupt, "Metadata contains an unknown object type");
}

Json ObjectJson(const LibraryObject& object)
{
  Json value = {{"hash", ToHex(object.hash)},
                {"type", TypeName(object.type)},
                {"path", object.libraryRelativePath},
                {"size", object.fileSize},
                {"display_name", object.originalDisplayName},
                {"source", static_cast<int>(object.source)},
                {"imported_at", object.importedAt},
                {"last_used_at", object.lastUsedAt},
                {"validation_version", object.validationVersion},
                {"pinned", object.pinned},
                {"missing", object.missing},
                {"corrupt", object.corrupt}};
  if (object.architecture) value["architecture"] = *object.architecture;
  if (object.captureRole) value["capture_role"] = static_cast<int>(*object.captureRole);
  if (object.waveSampleRate) value["wave_sample_rate"] = *object.waveSampleRate;
  if (object.waveChannels) value["wave_channels"] = *object.waveChannels;
  return value;
}

LibraryObject ParseObject(const Json& value)
{
  LibraryObject object;
  object.hash = ContentHashFromHex(value.at("hash").get<std::string>());
  object.type = ParseType(value.at("type").get<std::string>());
  object.libraryRelativePath = value.at("path").get<std::string>();
  object.fileSize = value.at("size").get<std::uint64_t>();
  object.originalDisplayName = value.value("display_name", std::string{});
  object.source = static_cast<LocalContentSource>(value.value("source", 2));
  object.importedAt = value.at("imported_at").get<std::int64_t>();
  object.lastUsedAt = value.at("last_used_at").get<std::int64_t>();
  object.validationVersion = value.value("validation_version", 1u);
  object.pinned = value.value("pinned", false);
  object.missing = value.value("missing", false);
  object.corrupt = value.value("corrupt", false);
  if (value.contains("architecture")) object.architecture = value.at("architecture").get<std::string>();
  if (value.contains("capture_role")) object.captureRole = static_cast<CaptureRole>(value.at("capture_role").get<int>());
  if (value.contains("wave_sample_rate")) object.waveSampleRate = value.at("wave_sample_rate").get<double>();
  if (value.contains("wave_channels")) object.waveChannels = value.at("wave_channels").get<std::uint32_t>();
  return object;
}

std::string RandomTemporarySuffix()
{
#if defined(_WIN32)
  std::array<std::uint8_t, 16> bytes{};
  if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    throw ImportException(ImportErrorCode::MetadataFailure, "Metadata temporary-name generation failed");
#else
  std::array<std::uint8_t, 16> bytes{};
  arc4random_buf(bytes.data(), bytes.size());
#endif
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size() * 2);
  for (const auto byte : bytes)
  {
    result.push_back(digits[byte >> 4]);
    result.push_back(digits[byte & 0x0f]);
  }
  return result;
}

void WriteExclusiveTemporary(const std::filesystem::path& path, std::string_view data)
{
#if defined(_WIN32)
  const HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    throw ImportException(ImportErrorCode::MetadataFailure, "Temporary metadata file could not be created safely");
  bool succeeded = true;
  std::size_t offset = 0;
  while (offset < data.size())
  {
    const auto amount = static_cast<DWORD>((std::min)(data.size() - offset, static_cast<std::size_t>(1024 * 1024)));
    DWORD written = 0;
    if (!WriteFile(handle, data.data() + offset, amount, &written, nullptr) || written != amount)
    {
      succeeded = false;
      break;
    }
    offset += written;
  }
  if (succeeded) succeeded = FlushFileBuffers(handle) != 0;
  CloseHandle(handle);
  if (!succeeded) throw ImportException(ImportErrorCode::MetadataFailure, "Temporary metadata write failed");
#else
  const int descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (descriptor < 0)
    throw ImportException(ImportErrorCode::MetadataFailure, "Temporary metadata file could not be created safely");
  bool succeeded = true;
  std::size_t offset = 0;
  while (offset < data.size())
  {
    const auto written = write(descriptor, data.data() + offset, data.size() - offset);
    if (written <= 0) { succeeded = false; break; }
    offset += static_cast<std::size_t>(written);
  }
  if (succeeded) succeeded = fsync(descriptor) == 0;
  close(descriptor);
  if (!succeeded) throw ImportException(ImportErrorCode::MetadataFailure, "Temporary metadata write failed");
#endif
}

void AtomicReplace(const std::filesystem::path& temporary, const std::filesystem::path& destination,
                   const std::filesystem::path& backup)
{
#if defined(_WIN32)
  if (std::filesystem::exists(destination))
  {
    if (!ReplaceFileW(destination.c_str(), temporary.c_str(), backup.c_str(), REPLACEFILE_WRITE_THROUGH, nullptr,
                      nullptr))
      throw ImportException(ImportErrorCode::TransactionFailure, "Atomic metadata replacement failed");
  }
  else if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
    throw ImportException(ImportErrorCode::TransactionFailure, "Initial metadata publication failed");
#else
  std::error_code error;
  if (std::filesystem::exists(destination, error) && !error)
    std::filesystem::copy_file(destination, backup, std::filesystem::copy_options::overwrite_existing, error);
  error.clear();
  std::filesystem::rename(temporary, destination, error);
  if (error) throw ImportException(ImportErrorCode::TransactionFailure, "Atomic metadata replacement failed");
#endif
}

} // namespace

LibraryIndex::LibraryIndex(std::filesystem::path metadataRoot) : mMetadataRoot(std::move(metadataRoot)) {}

std::filesystem::path LibraryIndex::IndexPath() const { return mMetadataRoot / "index-v1.json"; }

LibrarySnapshot LibraryIndex::Load() const
{
  const auto path = IndexPath();
  if (!std::filesystem::exists(path)) return {};
  try
  {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw ImportException(ImportErrorCode::LibraryCorrupt, "Library metadata is unreadable");
    Json root;
    input >> root;
    LibrarySnapshot snapshot;
    snapshot.schemaVersion = root.at("schema_version").get<std::uint32_t>();
    if (snapshot.schemaVersion > kLibrarySchemaVersion)
      throw ImportException(ImportErrorCode::LibraryCorrupt, "Library metadata uses an unsupported future schema");
    if (snapshot.schemaVersion != kLibrarySchemaVersion)
      throw ImportException(ImportErrorCode::LibraryCorrupt, "Library metadata schema is unsupported");
    snapshot.revision = root.value("revision", 0ull);
    for (const auto& value : root.value("objects", Json::array()))
    {
      auto object = ParseObject(value);
      const auto key = ToHex(object.hash);
      if (!snapshot.objects.emplace(key, std::move(object)).second)
        throw ImportException(ImportErrorCode::LibraryCorrupt, "Library metadata contains duplicate objects");
    }
    for (const auto& value : root.value("packs", Json::array()))
    {
      PackRecord pack;
      pack.packKey = value.at("key").get<std::string>();
      pack.source = static_cast<LocalContentSource>(value.value("source", 2));
      if (value.contains("source_hash")) pack.sourceHash = ContentHashFromHex(value.at("source_hash").get<std::string>());
      pack.displayName = value.at("display_name").get<std::string>();
      if (value.contains("source_display_name")) pack.sourceDisplayName = value.at("source_display_name").get<std::string>();
      if (value.contains("creator")) pack.creator = value.at("creator").get<std::string>();
      if (value.contains("license")) pack.license = value.at("license").get<std::string>();
      if (value.contains("description")) pack.description = value.at("description").get<std::string>();
      pack.importedAt = value.at("imported_at").get<std::int64_t>();
      if (!snapshot.packs.emplace(pack.packKey, std::move(pack)).second)
        throw ImportException(ImportErrorCode::LibraryCorrupt, "Library metadata contains duplicate packs");
    }
    for (const auto& value : root.value("pack_entries", Json::array()))
    {
      PackEntryRecord entry;
      entry.packKey = value.at("pack_key").get<std::string>();
      entry.objectHash = ContentHashFromHex(value.at("object_hash").get<std::string>());
      entry.relativePath = value.at("relative_path").get<std::string>();
      entry.displayName = value.at("display_name").get<std::string>();
      entry.type = ParseType(value.at("type").get<std::string>());
      entry.sortKey = value.value("sort_key", entry.relativePath);
      snapshot.packEntries.push_back(std::move(entry));
    }
    const auto classifications = root.value("user_classifications", Json::object());
    for (auto iterator = classifications.begin(); iterator != classifications.end(); ++iterator)
      snapshot.userClassifications.emplace(iterator.key(), static_cast<CaptureRole>(iterator.value().get<int>()));
    return snapshot;
  }
  catch (const ImportException&)
  {
    throw;
  }
  catch (...)
  {
    throw ImportException(ImportErrorCode::LibraryCorrupt, "Library metadata is corrupt");
  }
}

void LibraryIndex::Commit(const LibrarySnapshot& snapshot) const
{
  if (snapshot.schemaVersion != kLibrarySchemaVersion)
    throw ImportException(ImportErrorCode::MetadataFailure, "Refusing to write an unsupported metadata schema");
  std::error_code error;
  std::filesystem::create_directories(mMetadataRoot, error);
  if (error) throw ImportException(ImportErrorCode::LibraryUnavailable, "Metadata directory could not be created");

  Json root = {{"schema_version", snapshot.schemaVersion}, {"revision", snapshot.revision}};
  root["objects"] = Json::array();
  for (const auto& [key, object] : snapshot.objects) root["objects"].push_back(ObjectJson(object));
  root["packs"] = Json::array();
  for (const auto& [key, pack] : snapshot.packs)
  {
    Json value = {{"key", pack.packKey},
                  {"source", static_cast<int>(pack.source)},
                  {"display_name", pack.displayName},
                  {"imported_at", pack.importedAt}};
    if (pack.sourceHash) value["source_hash"] = ToHex(*pack.sourceHash);
    if (pack.sourceDisplayName) value["source_display_name"] = *pack.sourceDisplayName;
    if (pack.creator) value["creator"] = *pack.creator;
    if (pack.license) value["license"] = *pack.license;
    if (pack.description) value["description"] = *pack.description;
    root["packs"].push_back(std::move(value));
  }
  root["pack_entries"] = Json::array();
  for (const auto& entry : snapshot.packEntries)
    root["pack_entries"].push_back({{"pack_key", entry.packKey},
                                     {"object_hash", ToHex(entry.objectHash)},
                                     {"relative_path", entry.relativePath},
                                     {"display_name", entry.displayName},
                                     {"type", TypeName(entry.type)},
                                     {"sort_key", entry.sortKey}});
  root["user_classifications"] = Json::object();
  for (const auto& [hash, role] : snapshot.userClassifications)
    root["user_classifications"][hash] = static_cast<int>(role);

  const auto temporary = mMetadataRoot / ("index-v1.tmp-" + RandomTemporarySuffix());
  const auto backup = mMetadataRoot / "index-v1.json.bak";
  try
  {
    const auto serialized = root.dump(2) + '\n';
    WriteExclusiveTemporary(temporary, serialized);
    AtomicReplace(temporary, IndexPath(), backup);
  }
  catch (...)
  {
    std::filesystem::remove(temporary, error);
    throw;
  }
}

} // namespace amphibia::library
