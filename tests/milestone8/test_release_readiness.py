#!/usr/bin/env python3
"""Milestone 8 release-evidence, supply-chain, and hardening contracts."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
EVIDENCE_ID = "RC-EVIDENCE-2026-07-28"
SBOM_NAME = "Amphibia-0.1.0-SBOM.cdx.json"
ASSET_NAME = "ASSET-PROVENANCE.json"


def run(*arguments: str) -> None:
    subprocess.run(arguments, cwd=ROOT, check=True)


def source_contract() -> None:
    config = (ROOT / "NeuralAmpModeler" / "config.h").read_text(encoding="utf-8")
    identifiers = (ROOT / "docs" / "IDENTIFIERS.md").read_text(encoding="utf-8")
    dsp = (ROOT / "docs" / "DSP_CHARACTERIZATION.md").read_text(encoding="utf-8")
    validation_map = (
        ROOT / "docs" / "MILESTONE_8_RELEASE_VALIDATION_MAP.md"
    ).read_text(encoding="utf-8")
    assert '#define AMPHIBIA_STATE_VERSION "1.1.0"' in config
    assert "| State layout version | `1.1.0`" in identifiers
    assert "Current state is `###Amphibia###`, version `1.1.0`" in dsp
    assert EVIDENCE_ID in validation_map
    assert "Amphibia 0.1.0 RC validation build" in validation_map
    assert "blocked from public release" in validation_map

    props = (
        ROOT / "NeuralAmpModeler" / "config" / "NeuralAmpModeler-win.props"
    ).read_text(encoding="utf-8-sig")
    for fragment in (
        "<ControlFlowGuard Condition=\"'$(Configuration)'=='Release'\">Guard",
        "/PDBALTPATH:%_PDB%",
        "<RuntimeLibrary>MultiThreaded</RuntimeLibrary>",
    ):
        assert fragment in props or fragment in (
            ROOT / "iPlug2" / "common-win.props"
        ).read_text(encoding="utf-8-sig")

    installer = (
        ROOT / "NeuralAmpModeler" / "installer" / "Amphibia.iss"
    ).read_text(encoding="utf-8")
    for fragment in (
        "UseSetupLdr=x64",
        "RedirectionGuard=yes",
        'Source: "{#StageRoot}\\Licenses\\Roboto-Apache-2.0.txt"',
        'Source: "{#StageRoot}\\Licenses\\Michroma-OFL-1.1.txt"',
        'Source: "{#StageRoot}\\Licenses\\VST3-SDK-MIT.txt"',
        'Source: "{#StageRoot}\\Licenses\\iPlug2-Zlib.txt"',
        'Source: "{#StageRoot}\\Licenses\\minizip-ng-Zlib.txt"',
        'Source: "{#StageRoot}\\Licenses\\zlib.txt"',
        "ArchitecturesAllowed=x64os",
        "MinVersion=10.0.10240",
    ):
        assert fragment in installer

    package = (ROOT / "scripts" / "package-windows.ps1").read_text(
        encoding="utf-8-sig"
    )
    for fragment in (
        "tests\\milestone8\\test_release_readiness.py",
        "scripts\\generate-sbom.py",
        "scripts\\validate-sbom.py",
        "scripts\\scan-release-secrets.py",
        "Amphibia-0.1.0-SBOM.cdx.json",
        "ASSET-PROVENANCE.json",
        "RELEASE-SECURITY-SCAN.json",
        "BINARY-INSPECTION.txt",
        "High Entropy Virtual Addresses",
        "Dynamic base",
        "NX compatible",
        "CF instrumented",
        "not for public release",
    ):
        assert fragment in package, f"packaging evidence contract missing: {fragment}"

    for path in (
        ROOT / "docs" / "LICENSE_REVIEW_WINDOWS.md",
        ROOT / "NeuralAmpModeler" / "resources" / "ASSET_PROVENANCE.md",
        ROOT / "release" / "dependency-pins.json",
    ):
        assert path.is_file(), f"release evidence source missing: {path}"


def workflow_contract() -> None:
    workflows = list((ROOT / ".github" / "workflows").glob("*.yml"))
    assert workflows
    for path in workflows:
        text = path.read_text(encoding="utf-8")
        for match in re.finditer(r"(?m)^\s*uses:\s*([^#\s]+)", text):
            reference = match.group(1)
            assert re.search(r"@[0-9a-f]{40}$", reference), (
                f"mutable action reference in {path.name}: {reference}"
            )

    package = (ROOT / ".github" / "workflows" / "package-windows.yml").read_text(
        encoding="utf-8"
    )
    for fragment in (
        "9fad9770f2ae8542ab1a548a68c1ad1ac690abe0",
        "bcd96ce22282a890cabb5cd114a658cc84f36a620e189b89f05d8005773b1757",
        "9c73c3bae7ed48d44112a0f48e66742c00090bdb5bef71d9d3c056c66e97b732",
        "0cdd113610126d5dfd5e12ad0e0b4f3e879291ff19bb43b0c52ed2f2c2df1a37",
        "d29144deff3a68aa93ced33dddf84b7fdc26070add4aa0f4513094c8332afc4e",
        "9b360974c41a5d612eb026fb5e3bb1fa28e08865f7a4f915665b8bc94bd3bc31",
        "gitleaks.exe",
        "osv-scanner.exe",
        "cyclonedx.exe",
        "Independently validate the CycloneDX SBOM schema",
        "unsigned-not-for-public-release",
        "retention-days: 7",
    ):
        assert fragment in package, f"release workflow contract missing: {fragment}"
    assert "download-iplug-sdks.sh" not in package
    assert "winget install" not in package
    assert "permissions:\n  contents: read" in package
    assert "gh release" not in package.lower()
    assert "softprops/action-gh-release" not in package.lower()


def pin_contract() -> None:
    pins = json.loads(
        (ROOT / "release" / "dependency-pins.json").read_text(encoding="utf-8")
    )
    assert pins["schema_version"] == 1
    assert pins["evidence_id"] == EVIDENCE_ID
    by_id = {item["id"]: item for item in pins["dependencies"]}
    assert by_id["vst3-sdk"]["version"] == "3.8.0"
    assert (
        by_id["vst3-sdk"]["commit"]
        == "9fad9770f2ae8542ab1a548a68c1ad1ac690abe0"
    )
    assert by_id["asio-sdk-interface"]["release_status"].startswith("blocked")
    assert by_id["inno-setup"]["release_status"] == "reviewed-noncommercial"
    assert by_id["nlohmann-json"]["version"] == "3.12.0"
    assert by_id["minizip-ng"]["version"] == "4.2.2"
    assert by_id["zlib"]["version"] == "1.3.2"
    for section in ("build_inputs", "validation_tools"):
        for item in pins[section]:
            assert re.fullmatch(r"[0-9a-f]{64}", item["sha256"])


def deterministic_sbom_contract() -> None:
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True, encoding="utf-8"
    ).strip()
    with tempfile.TemporaryDirectory(prefix="amphibia-m8-") as temporary:
        temp = Path(temporary)
        first_sbom = temp / "first.cdx.json"
        first_assets = temp / "first-assets.json"
        second_sbom = temp / "second.cdx.json"
        second_assets = temp / "second-assets.json"
        for sbom, assets in (
            (first_sbom, first_assets),
            (second_sbom, second_assets),
        ):
            run(
                sys.executable,
                "scripts/generate-sbom.py",
                "--output",
                str(sbom),
                "--asset-manifest",
                str(assets),
                "--source-commit",
                commit,
                "--worktree-status",
                "dirty",
            )
        assert first_sbom.read_bytes() == second_sbom.read_bytes()
        assert first_assets.read_bytes() == second_assets.read_bytes()
        run(
            sys.executable,
            "scripts/validate-sbom.py",
            "--sbom",
            str(first_sbom),
            "--asset-manifest",
            str(first_assets),
        )

        sbom = json.loads(first_sbom.read_text(encoding="utf-8"))
        assets = json.loads(first_assets.read_text(encoding="utf-8"))
        assert len(sbom["components"]) >= 50
        assert assets["compiled_asset_count"] == 33
        assert assets["release_gate"] == "blocked"
        fonts = {
            Path(item["path"]).name: item
            for item in assets["assets"]
            if item["media_type"] == "font/ttf"
        }
        assert fonts["Roboto-Regular.ttf"]["license"] == "Apache-2.0"
        assert fonts["Michroma-Regular.ttf"]["license"] == "OFL-1.1"
        assert all(item["status"] == "cleared" for item in fonts.values())
        art = [
            item
            for item in assets["assets"]
            if item["media_type"].startswith("image/")
        ]
        assert art and all(
            item["status"] == "blocked-provenance" for item in art
        )
        assert all(re.fullmatch(r"[0-9a-f]{64}", item["sha256"]) for item in art)


def scanner_contract() -> None:
    with tempfile.TemporaryDirectory(prefix="amphibia-m8-scan-") as temporary:
        evidence = Path(temporary) / "scan.json"
        run(
            sys.executable,
            "scripts/scan-release-secrets.py",
            "--output",
            str(evidence),
        )
        result = json.loads(evidence.read_text(encoding="utf-8"))
        assert result["evidence_id"] == EVIDENCE_ID
        assert result["result"] == "PASS"
        assert result["finding_count"] == 0
        assert result["scope"]["tracked_source_files"] > 0
        assert result["limitations"]


def artifact_contract(root: Path) -> None:
    required = {
        SBOM_NAME,
        ASSET_NAME,
        "RELEASE-SECURITY-SCAN.json",
        "BINARY-INSPECTION.txt",
        "ARTIFACT-MANIFEST.txt",
        "SHA256SUMS.txt",
    }
    missing = sorted(name for name in required if not (root / name).is_file())
    assert not missing, f"Milestone 8 generated evidence missing: {missing}"
    run(
        sys.executable,
        "scripts/validate-sbom.py",
        "--sbom",
        str(root / SBOM_NAME),
        "--asset-manifest",
        str(root / ASSET_NAME),
        "--artifacts",
        str(root),
    )
    scan = json.loads(
        (root / "RELEASE-SECURITY-SCAN.json").read_text(encoding="utf-8")
    )
    assert scan["result"] == "PASS"
    inspection = (root / "BINARY-INSPECTION.txt").read_text(
        encoding="utf-8-sig"
    )
    for label in ("[Standalone]", "[VST3]", "[Setup]"):
        assert label in inspection
    assert "Authenticode: NotSigned" in inspection
    assert "D:\\amphibia" not in inspection
    assert "C:\\Users\\" not in inspection

    checksums = {}
    for line in (root / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines():
        digest, name = line.split("  ", 1)
        checksums[name] = digest
    for name in required - {"SHA256SUMS.txt"}:
        assert name in checksums
        assert hashlib.sha256((root / name).read_bytes()).hexdigest() == checksums[name]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()
    source_contract()
    workflow_contract()
    pin_contract()
    deterministic_sbom_contract()
    scanner_contract()
    if args.artifacts:
        artifact_contract(args.artifacts.resolve())
    print("Milestone 8 release-readiness contract: PASS")


if __name__ == "__main__":
    main()
