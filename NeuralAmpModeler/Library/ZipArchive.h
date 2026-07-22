#pragma once

#include "LibraryTypes.h"

namespace amphibia::library {

struct ExtractedEntry
{
  ArchiveEntryDescriptor descriptor;
  std::filesystem::path stagingPath;
};

ImportPackPlan ScanZipArchive(const std::filesystem::path& archive, const ImportLimits& limits = {},
                              const CancelCheck& cancelled = {}, const ProgressCallback& progress = {});

std::vector<ExtractedEntry> ExtractSelectedZipEntries(const ImportPackPlan& plan,
                                                      const std::filesystem::path& stagingRoot,
                                                      const ImportLimits& limits = {},
                                                      const CancelCheck& cancelled = {},
                                                      const ProgressCallback& progress = {});

} // namespace amphibia::library
