#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace iplug { class IPlugAPP; }

struct ManagedLibraryConfigurationStatus
{
  std::string information;
  std::string rootPath;
  bool available{};
  bool taskActive{};
  bool verificationComplete{};
  bool healthy{};
};

namespace amphibia::configuration
{
#ifdef APP_API
ManagedLibraryConfigurationStatus ManagedLibrary(iplug::IPlugAPP& plug);
void VerifyManagedLibrary(iplug::IPlugAPP& plug);
void ClearManagedLibraryStaging(iplug::IPlugAPP& plug);
void ScanManagedLibrarySources(iplug::IPlugAPP& plug, std::vector<std::filesystem::path> sources);
void CancelManagedLibraryTask(iplug::IPlugAPP& plug);
#endif
} // namespace amphibia::configuration
