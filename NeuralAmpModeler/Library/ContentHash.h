#pragma once

#include "LibraryTypes.h"

#include <filesystem>
#include <span>
#include <string>

namespace amphibia::library {

std::string ToHex(const ContentHash& hash);
ContentHash ContentHashFromHex(const std::string& text);
ContentHash HashBytes(std::span<const std::uint8_t> bytes);
ContentHash HashFile(const std::filesystem::path& path, const CancelCheck& cancelled = {},
                     const std::function<void(std::uint64_t)>& progress = {});

} // namespace amphibia::library
