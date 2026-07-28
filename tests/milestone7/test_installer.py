#!/usr/bin/env python3
"""Static contract checks for the Amphibia Windows installer."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
INSTALLER = ROOT / "NeuralAmpModeler" / "installer" / "Amphibia.iss"


def require(text: str, fragment: str) -> None:
    assert fragment in text, f"installer contract missing: {fragment}"


def main() -> None:
    text = INSTALLER.read_text(encoding="utf-8")

    for fragment in (
        '#define MyAppId "802D7F30-6B3A-44C3-B809-93DA217821CA"',
        "AppId={{{#MyAppId}}",
        "DefaultDirName={autopf}\\Amphibia",
        "PrivilegesRequired=admin",
        "ArchitecturesAllowed=x64os",
        "ArchitecturesInstallIn64BitMode=x64os",
        "MinVersion=10.0.10240",
        'Name: "standalone"',
        'Name: "vst3"',
        'Name: "desktopicon"',
        'Flags: unchecked',
        "{commoncf64}\\VST3\\Amphibia.vst3",
        'Parameters: "--first-run"',
        "skipifsilent runasoriginaluser",
        "ComparePackedVersion",
        "A newer Amphibia version",
        "UninstallSilent",
        "MB_DEFBUTTON2",
        "{localappdata}\\Amphibia",
        "FILE_ATTRIBUTE_REPARSE_POINT",
        "org.amphibiaaudio.Amphibia.TONE3000/oauth-v1",
        "CredDeleteW@advapi32.dll",
        "This does not remove your original NAM folders",
        "ChangesAssociations=no",
        "ChangesEnvironment=no",
        "RestartIfNeededByRun=no",
    ):
        require(text, fragment)

    assert "NeuralAmpModeler" not in text
    assert "Desktop.ini" not in text
    assert "PlugIn.ico" not in text
    assert "Flags: unchecked" in re.search(
        r'\[Tasks\](.*?)(?:\n\[|\Z)', text, re.DOTALL
    ).group(1)

    files = re.search(r"\[Files\](.*?)(?:\n\[|\Z)", text, re.DOTALL).group(1)
    sources = re.findall(r'^Source:\s*"([^"]+)"', files, re.MULTILINE)
    assert sources, "installer has no file inputs"
    assert all(source.startswith("{#StageRoot}\\") for source in sources)
    assert not any("*" in source for source in sources if "Documentation" not in source)

    print("Milestone 7 installer contract: PASS")


if __name__ == "__main__":
    main()
