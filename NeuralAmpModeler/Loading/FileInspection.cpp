#include "FileInspection.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <string>

namespace amphibia::loading
{
namespace
{

std::uint16_t ReadU16(std::istream& stream)
{
  std::array<unsigned char, 2> bytes{};
  stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
  if (!stream)
    throw LoadException(LoadErrorCode::InvalidWave, "The WAV header is truncated");
  return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

std::uint32_t ReadU32(std::istream& stream)
{
  std::array<unsigned char, 4> bytes{};
  stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
  if (!stream)
    throw LoadException(LoadErrorCode::InvalidWave, "The WAV header is truncated");
  return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8)
         | (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

bool FourCC(const std::array<char, 4>& value, const char* expected)
{
  return std::equal(value.begin(), value.end(), expected);
}

std::string Lower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

} // namespace

void InspectRegularFile(const std::filesystem::path& path, const char* requiredExtension,
                        std::uintmax_t maximumBytes)
{
  std::error_code error;
  if (!std::filesystem::exists(path, error) || error)
    throw LoadException(LoadErrorCode::FileNotFound, "The selected file no longer exists");
  if (!std::filesystem::is_regular_file(path, error) || error)
    throw LoadException(LoadErrorCode::FileUnreadable, "The selected path is not a readable file");
  if (Lower(path.extension().string()) != requiredExtension)
    throw LoadException(LoadErrorCode::InvalidFormat, "The selected file has the wrong extension");
  const auto size = std::filesystem::file_size(path, error);
  if (error)
    throw LoadException(LoadErrorCode::FileUnreadable, "The selected file size could not be read");
  if (size == 0)
    throw LoadException(LoadErrorCode::InvalidFormat, "The selected file is empty");
  if (size > maximumBytes)
    throw LoadException(LoadErrorCode::InvalidFormat, "The selected file exceeds the loading size limit");
}

WaveInspection InspectWaveFile(const std::filesystem::path& path)
{
  InspectRegularFile(path, ".wav", kMaximumIrBytes);
  const auto fileBytes = std::filesystem::file_size(path);
  if (fileBytes < 12)
    throw LoadException(LoadErrorCode::InvalidWave, "The WAV header is truncated");

  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    throw LoadException(LoadErrorCode::FileUnreadable, "The WAV file could not be opened");

  std::array<char, 4> id{};
  stream.read(id.data(), id.size());
  if (!FourCC(id, "RIFF"))
    throw LoadException(LoadErrorCode::InvalidWave, "The file is not a RIFF WAV");
  const auto riffBytes = ReadU32(stream);
  stream.read(id.data(), id.size());
  if (!FourCC(id, "WAVE"))
    throw LoadException(LoadErrorCode::InvalidWave, "The RIFF file is not WAVE audio");
  if (static_cast<std::uint64_t>(riffBytes) + 8ULL > fileBytes)
    throw LoadException(LoadErrorCode::InvalidWave, "The WAV RIFF size exceeds the file");

  WaveInspection result;
  bool haveFormat = false;
  bool haveData = false;
  while (stream && static_cast<std::uint64_t>(stream.tellg()) + 8ULL <= fileBytes)
  {
    stream.read(id.data(), id.size());
    const auto chunkBytes = ReadU32(stream);
    const auto dataStart = static_cast<std::uint64_t>(stream.tellg());
    const auto paddedBytes = static_cast<std::uint64_t>(chunkBytes) + (chunkBytes & 1U);
    if (dataStart + paddedBytes > fileBytes)
      throw LoadException(LoadErrorCode::InvalidWave, "A WAV chunk exceeds the file bounds");

    if (FourCC(id, "fmt "))
    {
      if (haveFormat || chunkBytes < 16)
        throw LoadException(LoadErrorCode::InvalidWave, "The WAV format chunk is invalid");
      result.format = ReadU16(stream);
      result.channels = ReadU16(stream);
      result.sampleRate = ReadU32(stream);
      const auto byteRate = ReadU32(stream);
      const auto blockAlign = ReadU16(stream);
      result.bitsPerSample = ReadU16(stream);
      if (result.channels != 1)
        throw LoadException(LoadErrorCode::UnsupportedWaveEncoding, "Only mono IR WAV files are supported");
      if (result.sampleRate == 0 || result.sampleRate > 768000 || blockAlign == 0 || byteRate == 0)
        throw LoadException(LoadErrorCode::InvalidWave, "The WAV format values are invalid");
      if (result.format == 1)
      {
        if (result.bitsPerSample != 16 && result.bitsPerSample != 24 && result.bitsPerSample != 32)
          throw LoadException(LoadErrorCode::UnsupportedWaveEncoding, "The PCM bit depth is unsupported");
      }
      else if (result.format == 3)
      {
        if (result.bitsPerSample != 32)
          throw LoadException(LoadErrorCode::UnsupportedWaveEncoding, "Only 32-bit float WAV is supported");
      }
      else if (result.format != 65534)
      {
        throw LoadException(LoadErrorCode::UnsupportedWaveEncoding, "The WAV encoding is unsupported");
      }
      haveFormat = true;
    }
    else if (FourCC(id, "data"))
    {
      if (!haveFormat || haveData || chunkBytes == 0)
        throw LoadException(LoadErrorCode::InvalidWave, "The WAV data chunk is missing or empty");
      result.dataBytes = chunkBytes;
      haveData = true;
    }

    stream.clear();
    stream.seekg(static_cast<std::streamoff>(dataStart + paddedBytes), std::ios::beg);
    if (haveData)
      break;
  }

  if (!haveFormat || !haveData)
    throw LoadException(LoadErrorCode::InvalidWave, "The WAV is missing a format or data chunk");
  return result;
}

} // namespace amphibia::loading
