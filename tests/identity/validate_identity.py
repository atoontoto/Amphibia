#!/usr/bin/env python3
"""Static Milestone 1 identity-contract checks with no third-party dependencies."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
PRODUCT = ROOT / "NeuralAmpModeler"


def read(relative: str) -> str:
    path = ROOT / relative
    if not path.is_file():
        raise AssertionError(f"missing required file: {relative}")
    return path.read_text(encoding="utf-8", errors="strict")


def require(text: str, needle: str, location: str) -> None:
    if needle not in text:
        raise AssertionError(f"{location}: missing {needle!r}")


def reject(text: str, needle: str, location: str) -> None:
    if needle in text:
        raise AssertionError(f"{location}: inherited identity remains: {needle!r}")


def main() -> int:
    config = read("NeuralAmpModeler/config.h")
    expected = (
        '#define PLUG_NAME "Amphibia"',
        '#define PLUG_MFR "Amphibia Project"',
        '#define PLUG_VERSION_HEX 0x00000100',
        '#define PLUG_VERSION_STR "0.1.0"',
        "#define PLUG_UNIQUE_ID 'AmPh'",
        "#define PLUG_MFR_ID 'AmBi'",
        '#define PLUG_CLASS_NAME Amphibia',
        '#define BUNDLE_NAME "Amphibia"',
        '#define SHARED_RESOURCES_SUBPATH "Amphibia"',
        '#define VST3_PROCESSOR_UID 0x7DBF8585, 0x2FC54817, 0xAE21F791, 0x0D0330C1',
        '#define VST3_CONTROLLER_UID 0x893C1354, 0xA5D5416D, 0xB84CCA8A, 0xE0C27034',
        '#define AMPHIBIA_STATE_VERSION "1.1.0"',
    )
    for value in expected:
        require(config, value, "config.h")
    for value in ("'1YEo'", "'SDAa'", "com.StevenAtkinson"):
        reject(config, value, "config.h")

    source = read("NeuralAmpModeler/NeuralAmpModeler.cpp")
    require(source, 'WDL_String header("###Amphibia###");', "state writer")
    require(source, 'const char* kAmphibiaHeader = "###Amphibia###";', "state reader")
    require(source, 'const char* kLegacyNamHeader = "###NeuralAmpModeler###";', "legacy state reader")
    require(source, "chunk.PutStr(header.Get());", "state writer")

    solution = read("NeuralAmpModeler/Amphibia.sln")
    for name in ("Amphibia-app", "Amphibia-vst3", "Amphibia-aax"):
        require(solution, name, "Amphibia.sln")
    for old_guid in (
        "41785AE4-5B70-4A75-880B-4B418B4E13C6",
        "079FC65A-F0E5-4E97-B318-A16D1D0B89DF",
        "DC4FEE2C-FA85-4139-B6F3-310EE928F46B",
    ):
        reject(solution, old_guid, "Amphibia.sln")

    props = read("NeuralAmpModeler/config/NeuralAmpModeler-win.props")
    require(props, "<BINARY_NAME>Amphibia</BINARY_NAME>", "Windows props")
    installer = read("NeuralAmpModeler/installer/Amphibia.iss")
    for value in ("AppName=Amphibia", "Amphibia.exe", "Amphibia.vst3"):
        require(installer, value, "Amphibia.iss")
    reject(installer, "NeuralAmpModeler.aaxplugin", "Amphibia.iss")

    build_files = [
        *PRODUCT.glob("resources/*.plist"),
        *PRODUCT.glob("projects/*.vcxproj"),
        *PRODUCT.glob("projects/**/*.xcscheme"),
        PRODUCT / "resources/main.rc",
    ]
    for path in build_files:
        text = path.read_text(encoding="utf-8", errors="strict")
        reject(text, "com.StevenAtkinson", str(path.relative_to(ROOT)))
        reject(text, 'BuildableName = "NeuralAmpModeler', str(path.relative_to(ROOT)))

    rc = read("NeuralAmpModeler/resources/main.rc")
    require(rc, 'VALUE "ProductName", "Amphibia"', "Windows version resource")
    require(rc, 'VALUE "OriginalFilename", AMPHIBIA_ORIGINAL_FILENAME', "Windows version resource")
    reject(rc, "NeuralAmpModeler.ico", "Windows version resource")

    rename_map = read("docs/MILESTONE_1_RENAME_MAP.md")
    uuids = re.findall(r"[0-9A-F]{8}(?:-[0-9A-F]{4}){3}-[0-9A-F]{12}", rename_map)
    if len(uuids) != len(set(uuids)):
        raise AssertionError("rename map contains duplicate UUIDs")

    print("Amphibia Milestone 1 identity validation passed")
    print(f"checked {len(build_files) + 7} identity-bearing files")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"identity validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
