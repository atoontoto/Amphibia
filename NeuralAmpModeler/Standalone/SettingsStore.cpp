#include "SettingsStore.h"

#include <fstream>
#include <random>
#include <system_error>

#include "nlohmann/json.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace amphibia::setup
{
namespace
{
using Json = nlohmann::json;

Json AudioToJson(const AudioConfiguration& value)
{
  return {{"backend_id", value.backendId}, {"backend_name", value.backendName},
          {"input_device_id", value.inputDeviceId}, {"input_device_name", value.inputDeviceName},
          {"output_device_id", value.outputDeviceId}, {"output_device_name", value.outputDeviceName},
          {"input_channel", value.inputChannel}, {"output_first_channel", value.outputFirstChannel},
          {"output_channel_count", value.outputChannelCount}, {"sample_rate", value.sampleRate},
          {"buffer_size", value.bufferSize}};
}

bool BoundedString(const Json& object, const char* key, std::string& value, std::size_t max = 1024)
{
  if (!object.contains(key) || !object.at(key).is_string()) return false;
  value = object.at(key).get<std::string>();
  return value.size() <= max && value.find('\0') == std::string::npos;
}

std::optional<AudioConfiguration> AudioFromJson(const Json& value)
{
  if (!value.is_object()) return std::nullopt;
  AudioConfiguration result;
  if (!BoundedString(value, "backend_id", result.backendId, 128) ||
      !BoundedString(value, "backend_name", result.backendName, 256) ||
      !BoundedString(value, "input_device_id", result.inputDeviceId, 256) ||
      !BoundedString(value, "input_device_name", result.inputDeviceName, 256) ||
      !BoundedString(value, "output_device_id", result.outputDeviceId, 256) ||
      !BoundedString(value, "output_device_name", result.outputDeviceName, 256)) return std::nullopt;
  try
  {
    result.inputChannel = value.at("input_channel").get<std::uint32_t>();
    result.outputFirstChannel = value.at("output_first_channel").get<std::uint32_t>();
    result.outputChannelCount = value.at("output_channel_count").get<std::uint32_t>();
    result.sampleRate = value.at("sample_rate").get<std::uint32_t>();
    result.bufferSize = value.at("buffer_size").get<std::uint32_t>();
  }
  catch (...) { return std::nullopt; }
  if (result.outputChannelCount < 1 || result.outputChannelCount > 2 || result.sampleRate < 8000 ||
      result.sampleRate > 768000 || result.bufferSize < 8 || result.bufferSize > 65536) return std::nullopt;
  return result;
}

Json SetupToJson(const SetupState& value)
{
  Json result{{"schema_version", value.schemaVersion}, {"completion", ToString(value.completion)},
              {"audio_configured", value.audioConfigured},
              {"nam_folder_configured", value.namFolderConfigured},
              {"ir_folder_configured", value.irFolderConfigured},
              {"library_validated", value.libraryValidated},
              {"tone3000_configured", value.tone3000Configured}};
  if (value.lastCompletedAppVersion) result["last_completed_app_version"] = *value.lastCompletedAppVersion;
  if (value.completedAtUnixSeconds) result["completed_at_unix_seconds"] = *value.completedAtUnixSeconds;
  return result;
}

bool ParseSetup(const Json& value, SetupState& result, SettingsLoadResult& load)
{
  if (!value.is_object()) return false;
  const auto schema = value.value("schema_version", 0u);
  if (schema > kSetupSchemaVersion)
  {
    load.status = SettingsLoadStatus::FutureSchema;
    load.message = "The setup-state schema is newer than this Amphibia build.";
    return false;
  }
  if (schema != kSetupSchemaVersion) return false;
  const auto completion = ParseSetupCompletionState(value.value("completion", std::string{}));
  if (!completion) return false;
  result.schemaVersion = schema;
  result.completion = *completion;
  result.audioConfigured = value.value("audio_configured", false);
  result.namFolderConfigured = value.value("nam_folder_configured", false);
  result.irFolderConfigured = value.value("ir_folder_configured", false);
  result.libraryValidated = value.value("library_validated", false);
  result.tone3000Configured = value.value("tone3000_configured", false);
  if (value.contains("last_completed_app_version") && value.at("last_completed_app_version").is_string())
  {
    const auto version = value.at("last_completed_app_version").get<std::string>();
    if (version.size() <= 64) result.lastCompletedAppVersion = version;
  }
  if (value.contains("completed_at_unix_seconds") && value.at("completed_at_unix_seconds").is_number_integer())
    result.completedAtUnixSeconds = value.at("completed_at_unix_seconds").get<std::int64_t>();
  return true;
}

Json DocumentToJson(const SettingsDocument& document)
{
  Json shared = Json::object();
  if (document.shared.namFolder) shared["nam_folder"] = *document.shared.namFolder;
  if (document.shared.irFolder) shared["ir_folder"] = *document.shared.irFolder;
  Json standalone{{"setup", SetupToJson(document.standalone.setup)}};
  if (document.standalone.audio) standalone["audio"] = AudioToJson(*document.standalone.audio);
  return {{"schema_version", document.schemaVersion}, {"shared", shared}, {"standalone", standalone}};
}

SettingsLoadResult ParseDocument(const std::string& bytes)
{
  SettingsLoadResult result;
  if (bytes.size() > 1024 * 1024)
  {
    result.status = SettingsLoadStatus::Corrupt;
    result.message = "Settings exceed the 1 MiB safety limit.";
    return result;
  }
  try
  {
    const auto root = Json::parse(bytes);
    if (!root.is_object()) throw std::runtime_error("root is not an object");
    const auto schema = root.value("schema_version", 0u);
    if (schema > kSettingsSchemaVersion)
    {
      result.status = SettingsLoadStatus::FutureSchema;
      result.message = "The settings schema is newer than this Amphibia build.";
      return result;
    }
    if (schema != kSettingsSchemaVersion) throw std::runtime_error("unsupported settings schema");
    result.document.schemaVersion = schema;
    if (root.contains("shared") && root.at("shared").is_object())
    {
      const auto& shared = root.at("shared");
      std::string path;
      if (BoundedString(shared, "nam_folder", path, 32768)) result.document.shared.namFolder = path;
      if (BoundedString(shared, "ir_folder", path, 32768)) result.document.shared.irFolder = path;
    }
    if (!root.contains("standalone") || !root.at("standalone").is_object())
      throw std::runtime_error("standalone settings missing");
    const auto& standalone = root.at("standalone");
    if (!standalone.contains("setup") || !ParseSetup(standalone.at("setup"), result.document.standalone.setup, result))
    {
      if (result.status == SettingsLoadStatus::FutureSchema) return result;
      throw std::runtime_error("setup state is invalid");
    }
    if (standalone.contains("audio")) result.document.standalone.audio = AudioFromJson(standalone.at("audio"));
    result.status = SettingsLoadStatus::Loaded;
  }
  catch (const std::exception& error)
  {
    result.status = SettingsLoadStatus::Corrupt;
    result.message = std::string("Settings could not be read: ") + error.what();
  }
  return result;
}

std::optional<std::string> ReadFile(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return std::nullopt;
  std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  if (!stream.good() && !stream.eof()) return std::nullopt;
  return bytes;
}

bool ReplaceFile(const std::filesystem::path& source, const std::filesystem::path& destination,
                 std::string& error)
{
#if defined(_WIN32)
  if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
  {
    error = "Settings replacement failed (Windows error " + std::to_string(GetLastError()) + ").";
    return false;
  }
  return true;
#else
  std::error_code code;
  std::filesystem::rename(source, destination, code);
  if (code) { error = "Settings replacement failed."; return false; }
  return true;
#endif
}
} // namespace

SettingsStore::SettingsStore(std::filesystem::path path) : mPath(std::move(path)) {}

std::filesystem::path SettingsStore::BackupPath() const
{
  auto backup = mPath;
  backup += ".bak";
  return backup;
}

SettingsLoadResult SettingsStore::Load() const
{
  const auto primary = ReadFile(mPath);
  if (!primary)
  {
    SettingsLoadResult missing;
    missing.status = SettingsLoadStatus::Missing;
    missing.message = "Settings do not exist yet.";
    return missing;
  }
  auto result = ParseDocument(*primary);
  if (result.status == SettingsLoadStatus::Loaded || result.status == SettingsLoadStatus::FutureSchema) return result;
  const auto backup = ReadFile(BackupPath());
  if (backup)
  {
    auto recovered = ParseDocument(*backup);
    if (recovered.status == SettingsLoadStatus::Loaded)
    {
      recovered.status = SettingsLoadStatus::RecoveredFromBackup;
      recovered.message = "Recovered settings from the validated backup.";
      return recovered;
    }
  }
  return result;
}

bool SettingsStore::Save(const SettingsDocument& document, std::string& error) const
{
  if (document.schemaVersion != kSettingsSchemaVersion ||
      document.standalone.setup.schemaVersion != kSetupSchemaVersion)
  {
    error = "Unsupported settings schema.";
    return false;
  }
  const auto serialized = DocumentToJson(document).dump(2);
  if (ContainsSensitiveSetupData(serialized))
  {
    error = "Sensitive provider material is not allowed in setup settings.";
    return false;
  }
  std::error_code code;
  std::filesystem::create_directories(mPath.parent_path(), code);
  if (code) { error = "Settings directory could not be created."; return false; }
  std::mt19937_64 random(std::random_device{}());
  auto temporary = mPath;
  temporary += ".tmp-" + std::to_string(random());
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) { error = "Settings temporary file could not be opened."; return false; }
    stream.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
    stream.flush();
    if (!stream) { error = "Settings temporary file could not be written."; stream.close(); std::filesystem::remove(temporary, code); return false; }
  }
  if (std::filesystem::exists(mPath, code) && !code)
  {
    auto backupTemporary = BackupPath();
    backupTemporary += ".tmp";
    std::filesystem::copy_file(mPath, backupTemporary, std::filesystem::copy_options::overwrite_existing, code);
    if (!code)
    {
      std::string ignored;
      ReplaceFile(backupTemporary, BackupPath(), ignored);
    }
  }
  if (!ReplaceFile(temporary, mPath, error))
  {
    std::filesystem::remove(temporary, code);
    return false;
  }
  return true;
}

bool SettingsStore::UpdateShared(const SharedSettings& shared, std::string& error) const
{
  auto loaded = Load();
  if (loaded.status == SettingsLoadStatus::FutureSchema) { error = loaded.message; return false; }
  auto document = loaded.status == SettingsLoadStatus::Loaded || loaded.status == SettingsLoadStatus::RecoveredFromBackup
                    ? loaded.document : SettingsDocument{};
  document.shared = shared;
  return Save(document, error);
}

bool SettingsStore::UpdateStandalone(const StandaloneSettings& standalone, std::string& error) const
{
  auto loaded = Load();
  if (loaded.status == SettingsLoadStatus::FutureSchema) { error = loaded.message; return false; }
  auto document = loaded.status == SettingsLoadStatus::Loaded || loaded.status == SettingsLoadStatus::RecoveredFromBackup
                    ? loaded.document : SettingsDocument{};
  document.standalone = standalone;
  return Save(document, error);
}

bool ContainsSensitiveSetupData(const std::string& serialized)
{
  static const char* forbidden[] = {"access_token", "refresh_token", "authorization_code", "pkce",
                                    "code_verifier", "signed_url", "model_url", "client_secret"};
  for (const auto* value : forbidden) if (serialized.find(value) != std::string::npos) return true;
  return false;
}
} // namespace amphibia::setup
