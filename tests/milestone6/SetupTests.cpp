#include "../../NeuralAmpModeler/Standalone/AudioConfiguration.h"
#include "../../NeuralAmpModeler/Standalone/CommandLine.h"
#include "../../NeuralAmpModeler/Standalone/FolderValidation.h"
#include "../../NeuralAmpModeler/Standalone/SettingsStore.h"
#include "../../NeuralAmpModeler/Standalone/SetupController.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <windows.h>

using namespace amphibia::setup;

namespace
{
void Require(bool value, const std::string& message)
{
  if (!value) throw std::runtime_error(message);
}

std::filesystem::path TemporaryRoot(const char* name)
{
  const auto path = std::filesystem::temp_directory_path() /
                    (std::string("amphibia-m6-") + name + '-' + std::to_string(GetCurrentProcessId()));
  std::error_code code;
  std::filesystem::remove_all(path, code);
  std::filesystem::create_directories(path);
  return path;
}

std::string Utf8(const std::filesystem::path& path)
{
  const auto value = path.u8string();
  return {value.begin(), value.end()};
}

AudioConfiguration Configuration(const char* output = "out-1")
{
  AudioConfiguration value;
  value.backendId = "asio";
  value.backendName = "ASIO";
  value.inputDeviceId = "in-1";
  value.inputDeviceName = "Input";
  value.outputDeviceId = output;
  value.outputDeviceName = "Output";
  value.inputChannel = 1;
  value.outputFirstChannel = 1;
  value.outputChannelCount = 2;
  value.sampleRate = 48000;
  value.bufferSize = 128;
  return value;
}

class FakeAudioService final : public IAudioConfigurationService
{
public:
  std::vector<AudioBackendDescriptor> Enumerate() override { ++enumerateCalls; return inventory; }
  std::optional<AudioConfiguration> Current() const override { return current; }
  std::optional<std::string> Validate(const AudioConfiguration& requested) const override
  {
    if (invalid || (requested.backendId != "asio" && requested.backendId != "directsound"))
      return "Audio backend is unavailable.";
    if (requested.inputDeviceId == "missing-input") return "Input device is unavailable.";
    if (requested.outputDeviceId.empty()) return "Output device is unavailable.";
    if (requested.sampleRate == 12345) return "Selected sample rate is not supported.";
    if (requested.bufferSize == 13) return "Selected buffer size is not supported.";
    return std::nullopt;
  }
  AudioOpenResult Open(const AudioConfiguration& requested) override
  {
    opens.push_back(requested);
    if (requested.outputDeviceId == failingOutput)
      return {false, false, false, "Audio device could not be opened."};
    if (failEveryOpen) return {false, false, false, "Audio device could not be opened."};
    current = requested;
    return {true, callbackActive, outputAvailable, {}};
  }
  void Close() override { current.reset(); closed = true; }
  void StopTestTone() override { testStopped = true; }

  std::optional<AudioConfiguration> current;
  std::vector<AudioBackendDescriptor> inventory;
  std::vector<AudioConfiguration> opens;
  std::string failingOutput;
  bool invalid{};
  bool failEveryOpen{};
  bool callbackActive{true};
  bool outputAvailable{true};
  bool closed{};
  bool testStopped{};
  int enumerateCalls{};
};

void TestCommandLineAndDetection()
{
  const auto parsed = ParseCommandLine({"--first-run", "--safe-mode", "--audio-settings"});
  Require(parsed.forceFirstRun && parsed.safeMode && parsed.openAudioSettings, "known options were not parsed");
  Require(!parsed.IsTerminal(), "ordinary setup options became terminal");
  const auto unknown = ParseCommandLine({"model.nam", "--token=secret"});
  Require(unknown.unknownOptions.size() == 2 && unknown.IsTerminal(), "unknown arguments were interpreted");
  Require(CommandLineHelp().find("--setup") != std::string::npos, "bounded help omitted setup");
  Require(VersionText("0.1.0", "abc").find("abc") != std::string::npos, "version omitted commit");

  SetupState state;
  Require(ShouldShowWizard(state, {}, false, false, false), "never-run setup did not show");
  state.completion = SetupCompletionState::Completed;
  Require(!ShouldShowWizard(state, {}, false, false, false), "completed setup showed automatically");
  state.completion = SetupCompletionState::Skipped;
  Require(!ShouldShowWizard(state, {}, false, false, false), "skipped setup repeated in session");
  Require(ShouldShowWizard(state, ParseCommandLine({"--setup"}), false, false, false), "forced setup failed");
  Require(ShouldShowWizard(state, ParseCommandLine({"--first-run"}), false, false, false), "forced first run failed");
  Require(ShouldShowWizard(state, {}, true, false, false), "rerun setup failed");
  Require(!ShouldShowWizard(state, {}, false, true, false), "repair appeared without consent");
  Require(ShouldShowWizard(state, {}, false, true, true), "accepted repair did not show");
}

void TestSettingsRoundTripRecoveryAndFutureSchema()
{
  const auto root = TemporaryRoot("settings");
  SettingsStore store(root / "settings-v1.json");
  auto missing = store.Load();
  Require(missing.status == SettingsLoadStatus::Missing, "missing setup state was not safe");

  SettingsDocument document;
  document.shared.namFolder = Utf8(root / "Models");
  document.shared.irFolder = Utf8(root / "IRs");
  document.standalone.audio = Configuration();
  document.standalone.setup.completion = SetupCompletionState::Completed;
  document.standalone.setup.audioConfigured = true;
  document.standalone.setup.lastCompletedAppVersion = "0.1.0";
  document.standalone.setup.completedAtUnixSeconds = 123;
  std::string error;
  Require(store.Save(document, error), error);
  auto loaded = store.Load();
  Require(loaded.status == SettingsLoadStatus::Loaded, "settings did not load");
  Require(loaded.document.shared.namFolder == document.shared.namFolder, "NAM folder did not round trip");
  Require(loaded.document.standalone.audio == document.standalone.audio, "audio did not round trip");
  Require(loaded.document.standalone.setup.completion == SetupCompletionState::Completed,
          "completion did not round trip");

  document.shared.namFolder = "second";
  Require(store.Save(document, error), error);
  {
    std::ofstream corrupt(store.Path(), std::ios::trunc);
    corrupt << "{corrupt";
  }
  loaded = store.Load();
  Require(loaded.status == SettingsLoadStatus::RecoveredFromBackup, "corrupt primary did not use backup");
  Require(loaded.document.shared.namFolder && *loaded.document.shared.namFolder != "second",
          "backup was not the previous validated settings");

  {
    std::ofstream future(store.Path(), std::ios::trunc);
    future << "{\"schema_version\":999,\"shared\":{},\"standalone\":{}}";
  }
  loaded = store.Load();
  Require(loaded.status == SettingsLoadStatus::FutureSchema, "future settings schema was not rejected");
  Require(!store.UpdateShared({}, error), "future settings were overwritten");
  Require(ContainsSensitiveSetupData("{\"access_token\":\"canary\"}"), "token canary was not rejected");
  Require(ContainsSensitiveSetupData("{\"signed_url\":\"https://example.invalid\"}"), "signed URL canary was not rejected");
  Require(!ContainsSensitiveSetupData("{\"tone3000_configured\":false}"), "ordinary provider status was rejected");

  SettingsStore corruptOnly(root / "corrupt-only.json");
  { std::ofstream corrupt(corruptOnly.Path()); corrupt << "not-json"; }
  Require(corruptOnly.Load().status == SettingsLoadStatus::Corrupt, "corrupt settings without backup were accepted");
  std::error_code code;
  std::filesystem::remove_all(root, code);
}

void TestSetupNavigationCancelAndFinish()
{
  const auto root = TemporaryRoot("controller");
  const auto nam = root / "NAM";
  const auto ir = root / "IR";
  std::filesystem::create_directories(nam);
  std::filesystem::create_directories(ir);
  SettingsStore store(root / "settings-v1.json");
  SettingsDocument document;
  SeedLegacyAudio(document, Configuration("legacy"));
  Require(document.standalone.audio == Configuration("legacy") && document.standalone.setup.audioConfigured,
          "active legacy INI audio was not seeded into setup");
  SetupController controller(document, "0.1.0");
  Require(controller.Page() == WizardPage::Welcome, "wizard did not start at Welcome");
  std::string error;
  Require(controller.Next(error) && controller.Page() == WizardPage::AudioDevice, "Next failed");
  controller.Draft().audio = Configuration();
  Require(controller.Next(error) && controller.Back(), "Back/Next failed");
  Require(controller.Draft().audio.has_value(), "page state was not retained");
  controller.Cancel(false);
  Require(controller.Document().standalone.setup.completion == SetupCompletionState::InProgress,
          "cancel marked setup complete");
  controller.Cancel(true);
  Require(controller.Document().standalone.setup.completion == SetupCompletionState::Skipped,
          "explicit skip was not recorded");

  controller.Draft().namFolder = Utf8(nam);
  controller.Draft().irFolder = Utf8(ir);
  controller.Draft().libraryAvailable = true;
  controller.Draft().libraryValidated = true;
  controller.Draft().tone3000Skipped = true;
  Require(controller.Finish(store, 999, error), error);
  const auto loaded = store.Load();
  Require(loaded.document.standalone.setup.completion == SetupCompletionState::Completed,
          "Finish did not commit completion");
  Require(loaded.document.standalone.setup.completedAtUnixSeconds == 999, "Finish time missing");
  Require(!loaded.document.standalone.setup.tone3000Configured, "skipped TONE3000 became an error/configured");
  const auto settingsBytes = [&] { std::ifstream stream(store.Path()); return std::string(
    std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()); }();
  Require(settingsBytes.find("test_tone") == std::string::npos && settingsBytes.find("project_state") == std::string::npos,
          "ephemeral or project state entered setup settings");
  std::error_code code;
  std::filesystem::remove_all(root, code);
}

void TestAudioInventoryAndValidation()
{
  FakeAudioService service;
  AudioBackendDescriptor backend;
  backend.id = "asio";
  backend.name = "ASIO";
  backend.preferred = true;
  AudioDeviceDescriptor first;
  first.id = "asio:7";
  first.name = "Interf\xC3\xA1" "ce";
  first.inputChannelNames = {"Input 1", "Input 2"};
  first.outputChannelNames = {"Output 1", "Output 2", "Output 3", "Output 4"};
  first.sampleRates = {44100, 48000};
  first.bufferSizes = {64, 128, 256};
  auto duplicate = first;
  duplicate.id = "asio:9";
  backend.devices = {first, duplicate};
  service.inventory = {backend};
  const auto initial = service.Enumerate();
  const auto refreshed = service.Enumerate();
  Require(service.enumerateCalls == 2 && initial.size() == refreshed.size() &&
          initial.front().devices.size() == refreshed.front().devices.size(), "device refresh was not repeatable");
  Require(initial.front().devices[0].name == initial.front().devices[1].name &&
          initial.front().devices[0].id != initial.front().devices[1].id,
          "duplicate display names were not distinguished by device IDs");

  auto value = Configuration();
  value.inputDeviceId.clear();
  value.inputDeviceName = "No input";
  value.inputChannel = 0;
  Require(!service.Validate(value), "output-only configuration was rejected");
  value.backendId = "unknown";
  Require(service.Validate(value) == "Audio backend is unavailable.", "invalid backend was accepted");
  value = Configuration();
  value.inputDeviceId = "missing-input";
  Require(service.Validate(value) == "Input device is unavailable.", "missing input was accepted");
  value = Configuration();
  value.outputDeviceId.clear();
  Require(service.Validate(value) == "Output device is unavailable.", "missing output was accepted");
  value = Configuration();
  value.bufferSize = 13;
  Require(service.Validate(value) == "Selected buffer size is not supported.", "unsupported buffer was accepted");
}

void TestAudioTransaction()
{
  FakeAudioService service;
  service.current = Configuration("working");
  AudioConfigurationTransaction transaction(service);
  auto result = transaction.Apply(Configuration("new"));
  Require(result.committed && result.active == Configuration("new"), "valid transaction did not commit");
  Require(service.testStopped, "device change did not stop the test tone");

  service.current = Configuration("working");
  service.failingOutput = "broken";
  result = transaction.Apply(Configuration("broken"));
  Require(!result.committed && result.rolledBack && result.active == Configuration("working"),
          "failed configuration did not restore previous");
  Require(service.opens.size() >= 3 && service.opens.back() == Configuration("working"),
          "rollback did not reopen the previous device");

  service.current.reset();
  service.failEveryOpen = true;
  result = transaction.Apply(Configuration("none"));
  Require(result.noAudio && service.closed, "missing previous configuration did not enter no-audio state");

  service.failEveryOpen = false;
  auto invalid = Configuration();
  invalid.sampleRate = 12345;
  const auto openCount = service.opens.size();
  result = transaction.Apply(invalid);
  Require(!result.committed && service.opens.size() == openCount, "unsupported rate reached device open");
}

void TestToneAndMeters()
{
  TestToneGenerator tone;
  Require(!tone.Active(), "test tone started automatically");
  Require(tone.Start(48000.0), "explicit test did not start");
  Require(!tone.Start(48000.0), "multiple clicks stacked test tones");
  std::vector<double> output(2 * 256, 0.0);
  tone.ProcessNonInterleaved(output.data(), 2, 256);
  double peak = 0.0;
  for (const auto value : output) peak = std::max(peak, std::abs(value));
  Require(peak > 0.0 && peak <= TestToneGenerator::kMaximumPeak + 1e-12, "test peak exceeded bound");
  Require(std::abs(output.front()) < 1e-12, "test tone did not fade in");
  tone.Stop();
  for (int i = 0; i < 8 && tone.Active(); ++i)
  {
    std::fill(output.begin(), output.end(), 0.0);
    tone.ProcessNonInterleaved(output.data(), 2, 256);
  }
  Require(!tone.Active(), "test tone did not stop after fade");

  AudioMeters meters;
  const double input[] = {0.1, -0.7, 0.2};
  const double measuredOutput[] = {0.2, -0.4, 0.3};
  meters.Observe(input, 1, measuredOutput, 1, 3);
  Require(std::abs(meters.InputPeak() - 0.7) < 1e-12 && std::abs(meters.OutputPeak() - 0.4) < 1e-12,
          "meters did not publish peaks");
  meters.Reset();
  Require(meters.InputPeak() == 0.0 && meters.OutputPeak() == 0.0, "meters did not reset");
}

void TestFolderValidation()
{
  const auto root = TemporaryRoot("folders");
  const auto folder = root / L"M\u00f6dels";
  const auto nested = folder / "nested";
  std::filesystem::create_directories(nested);
  std::ofstream(folder / "one.nam") << "x";
  std::ofstream(folder / "two.NAM") << "x";
  std::ofstream(folder / "not.txt") << "x";
  std::ofstream(nested / "nested.nam") << "x";
  auto result = ValidateLocalFolder(folder, ".nam");
  Require(result.status == FolderValidationStatus::Available && result.compatibleFiles == 2,
          "folder count was not same-directory and extension bounded");
  result = ValidateLocalFolder(folder / "one.nam", ".nam");
  Require(result.status == FolderValidationStatus::NotDirectory, "file was accepted as folder");
  result = ValidateLocalFolder(root / "missing", ".wav");
  Require(result.status == FolderValidationStatus::Missing, "missing folder was accepted");
  std::ofstream(folder / "cab.wav") << "x";
  result = ValidateLocalFolder(folder, ".wav", 1);
  Require(result.status == FolderValidationStatus::Available && result.compatibleFiles == 1 && result.countTruncated,
          "IR count was not bounded");
  std::filesystem::remove(folder / "cab.wav");
  const auto removed = root / "removed";
  std::filesystem::create_directories(removed);
  std::filesystem::remove(removed);
  Require(ValidateLocalFolder(removed, ".nam").status == FolderValidationStatus::Missing,
          "folder removal after selection was not detected");
  Require(std::filesystem::exists(folder / "one.nam") && std::filesystem::exists(nested / "nested.nam"),
          "folder validation modified source files");
  std::error_code code;
  std::filesystem::remove_all(root, code);
}
} // namespace

int main()
{
  const std::vector<std::pair<const char*, std::function<void()>>> tests{
    {"command line and detection", TestCommandLineAndDetection},
    {"settings round trip recovery and future schema", TestSettingsRoundTripRecoveryAndFutureSchema},
    {"setup navigation cancel and finish", TestSetupNavigationCancelAndFinish},
    {"audio transaction", TestAudioTransaction},
    {"audio inventory and validation", TestAudioInventoryAndValidation},
    {"test tone and meters", TestToneAndMeters},
    {"folder validation", TestFolderValidation}};
  int failed = 0;
  for (const auto& test : tests)
  {
    try { test.second(); std::cout << "[PASS] " << test.first << '\n'; }
    catch (const std::exception& error) { ++failed; std::cerr << "[FAIL] " << test.first << ": " << error.what() << '\n'; }
  }
  std::cout << (tests.size() - failed) << '/' << tests.size() << " Milestone 6 groups passed\n";
  return failed == 0 ? 0 : 1;
}
