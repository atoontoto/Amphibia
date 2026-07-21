#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace amphibia::loading
{

enum class LoadState
{
  Idle,
  Inspecting,
  Preparing,
  ReadyToActivate,
  Active,
  Failed,
  Cancelled
};

enum class LoadErrorCode
{
  None,
  FileNotFound,
  FileUnreadable,
  InvalidFormat,
  UnsupportedArchitecture,
  InvalidModel,
  InvalidWave,
  UnsupportedWaveEncoding,
  PreparationFailed,
  ResamplingFailed,
  Cancelled,
  Superseded,
  StaleProcessingConfiguration,
  OutOfMemory,
  InternalError
};

struct LoadStatus
{
  std::uint64_t requestId{};
  LoadState state{LoadState::Idle};
  float progress{};
  std::string displayName;
  std::string message;
  std::optional<LoadErrorCode> errorCode;
};

struct ProcessingConfiguration
{
  double sampleRate{};
  int maximumBlockSize{};
  int inputChannels{};
  int outputChannels{};
  std::uint64_t generation{};
};

struct LoadRequest
{
  std::uint64_t requestId{};
  std::string path;
  std::string displayName;
  ProcessingConfiguration configuration;
  double userValue{};
  bool clear{};
};

class LoadException final : public std::runtime_error
{
public:
  LoadException(LoadErrorCode code, const std::string& message)
  : std::runtime_error(message)
  , mCode(code)
  {
  }

  LoadErrorCode Code() const noexcept { return mCode; }

private:
  LoadErrorCode mCode;
};

inline const char* ErrorCodeName(LoadErrorCode code) noexcept
{
  switch (code)
  {
    case LoadErrorCode::None: return "None";
    case LoadErrorCode::FileNotFound: return "FileNotFound";
    case LoadErrorCode::FileUnreadable: return "FileUnreadable";
    case LoadErrorCode::InvalidFormat: return "InvalidFormat";
    case LoadErrorCode::UnsupportedArchitecture: return "UnsupportedArchitecture";
    case LoadErrorCode::InvalidModel: return "InvalidModel";
    case LoadErrorCode::InvalidWave: return "InvalidWave";
    case LoadErrorCode::UnsupportedWaveEncoding: return "UnsupportedWaveEncoding";
    case LoadErrorCode::PreparationFailed: return "PreparationFailed";
    case LoadErrorCode::ResamplingFailed: return "ResamplingFailed";
    case LoadErrorCode::Cancelled: return "Cancelled";
    case LoadErrorCode::Superseded: return "Superseded";
    case LoadErrorCode::StaleProcessingConfiguration: return "StaleProcessingConfiguration";
    case LoadErrorCode::OutOfMemory: return "OutOfMemory";
    case LoadErrorCode::InternalError: return "InternalError";
  }
  return "InternalError";
}

} // namespace amphibia::loading
