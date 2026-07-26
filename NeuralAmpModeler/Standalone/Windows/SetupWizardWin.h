#pragma once

#include "../SetupTypes.h"

#include <windows.h>

namespace iplug { class IPlugAPPHost; }

namespace amphibia::setup
{
// Runs a native modal message loop owned by the standalone main window. The
// VST3 project does not compile or reference this function.
bool ShowSetupWizardWin(HWND parent, iplug::IPlugAPPHost& host, WizardPage initialPage = WizardPage::Welcome);
} // namespace amphibia::setup
