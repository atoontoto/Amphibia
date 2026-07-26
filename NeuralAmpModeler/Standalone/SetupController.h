#pragma once

#include "SettingsStore.h"

#include <cstdint>
#include <string>

namespace amphibia::setup
{
struct CommandLineOptions;

class SetupController
{
public:
  SetupController(SettingsDocument document, std::string appVersion);

  WizardPage Page() const { return mPage; }
  const SetupDraft& Draft() const { return mDraft; }
  SetupDraft& Draft() { return mDraft; }
  const SettingsDocument& Document() const { return mDocument; }

  bool Next(std::string& error);
  bool Back();
  void GoTo(WizardPage page) { mPage = page; }
  void Cancel(bool explicitlySkip);
  bool Finish(SettingsStore& store, std::int64_t unixSeconds, std::string& error);

private:
  bool ValidatePage(WizardPage page, std::string& error) const;

  SettingsDocument mDocument;
  SetupDraft mDraft;
  WizardPage mPage{WizardPage::Welcome};
  std::string mAppVersion;
};

bool ShouldShowWizard(const SetupState& state, const CommandLineOptions& options, bool explicitRerun,
                      bool criticalConfigurationInvalid, bool userAcceptedRepair);
void SeedLegacyAudio(SettingsDocument& document, const std::optional<AudioConfiguration>& activeLegacyAudio);
} // namespace amphibia::setup
