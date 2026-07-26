#pragma once

#include "Standalone/SettingsStore.h"

#include <filesystem>
#include <string>

namespace amphibia::preferences
{
struct SharedSettingsResult
{
  setup::SharedSettings settings;
  bool available{};
  std::string message;
};

std::filesystem::path DefaultSettingsPath();
SharedSettingsResult LoadSharedSettings();
bool UpdateSharedSettings(const setup::SharedSettings& settings, std::string& error);
} // namespace amphibia::preferences
