#!/usr/bin/env python3
"""Static integration contracts for Milestone 6 setup ownership and RT safety."""

from __future__ import annotations

import pathlib
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    source = read("NeuralAmpModeler/NeuralAmpModeler.cpp")
    controls = read("NeuralAmpModeler/NeuralAmpModelerControls.h")
    app_main = read("NeuralAmpModeler/Standalone/Windows/AmphibiaAPPMain.cpp")
    wizard = read("NeuralAmpModeler/Standalone/Windows/SetupWizardWin.cpp")
    settings = read("NeuralAmpModeler/Standalone/SettingsStore.cpp")
    app_project = read("NeuralAmpModeler/projects/NeuralAmpModeler-app.vcxproj")
    vst3_project = read("NeuralAmpModeler/projects/NeuralAmpModeler-vst3.vcxproj")

    require("SetupWizardWin.cpp" in app_project, "standalone project lost the setup wizard")
    require("SetupWizardWin.cpp" not in vst3_project and "AmphibiaAPPMain.cpp" not in vst3_project,
            "VST3 project gained standalone wizard/startup code")
    require("NAMConfigurationPanelControl" in controls, "explicit VST3 configuration panel is missing")
    require("Audio devices, channel routing, sample rate, and buffer size are controlled by your DAW." in controls,
            "VST3 no longer states DAW audio ownership")
    require("Choose NAM folder" in controls and "Choose IR folder" in controls,
            "shared local-folder controls are missing")
    require("UpdateSharedSettings" in source and "UpdateShared" in settings,
            "VST3 shared-folder merge boundary is missing")
    require("UpdateStandalone" not in controls, "VST3 panel can write standalone settings")

    require("ReadOptions()" in app_main and app_main.index("ReadOptions()") < app_main.index("IPlugAPPHost::Create()"),
            "command line is no longer parsed before host/audio initialization")
    for option in ("--first-run", "--setup", "--audio-settings", "--safe-mode", "--help", "--version"):
        require(option in read("NeuralAmpModeler/Standalone/CommandLine.cpp"), f"missing command option {option}")
    require("ShowSetupWizardWin" in app_main, "standalone launch policy lost wizard entry")
    require("ShowSetupWizardWin" not in source, "plug-in construction can launch the standalone wizard")

    require("Open Library Folder" in wizard and "Verify Library" in wizard and "Clean Staging" in wizard,
            "library maintenance actions are incomplete")
    require("Import Files" in wizard and "Import Folder" in wizard and "Import ZIP" in wizard,
            "library import actions are incomplete")
    require("Cancel Library Task" in wizard, "library work lacks explicit cancellation")
    require("TONE3000 integration is not configured in this build" in wizard,
            "unconfigured provider state is not explained honestly")

    process = source[source.index("void Amphibia::ProcessBlock"): source.index("void Amphibia::OnReset")]
    for forbidden in ("SettingsStore", "LoadSharedSettings", "ValidateLocalFolder", "VerifyManagedLibrary",
                      "ScanManagedLibrarySources", "ShowSetupWizardWin", "AudioDiagnostics", "ParseCommandLine"):
        require(forbidden not in process, f"{forbidden} became reachable from ProcessBlock")
    require("mTestTone.ProcessNonInterleaved" in read("NeuralAmpModeler/Standalone/Windows/AmphibiaAPPHost.cpp"),
            "bounded callback test generator is not wired")

    writer = source[source.index("bool Amphibia::SerializeState"): source.index("int Amphibia::UnserializeState")]
    for forbidden in ("setup", "AudioConfiguration", "namFolder", "irFolder", "tone3000Configured"):
        require(forbidden not in writer, f"application setup field {forbidden} entered DAW project state")
    for secret in ("access_token", "refresh_token", "signed_url", "client_secret"):
        require(secret in settings, f"settings sensitive-data guard omitted {secret}")

    print("Milestone 6 setup/VST3/real-time contract: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, ValueError) as error:
        print(f"Milestone 6 setup/VST3/real-time contract: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
