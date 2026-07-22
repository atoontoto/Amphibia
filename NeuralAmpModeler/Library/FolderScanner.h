#pragma once

#include "LibraryTypes.h"

namespace amphibia::library {

ImportPackPlan ScanFolder(const std::filesystem::path& root, const ImportLimits& limits = {},
                          const CancelCheck& cancelled = {}, const ProgressCallback& progress = {});

} // namespace amphibia::library
