#!/usr/bin/env python3
"""Packaging source checks and optional generated-artifact verification."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path, PurePosixPath
import re
import zipfile


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "package-windows.ps1"
VERSION = "0.1.0"

FORBIDDEN_SUFFIXES = {
    ".pdb", ".ilk", ".lib", ".exp", ".nam", ".wav", ".pfx", ".p12",
    ".key", ".pem", ".log",
}
FORBIDDEN_PARTS = {"debug", "test", "tests", "model", "models", "ir", "irs",
                   "library", "cache", "log", "logs"}


def assert_source_contract() -> None:
    text = SCRIPT.read_text(encoding="utf-8")
    for fragment in (
        "$script:ExpectedInnoVersion = '6.7.3'",
        "PLUG_VERSION_STR",
        "Release packaging requires a clean worktree",
        "Amphibia-app:Rebuild",
        "Amphibia-vst3:Rebuild",
        "/p:PostBuildEventUseInBuild=false",
        "8664 machine \\(x64\\)",
        "AMPHIBIA_SIGN_CERT_SHA1",
        "signtool.exe",
        "SOURCE-COMMIT.txt",
        "DEPENDENCY-VERSIONS.txt",
        "ARTIFACT-MANIFEST.txt",
        "SHA256SUMS.txt",
        "Get-FileHash",
        "THIRD_PARTY_NOTICES.txt",
        "UNVERIFIED-DEVELOPMENT.txt",
        "test_installer.py",
        "test_packaging.py",
    ):
        assert fragment in text, f"packaging contract missing: {fragment}"
    assert "NeuralAmpModeler-installer-bg.png" not in text
    assert "PlugIn.ico" not in text
    assert "Desktop.ini" not in text
    assert "RuntimeLibrary" not in text, "packaging must validate, not rewrite build inputs"


def safe_archive_name(name: str) -> bool:
    normalized = name.replace("\\", "/")
    path = PurePosixPath(normalized)
    if path.is_absolute() or re.match(r"^[A-Za-z]:/", normalized):
        return False
    lowered = {part.lower() for part in path.parts}
    if lowered & FORBIDDEN_PARTS:
        return False
    if path.suffix.lower() in FORBIDDEN_SUFFIXES:
        return False
    sensitive = normalized.lower()
    return not any(item in sensitive for item in (
        "settings-v1.json", "settings.ini", "credential", "token", "signed-url"
    ))


def verify_artifacts(root: Path) -> None:
    required = {
        f"Amphibia-{VERSION}-Windows-Setup.exe",
        f"Amphibia-{VERSION}-Windows-Portable.zip",
        f"Amphibia-{VERSION}-Windows-VST3.zip",
        f"Amphibia-{VERSION}-Windows-Standalone.zip",
        "SOURCE-COMMIT.txt",
        "DEPENDENCY-VERSIONS.txt",
        "THIRD_PARTY_NOTICES.txt",
        "ARTIFACT-MANIFEST.txt",
        "SHA256SUMS.txt",
    }
    missing = sorted(name for name in required if not (root / name).is_file())
    assert not missing, f"generated artifacts missing: {missing}"

    manifest = (root / "ARTIFACT-MANIFEST.txt").read_text(encoding="utf-8-sig")
    for name in required - {"SHA256SUMS.txt", "ARTIFACT-MANIFEST.txt"}:
        assert name in manifest

    checksums = {}
    for line in (root / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        assert match, f"invalid checksum line: {line}"
        checksums[match.group(2)] = match.group(1)
    for name, expected in checksums.items():
        actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
        assert actual == expected, f"checksum mismatch: {name}"

    for archive in root.glob("*.zip"):
        with zipfile.ZipFile(archive) as handle:
            names = handle.namelist()
            assert names, f"empty archive: {archive.name}"
            assert all(safe_archive_name(name) for name in names), archive.name
            if "VST3" in archive.name:
                assert "Amphibia.vst3/Contents/x86_64-win/Amphibia.vst3" in names


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()
    assert_source_contract()
    if args.artifacts:
        verify_artifacts(args.artifacts.resolve())
    print("Milestone 7 packaging contract: PASS")


if __name__ == "__main__":
    main()
