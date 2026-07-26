#pragma once

#include "SetupTypes.h"

#include <filesystem>
#include <string>

namespace amphibia::setup
{
enum class SettingsLoadStatus
{
  Loaded,
  Missing,
  RecoveredFromBackup,
  Corrupt,
  FutureSchema,
  IOError
};

struct SettingsLoadResult
{
  SettingsLoadStatus status{SettingsLoadStatus::Missing};
  SettingsDocument document;
  std::string message;
};

class SettingsStore
{
public:
  explicit SettingsStore(std::filesystem::path path);

  SettingsLoadResult Load() const;
  bool Save(const SettingsDocument& document, std::string& error) const;
  bool UpdateShared(const SharedSettings& shared, std::string& error) const;
  bool UpdateStandalone(const StandaloneSettings& standalone, std::string& error) const;

  const std::filesystem::path& Path() const { return mPath; }
  std::filesystem::path BackupPath() const;

private:
  bool SaveUnlocked(const SettingsDocument& document, std::string& error) const;
  std::filesystem::path mPath;
};

bool ContainsSensitiveSetupData(const std::string& serialized);
} // namespace amphibia::setup
