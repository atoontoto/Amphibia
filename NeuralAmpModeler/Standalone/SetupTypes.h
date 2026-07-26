#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace amphibia::setup
{
constexpr std::uint32_t kSettingsSchemaVersion = 1;
constexpr std::uint32_t kSetupSchemaVersion = 1;

enum class SetupCompletionState
{
  NeverStarted,
  InProgress,
  Completed,
  Skipped
};

struct SetupState
{
  std::uint32_t schemaVersion{kSetupSchemaVersion};
  SetupCompletionState completion{SetupCompletionState::NeverStarted};
  bool audioConfigured{};
  bool namFolderConfigured{};
  bool irFolderConfigured{};
  bool libraryValidated{};
  bool tone3000Configured{};
  std::optional<std::string> lastCompletedAppVersion;
  std::optional<std::int64_t> completedAtUnixSeconds;
};

struct AudioConfiguration
{
  std::string backendId;
  std::string backendName;
  std::string inputDeviceId;
  std::string inputDeviceName;
  std::string outputDeviceId;
  std::string outputDeviceName;
  std::uint32_t inputChannel{};
  std::uint32_t outputFirstChannel{};
  std::uint32_t outputChannelCount{2};
  std::uint32_t sampleRate{48000};
  std::uint32_t bufferSize{256};
};

inline bool operator==(const AudioConfiguration& lhs, const AudioConfiguration& rhs)
{
  return lhs.backendId == rhs.backendId && lhs.inputDeviceId == rhs.inputDeviceId &&
         lhs.outputDeviceId == rhs.outputDeviceId && lhs.inputChannel == rhs.inputChannel &&
         lhs.outputFirstChannel == rhs.outputFirstChannel && lhs.outputChannelCount == rhs.outputChannelCount &&
         lhs.sampleRate == rhs.sampleRate && lhs.bufferSize == rhs.bufferSize;
}

inline bool operator!=(const AudioConfiguration& lhs, const AudioConfiguration& rhs) { return !(lhs == rhs); }

struct AudioDeviceDescriptor
{
  std::string id;
  std::string name;
  std::vector<std::string> inputChannelNames;
  std::vector<std::string> outputChannelNames;
  std::vector<std::uint32_t> sampleRates;
  std::vector<std::uint32_t> bufferSizes;
  bool defaultInput{};
  bool defaultOutput{};
};

struct AudioBackendDescriptor
{
  std::string id;
  std::string name;
  bool preferred{};
  std::vector<AudioDeviceDescriptor> devices;
};

struct SharedSettings
{
  std::optional<std::string> namFolder;
  std::optional<std::string> irFolder;
};

struct StandaloneSettings
{
  std::optional<AudioConfiguration> audio;
  SetupState setup;
};

struct SettingsDocument
{
  std::uint32_t schemaVersion{kSettingsSchemaVersion};
  SharedSettings shared;
  StandaloneSettings standalone;
};

enum class WizardPage
{
  Welcome,
  AudioDevice,
  InputOutputTest,
  NamFolder,
  IrFolder,
  ManagedLibrary,
  Tone3000,
  Summary,
  Finish
};

struct SetupDraft
{
  std::optional<AudioConfiguration> audio;
  std::optional<std::string> namFolder;
  std::optional<std::string> irFolder;
  bool namFolderSkipped{};
  bool irFolderSkipped{};
  bool libraryAvailable{};
  bool libraryValidated{};
  bool tone3000Configured{};
  bool tone3000Skipped{};
};

const char* ToString(SetupCompletionState value);
std::optional<SetupCompletionState> ParseSetupCompletionState(const std::string& value);
const char* ToString(WizardPage value);
} // namespace amphibia::setup
