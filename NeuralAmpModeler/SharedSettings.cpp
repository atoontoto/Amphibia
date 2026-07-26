#include "SharedSettings.h"

#include "IPlugPaths.h"

namespace amphibia::preferences
{
std::filesystem::path DefaultSettingsPath()
{
  WDL_String appSupport;
  iplug::AppSupportPath(appSupport, false);
  if (!appSupport.GetLength()) return {};
  return std::filesystem::u8path(appSupport.Get()) / "Amphibia" / "settings-v1.json";
}

SharedSettingsResult LoadSharedSettings()
{
  SharedSettingsResult result;
  const auto path = DefaultSettingsPath();
  if (path.empty()) { result.message = "Application-data path is unavailable."; return result; }
  const setup::SettingsStore store(path);
  const auto loaded = store.Load();
  if (loaded.status == setup::SettingsLoadStatus::Loaded ||
      loaded.status == setup::SettingsLoadStatus::RecoveredFromBackup)
  {
    result.settings = loaded.document.shared;
    result.available = true;
    result.message = loaded.message;
    return result;
  }
  if (loaded.status == setup::SettingsLoadStatus::Missing)
  {
    result.available = true;
    result.message = "No shared folders are configured.";
    return result;
  }
  result.message = loaded.message.empty() ? "Shared settings are unavailable." : loaded.message;
  return result;
}

bool UpdateSharedSettings(const setup::SharedSettings& settings, std::string& error)
{
  const auto path = DefaultSettingsPath();
  if (path.empty()) { error = "Application-data path is unavailable."; return false; }
  return setup::SettingsStore(path).UpdateShared(settings, error);
}
} // namespace amphibia::preferences
