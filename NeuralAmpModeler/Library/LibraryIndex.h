#pragma once

#include "LibraryTypes.h"

#include <unordered_map>

namespace amphibia::library {

inline constexpr std::uint32_t kLibrarySchemaVersion = 1;

struct LibrarySnapshot
{
  std::uint32_t schemaVersion{kLibrarySchemaVersion};
  std::uint64_t revision{};
  std::unordered_map<std::string, LibraryObject> objects;
  std::unordered_map<std::string, PackRecord> packs;
  std::vector<PackEntryRecord> packEntries;
  std::unordered_map<std::string, CaptureRole> userClassifications;
};

class LibraryIndex
{
public:
  explicit LibraryIndex(std::filesystem::path metadataRoot);

  LibrarySnapshot Load() const;
  void Commit(const LibrarySnapshot& snapshot) const;
  const std::filesystem::path& MetadataRoot() const { return mMetadataRoot; }
  std::filesystem::path IndexPath() const;

private:
  std::filesystem::path mMetadataRoot;
};

} // namespace amphibia::library
