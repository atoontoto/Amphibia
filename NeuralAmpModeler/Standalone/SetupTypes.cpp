#include "SetupTypes.h"

namespace amphibia::setup
{
const char* ToString(SetupCompletionState value)
{
  switch (value)
  {
    case SetupCompletionState::NeverStarted: return "never_started";
    case SetupCompletionState::InProgress: return "in_progress";
    case SetupCompletionState::Completed: return "completed";
    case SetupCompletionState::Skipped: return "skipped";
  }
  return "never_started";
}

std::optional<SetupCompletionState> ParseSetupCompletionState(const std::string& value)
{
  if (value == "never_started") return SetupCompletionState::NeverStarted;
  if (value == "in_progress") return SetupCompletionState::InProgress;
  if (value == "completed") return SetupCompletionState::Completed;
  if (value == "skipped") return SetupCompletionState::Skipped;
  return std::nullopt;
}

const char* ToString(WizardPage value)
{
  switch (value)
  {
    case WizardPage::Welcome: return "Welcome";
    case WizardPage::AudioDevice: return "Audio device";
    case WizardPage::InputOutputTest: return "Input/output test";
    case WizardPage::NamFolder: return "Local NAM folder";
    case WizardPage::IrFolder: return "Local IR folder";
    case WizardPage::ManagedLibrary: return "Managed library";
    case WizardPage::Tone3000: return "TONE3000";
    case WizardPage::Summary: return "Summary";
    case WizardPage::Finish: return "Finish";
  }
  return "Welcome";
}
} // namespace amphibia::setup
