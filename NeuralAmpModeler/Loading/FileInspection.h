#pragma once

#include "LoadTypes.h"

#include <cstdint>
#include <filesystem>

namespace amphibia::loading
{

constexpr std::uintmax_t kMaximumNamBytes = 512ULL * 1024ULL * 1024ULL;
constexpr std::uintmax_t kMaximumIrBytes = 64ULL * 1024ULL * 1024ULL;

struct WaveInspection
{
  std::uint32_t sampleRate{};
  std::uint16_t channels{};
  std::uint16_t bitsPerSample{};
  std::uint16_t format{};
  std::uint32_t dataBytes{};
};

void InspectRegularFile(const std::filesystem::path& path, const char* requiredExtension,
                        std::uintmax_t maximumBytes);
WaveInspection InspectWaveFile(const std::filesystem::path& path);

} // namespace amphibia::loading
