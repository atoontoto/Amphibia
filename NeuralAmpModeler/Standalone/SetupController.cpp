#include "SetupController.h"

#include "CommandLine.h"

#include <filesystem>

namespace amphibia::setup
{
SetupController::SetupController(SettingsDocument document, std::string appVersion)
: mDocument(std::move(document)), mAppVersion(std::move(appVersion))
{
  mDraft.audio = mDocument.standalone.audio;
  mDraft.namFolder = mDocument.shared.namFolder;
  mDraft.irFolder = mDocument.shared.irFolder;
  mDraft.tone3000Configured = mDocument.standalone.setup.tone3000Configured;
  if (mDocument.standalone.setup.completion != SetupCompletionState::Completed)
    mDocument.standalone.setup.completion = SetupCompletionState::InProgress;
}

bool SetupController::ValidatePage(WizardPage page, std::string& error) const
{
  if (page == WizardPage::AudioDevice && mDraft.audio && mDraft.audio->outputDeviceId.empty())
  {
    error = "Output device is unavailable.";
    return false;
  }
  if (page == WizardPage::NamFolder && mDraft.namFolder && !std::filesystem::is_directory(*mDraft.namFolder))
  {
    error = "The NAM folder no longer exists.";
    return false;
  }
  if (page == WizardPage::IrFolder && mDraft.irFolder && !std::filesystem::is_directory(*mDraft.irFolder))
  {
    error = "The IR folder no longer exists.";
    return false;
  }
  return true;
}

bool SetupController::Next(std::string& error)
{
  if (!ValidatePage(mPage, error)) return false;
  if (mPage == WizardPage::Finish) return false;
  mPage = static_cast<WizardPage>(static_cast<int>(mPage) + 1);
  return true;
}

bool SetupController::Back()
{
  if (mPage == WizardPage::Welcome) return false;
  mPage = static_cast<WizardPage>(static_cast<int>(mPage) - 1);
  return true;
}

void SetupController::Cancel(bool explicitlySkip)
{
  mDocument.standalone.setup.completion = explicitlySkip ? SetupCompletionState::Skipped
                                                          : SetupCompletionState::InProgress;
}

bool SetupController::Finish(SettingsStore& store, std::int64_t unixSeconds, std::string& error)
{
  if (mDraft.audio && mDraft.audio->outputDeviceId.empty())
  {
    error = "Output device is unavailable.";
    return false;
  }
  mDocument.shared.namFolder = mDraft.namFolder;
  mDocument.shared.irFolder = mDraft.irFolder;
  mDocument.standalone.audio = mDraft.audio;
  auto& state = mDocument.standalone.setup;
  state.audioConfigured = mDraft.audio.has_value();
  state.namFolderConfigured = mDraft.namFolder.has_value();
  state.irFolderConfigured = mDraft.irFolder.has_value();
  state.libraryValidated = mDraft.libraryValidated;
  state.tone3000Configured = mDraft.tone3000Configured;
  state.lastCompletedAppVersion = mAppVersion;
  state.completedAtUnixSeconds = unixSeconds;
  state.completion = SetupCompletionState::Completed;
  return store.Save(mDocument, error);
}

bool ShouldShowWizard(const SetupState& state, const CommandLineOptions& options, bool explicitRerun,
                      bool criticalConfigurationInvalid, bool userAcceptedRepair)
{
  if (options.forceFirstRun || options.openSetup || options.openAudioSettings || explicitRerun) return true;
  if (criticalConfigurationInvalid && userAcceptedRepair) return true;
  return state.completion == SetupCompletionState::NeverStarted;
}
} // namespace amphibia::setup
