#pragma once

#include "LibraryTypes.h"

#include <filesystem>
#include <optional>
#include <string>

namespace amphibia::library {

struct NormalizedPath
{
  std::string value;
  std::vector<std::string> components;
};

std::optional<NormalizedPath> NormalizeImportPath(const std::string& original, const ImportLimits& limits,
                                                  std::string& diagnostic);
std::string CaseCollisionKey(const std::string& normalized);
ImportedFileType ClassifyImportPath(const std::string& normalized);
bool IsIgnoredPlatformPath(const std::string& normalized);
bool IsPathWithin(const std::filesystem::path& root, const std::filesystem::path& candidate);
std::string PathToUtf8(const std::filesystem::path& path);
bool IsWindowsReservedComponent(const std::string& component);

} // namespace amphibia::library
