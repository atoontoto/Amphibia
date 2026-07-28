from pathlib import Path


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
        "CalculateMainEditorLayout" in product
        and "CalculateSettingsLayout" in controls
        and "struct LayoutMetrics" in layout,
        "central layout model is not exercised by both surfaces",
    )
    require(
        "Audio devices, channel routing, sample rate, and buffer size are controlled by your DAW."
        in controls,
        "VST3 DAW audio-ownership text is missing",
    )
    require(
        "Standalone audio devices, channels, sample rate, and buffer size are configured in Audio Settings."
        in controls,
        "standalone audio-ownership text is missing",
    )

    public_id = "t3k_pub_kQRhztgEo6BAZeTI1o0iJmOurV7PdeUV"
    source_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore")
        for path in (ROOT / "NeuralAmpModeler").rglob("*")
        if path.is_file() and path.suffix.lower() in {".h", ".cpp", ".vcxproj", ".props"}
    )
    require(source_text.count(public_id) == 1, "publishable client ID is duplicated")
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
