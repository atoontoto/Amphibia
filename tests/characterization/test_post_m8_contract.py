from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    controls = read("NeuralAmpModeler/NeuralAmpModelerControls.h")
    product = read("NeuralAmpModeler/NeuralAmpModeler.cpp")
    configuration = read(
        "NeuralAmpModeler/Providers/Tone3000/Tone3000Configuration.cpp"
    )
    receiver = read(
        "NeuralAmpModeler/Platform/FixedLoopbackCallbackReceiver.cpp"
    )
    provider_types = read("NeuralAmpModeler/Providers/ProviderTypes.h")
    layout = read("NeuralAmpModeler/UI/EditorLayout.h")
    app_project = read("NeuralAmpModeler/projects/NeuralAmpModeler-app.vcxproj")
    vst3_project = read("NeuralAmpModeler/projects/NeuralAmpModeler-vst3.vcxproj")
    standalone_dialog = read(
        "NeuralAmpModeler/Standalone/Windows/AmphibiaAPPDialog.cpp"
    )

    for caption in (
        "Search TONE3000",
        "Find IR on TONE3000",
        "Connect",
        "Disconnect",
        "Test Connection",
        "Cancel Current Operation",
        "Open TONE3000",
    ):
        require(caption in product + controls + layout, f"missing visible caption: {caption}")
    for provider_line in (
        "Application registration: Development",
        "Callback: Fixed local callback",
        "Status: Disconnected",
    ):
        require(provider_line in controls, f"missing bounded provider status line: {provider_line}")
    require(
        "WithShowLabel(false)" not in product + controls,
        "a relevant vector control still suppresses its label",
    )

    for section in (
        "General",
        "Local Files",
        "Managed Library",
        "TONE3000",
        "Calibration",
        "Diagnostics",
        "About",
    ):
        require(section in controls + layout, f"missing Settings section: {section}")
    require(
        "ApplySectionVisibility" in controls
        and "mPages[index]->SetDisabled(inactive)" in controls
        and "mPages[index]->Hide(inactive)" in controls,
        "inactive Settings sections are not hidden and disabled",
    )
    require(
        "mPages[index]->ForAllChildrenFunc([inactive]" in controls
        and "child->SetDisabled(inactive)" in controls
        and "child->Hide(inactive)" in controls,
        "inactive Settings page descendants remain visible or interactive",
    )
    require(
        "IContainerBase::Hide(mWillHide);" in controls
        and "if (!mWillHide) ApplySectionVisibility();" in controls,
        "Settings animation completion can unhide inactive pages",
    )
    require(
        "CalculateMainEditorLayout" in product
        and "CalculateSettingsLayout" in controls
        and "struct LayoutMetrics" in layout,
        "central layout model is not exercised by both surfaces",
    )
    require(
        "Audio devices, sample rate, and buffer size are controlled by your DAW."
        in controls,
        "VST3 DAW audio-ownership text is missing",
    )
    require(
        "Standalone audio settings are configured from File > Audio Settings."
        in controls,
        "standalone audio-ownership text is missing",
    )
    require(
        "GetScaleForHWND(hwndDlg)" in standalone_dialog
        and "static_cast<float>(width) * scale" in standalone_dialog
        and "static_cast<float>(w) * scale" in standalone_dialog,
        "standalone client size is not DPI-scaled at creation and DPI changes",
    )
    require(
        "FindWindowExA(hWnd, nullptr, \"IPlugWndClass\", nullptr)"
        in standalone_dialog
        and standalone_dialog.count("EditorChildResize(") >= 4,
        "standalone iPlug child does not track its DPI-scaled client",
    )

    source_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore")
        for path in (ROOT / "NeuralAmpModeler").rglob("*")
        if path.is_file() and path.suffix.lower() in {".h", ".cpp", ".vcxproj", ".props"}
    )
    public_client_assignments = re.findall(
        r'configuration\.clientId\s*=\s*"(t3k_pub_[A-Za-z0-9]+)"',
        source_text,
    )
    require(
        len(public_client_assignments) == 1,
        "publishable client configuration is missing or duplicated",
    )
    require(
        'clientId = "t3k_cs' not in configuration,
        "secret-key value entered configuration",
    )
    require(
        'configuration.kind = Tone3000ConfigurationKind::Development' in configuration
        and "Tone3000CallbackStrategy::FixedLoopback" in configuration,
        "official registration is not development fixed-loopback",
    )
    exact_redirect = "http://127.0.0.1:43821/oauth/callback"
    require(exact_redirect in configuration, "exact redirect is missing from configuration")
    require(
        "127.0.0.1:43821" in receiver
        and "INADDR_LOOPBACK" in receiver
        and "SO_EXCLUSIVEADDRUSE" in receiver,
        "listener is not exclusive IPv4 loopback",
    )
    require(
        "kCallbackPort = 43821" in receiver
        and 'kCallbackPath[] = "/oauth/callback"' in receiver,
        "listener port/path changed",
    )
    require("INADDR_ANY" not in receiver, "listener binds a wildcard address")
    require(
        "Cache-Control: no-store" in receiver
        and "Authorization received. You may return to Amphibia." in receiver,
        "minimal no-store browser response is missing",
    )
    require(
        "LocalPortInUse" in provider_types
        and "already in use" in receiver
        and "kMB_RETRYCANCEL" in product,
        "deterministic port-conflict Retry/Cancel behavior is missing",
    )
    require(
        "FixedLoopbackCallbackReceiver.cpp" in app_project
        and "FixedLoopbackCallbackReceiver.cpp" in vst3_project,
        "fixed callback receiver is not compiled into both Windows products",
    )
    require(
        "mTone3000Selection->Begin" in product
        and "HostedSelectionKind::ImpulseResponse" in product
        and "Tone3000HostedSelectionCoordinator" in product,
        "main hosted NAM/IR actions are not live-wired",
    )
    require(
        "ProcessBlock" not in receiver,
        "callback receiver entered the audio callback",
    )

    print("Post-M8 UI/TONE3000 static contract checks passed")


if __name__ == "__main__":
    main()
