#!/usr/bin/env python3
"""Validate Amphibia's CycloneDX, asset, pin, and staging contracts."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import zipfile


ROOT = Path(__file__).resolve().parents[1]
PINS_PATH = ROOT / "release" / "dependency-pins.json"
EVIDENCE_ID = "RC-EVIDENCE-2026-07-28"
SBOM_NAME = "Amphibia-0.1.0-SBOM.cdx.json"
ASSET_NAME = "ASSET-PROVENANCE.json"


def fail(message: str) -> None:
    raise AssertionError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def properties(item: dict) -> dict[str, str]:
    result = {}
    for prop in item.get("properties", []):
        name, value = prop.get("name"), prop.get("value")
        if not isinstance(name, str) or not isinstance(value, str):
            fail(f"invalid CycloneDX property: {prop!r}")
        if name in result:
            fail(f"duplicate CycloneDX property: {name}")
        result[name] = value
    return result


def git_submodule_pins() -> dict[str, str]:
    output = subprocess.check_output(
        ["git", "submodule", "status", "--recursive"],
        cwd=ROOT,
        text=True,
        encoding="utf-8",
    )
    pins = {}
    for line in output.splitlines():
        match = re.match(r"^[ +-]?([0-9a-f]{40})\s+(\S+)", line)
        if match:
            pins[match.group(2)] = match.group(1)
    return pins


def validate_sbom(sbom: dict, pins: dict) -> None:
    assert sbom.get("$schema") == "http://cyclonedx.org/schema/bom-1.5.schema.json"
    assert sbom.get("bomFormat") == "CycloneDX"
    assert sbom.get("specVersion") == "1.5"
    assert sbom.get("version") == 1
    assert re.fullmatch(
        r"urn:uuid:[0-9a-f]{8}-[0-9a-f]{4}-5[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}",
        sbom.get("serialNumber", ""),
    )
    assert "timestamp" not in sbom.get("metadata", {}), "SBOM must be deterministic"

    root = sbom["metadata"]["component"]
    assert root["name"] == "Amphibia"
    assert root["version"] == "0.1.0"
    root_properties = properties(root)
    assert root_properties["amphibia:evidence-id"] == EVIDENCE_ID
    assert (
        root_properties["amphibia:candidate"]
        == "Amphibia 0.1.0 RC validation build"
    )
    assert (
        root_properties["amphibia:release-status"]
        == "blocked-from-public-release"
    )
    assert root_properties["amphibia:signing-status"] == "unsigned"
    assert root_properties["amphibia:tone3000-status"] == "unconfigured-fail-closed"
    assert root_properties["amphibia:macos-status"] == "unvalidated"
    assert re.fullmatch(
        r"[0-9a-f]{40}", root_properties["amphibia:source-commit"]
    )

    components = sbom.get("components", [])
    refs = [item.get("bom-ref") for item in components]
    assert all(isinstance(ref, str) and ref for ref in refs)
    assert len(refs) == len(set(refs)), "duplicate component bom-ref"
    by_ref = {item["bom-ref"]: item for item in components}

    for pin in pins["dependencies"]:
        ref = f"dependency:{pin['id']}"
        assert ref in by_ref, f"SBOM dependency missing: {pin['id']}"
        component = by_ref[ref]
        assert component["name"] == pin["name"]
        assert component["version"] == pin["version"]
        component_properties = properties(component)
        assert component_properties["amphibia:source"] == pin["source"]
        assert component_properties["amphibia:scope"] == ",".join(pin["scope"])
        assert (
            component_properties["amphibia:release-status"]
            == pin["release_status"]
        )
        if "commit" in pin:
            assert component_properties["amphibia:commit"] == pin["commit"]
        if "purl" in pin:
            assert component["purl"] == pin["purl"]
        assert component.get("licenses"), f"license missing for {pin['id']}"

    dependency_rows = sbom.get("dependencies", [])
    dependency_refs = {row.get("ref") for row in dependency_rows}
    assert dependency_refs == {root["bom-ref"], *refs}
    root_row = next(row for row in dependency_rows if row["ref"] == root["bom-ref"])
    assert set(root_row["dependsOn"]) == set(refs)
    for row in dependency_rows:
        assert set(row.get("dependsOn", ())).issubset(dependency_refs)

    serialized = json.dumps(sbom, sort_keys=True)
    for forbidden in (r"C:\\Users\\", "D:\\amphibia", "file://"):
        assert forbidden not in serialized, f"private path leaked into SBOM: {forbidden}"


def validate_pins(pins: dict) -> None:
    assert pins["schema_version"] == 1
    assert pins["evidence_id"] == EVIDENCE_ID
    ids = [item["id"] for item in pins["dependencies"]]
    assert len(ids) == len(set(ids))
    required = {
        "neural-amp-modeler-plugin",
        "neural-amp-modeler-core",
        "audio-dsp-tools",
        "iplug2",
        "eigen-root",
        "eigen-nam-core",
        "eigen-audio-dsp-tools",
        "nlohmann-json",
        "minizip-ng",
        "zlib",
        "vst3-sdk",
        "rtaudio",
        "rtmidi",
        "asio-sdk-interface",
        "nanovg",
        "nanosvg",
        "wdl",
        "inno-setup",
    }
    assert set(ids) == required
    by_id = {item["id"]: item for item in pins["dependencies"]}
    assert by_id["vst3-sdk"]["commit"] == (
        "9fad9770f2ae8542ab1a548a68c1ad1ac690abe0"
    )
    assert by_id["vst3-sdk"]["tag"] == "v3.8.0_build_66"
    assert by_id["asio-sdk-interface"]["release_status"].startswith("blocked")
    assert by_id["inno-setup"]["release_status"] == "reviewed-noncommercial"

    actual = git_submodule_pins()
    expected_paths = {
        "AudioDSPTools": by_id["audio-dsp-tools"]["commit"],
        "NeuralAmpModelerCore": by_id["neural-amp-modeler-core"]["commit"],
        "iPlug2": by_id["iplug2"]["commit"],
        "eigen": by_id["eigen-root"]["commit"],
        "NeuralAmpModelerCore/Dependencies/eigen": by_id[
            "eigen-nam-core"
        ]["commit"],
        "AudioDSPTools/Dependencies/eigen": by_id[
            "eigen-audio-dsp-tools"
        ]["commit"],
    }
    for path, expected in expected_paths.items():
        assert actual.get(path) == expected, (
            f"dependency pin drift: {path}: expected {expected}, "
            f"found {actual.get(path)}"
        )

    for group in ("build_inputs", "validation_tools"):
        for item in pins[group]:
            assert re.fullmatch(r"[0-9a-f]{64}", item["sha256"])
            assert item["source"].startswith("https://")
    for action in pins["github_actions"]:
        assert re.fullmatch(r"[0-9a-f]{40}", action["commit"])


def validate_assets(manifest: dict, sbom: dict) -> None:
    assert manifest["schema_version"] == 1
    assert manifest["evidence_id"] == EVIDENCE_ID
    assert manifest["release_gate"] in {"blocked", "cleared"}
    assets = manifest["assets"]
    assert manifest["compiled_asset_count"] == len(assets)
    assert assets
    paths = [item["path"] for item in assets]
    macros = [item["macro"] for item in assets]
    assert len(paths) == len(set(paths))
    assert len(macros) == len(set(macros))
    assert not any("Images.xcassets" in path for path in paths)
    assert not any(path.lower().endswith((".ico", ".icns")) for path in paths)

    sbom_by_ref = {item["bom-ref"]: item for item in sbom["components"]}
    blocked = 0
    for asset in assets:
        path = ROOT / PurePosixPath(asset["path"])
        assert path.is_file(), f"asset missing: {asset['path']}"
        assert path.stat().st_size == asset["bytes"]
        assert sha256(path) == asset["sha256"]
        assert re.fullmatch(r"[0-9a-f]{64}", asset["sha256"])
        ref = f"asset:{asset['path']}"
        assert ref in sbom_by_ref
        hashes = sbom_by_ref[ref].get("hashes", [])
        assert hashes == [{"alg": "SHA-256", "content": asset["sha256"]}]
        if asset["status"].startswith("blocked"):
            blocked += 1
            assert asset["license"] == "NOASSERTION"
        else:
            assert asset["status"] == "cleared"
            license_path = ROOT / PurePosixPath(asset["license_file"])
            assert license_path.is_file()
            assert sha256(license_path) == asset["license_file_sha256"]
    assert (blocked > 0) == (manifest["release_gate"] == "blocked")


def validate_artifacts(root: Path, sbom: dict, manifest: dict) -> None:
    expected = {
        SBOM_NAME,
        ASSET_NAME,
        "RELEASE-SECURITY-SCAN.json",
        "BINARY-INSPECTION.txt",
    }
    missing = sorted(name for name in expected if not (root / name).is_file())
    assert not missing, f"release evidence missing: {missing}"

    artifact_manifest = (root / "ARTIFACT-MANIFEST.txt").read_text(
        encoding="utf-8-sig"
    )
    for name in expected:
        assert name in artifact_manifest, f"artifact manifest omits {name}"

    sums = {}
    for line in (root / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        assert match, f"invalid checksum line: {line}"
        sums[match.group(2)] = match.group(1)
    for name in expected:
        assert name in sums, f"checksum file omits {name}"
        assert sha256(root / name) == sums[name]

    for archive in root.glob("*.zip"):
        with zipfile.ZipFile(archive) as handle:
            names = {name.replace("\\", "/") for name in handle.namelist()}
            assert SBOM_NAME in names, f"{archive.name} omits SBOM"
            assert ASSET_NAME in names, f"{archive.name} omits asset evidence"
            assert "Licenses/Roboto-Apache-2.0.txt" in names
            assert "Licenses/Michroma-OFL-1.1.txt" in names

    assert json.loads((root / SBOM_NAME).read_text(encoding="utf-8")) == sbom
    assert json.loads((root / ASSET_NAME).read_text(encoding="utf-8")) == manifest


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sbom", required=True, type=Path)
    parser.add_argument("--asset-manifest", required=True, type=Path)
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()

    pins = json.loads(PINS_PATH.read_text(encoding="utf-8"))
    sbom = json.loads(args.sbom.read_text(encoding="utf-8"))
    manifest = json.loads(args.asset_manifest.read_text(encoding="utf-8"))
    validate_pins(pins)
    validate_sbom(sbom, pins)
    validate_assets(manifest, sbom)
    if args.artifacts:
        validate_artifacts(args.artifacts.resolve(), sbom, manifest)
    print(
        "CycloneDX/asset/pin validation: PASS "
        "(repository validator; not an independent schema validator)"
    )
    if manifest["release_gate"] == "blocked":
        print(
            "Asset public-release gate: BLOCKED "
            "(compiled UI artwork provenance remains unresolved)"
        )


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, KeyError, ValueError, OSError) as error:
        print(f"CycloneDX/asset/pin validation: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
