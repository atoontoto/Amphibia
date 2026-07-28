#!/usr/bin/env python3
"""Generate deterministic CycloneDX and compiled-asset evidence for Amphibia."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import uuid


ROOT = Path(__file__).resolve().parents[1]
PINS_PATH = ROOT / "release" / "dependency-pins.json"
CONFIG_PATH = ROOT / "NeuralAmpModeler" / "config.h"
RESOURCE_PATH = ROOT / "NeuralAmpModeler" / "resources"
RESOURCE_SCRIPT = RESOURCE_PATH / "main.rc"
GENERATOR_VERSION = "1.0.0"
EVIDENCE_ID = "RC-EVIDENCE-2026-07-28"

LICENSE_FILES = {
    "neural-amp-modeler-plugin": ROOT / "LICENSE",
    "neural-amp-modeler-core": ROOT / "NeuralAmpModelerCore" / "LICENSE",
    "audio-dsp-tools": ROOT / "AudioDSPTools" / "LICENSE",
    "iplug2": ROOT / "iPlug2" / "LICENSE.txt",
    "minizip-ng": ROOT / "ThirdParty" / "minizip-ng" / "LICENSE",
    "zlib": ROOT / "ThirdParty" / "zlib" / "LICENSE",
    "vst3-sdk": ROOT / "iPlug2" / "Dependencies" / "IPlug" / "VST3_SDK" / "LICENSE.txt",
    "nanovg": ROOT / "iPlug2" / "Dependencies" / "IGraphics" / "NanoVG" / "LICENSE.txt",
    "nanosvg": ROOT / "iPlug2" / "Dependencies" / "IGraphics" / "NanoSVG" / "LICENSE.txt",
}

FONT_RECORDS = {
    "Roboto-Regular.ttf": {
        "name": "Roboto Regular",
        "version": "1.100141 (2013)",
        "creator": "Google",
        "source": "embedded font name records; Google Fonts apache/roboto",
        "license": "Apache-2.0",
        "license_file": "eigen/COPYING.APACHE",
        "status": "cleared",
    },
    "Michroma-Regular.ttf": {
        "name": "Michroma Regular",
        "version": "1.100",
        "creator": "The Michroma Project Authors; Vernon Adams",
        "source": "https://github.com/googlefonts/Michroma-font",
        "license": "OFL-1.1",
        "license_file": "iPlug2/Dependencies/IGraphics/NanoVG/example/LICENSE_OFL.txt",
        "status": "cleared",
    },
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git(*args: str) -> str:
    return subprocess.check_output(
        ["git", *args], cwd=ROOT, text=True, encoding="utf-8"
    ).strip()


def product_version() -> str:
    text = CONFIG_PATH.read_text(encoding="utf-8")
    match = re.search(r'(?m)^#define PLUG_VERSION_STR "(\d+\.\d+\.\d+)"$', text)
    if not match:
        raise ValueError("PLUG_VERSION_STR is missing")
    return match.group(1)


def license_entry(identifier: str) -> dict:
    if identifier.startswith("LicenseRef-") or identifier == "NOASSERTION":
        return {"license": {"name": identifier}}
    return {"license": {"id": identifier}}


def property_entry(name: str, value: object) -> dict:
    if isinstance(value, list):
        value = ",".join(str(item) for item in value)
    return {"name": name, "value": str(value)}


def resource_macros() -> dict[str, tuple[str, str]]:
    config = CONFIG_PATH.read_text(encoding="utf-8")
    values = {
        match.group(1): match.group(2)
        for match in re.finditer(
            r'(?m)^#define\s+([A-Z][A-Z0-9_]+_FN)\s+"([^"]+)"$', config
        )
    }
    resources: dict[str, tuple[str, str]] = {}
    for match in re.finditer(
        r"(?m)^([A-Z][A-Z0-9_]+_FN)\s+(TTF|SVG|JPG|PNG)\s+\1\s*$",
        RESOURCE_SCRIPT.read_text(encoding="utf-8-sig"),
    ):
        macro, kind = match.groups()
        if macro not in values:
            raise ValueError(f"resource macro has no config value: {macro}")
        resources[macro] = (kind, values[macro])
    if not resources:
        raise ValueError("no compiled resource macros found")
    return resources


def compiled_assets() -> list[dict]:
    assets = []
    for macro, (kind, filename) in sorted(resource_macros().items()):
        folder = "fonts" if kind == "TTF" else "img"
        path = RESOURCE_PATH / folder / filename
        if not path.is_file():
            raise FileNotFoundError(f"compiled resource missing: {path}")
        relative = path.relative_to(ROOT).as_posix()
        if filename in FONT_RECORDS:
            record = FONT_RECORDS[filename]
            license_file = ROOT / record["license_file"]
            if not license_file.is_file():
                raise FileNotFoundError(
                    f"font license evidence missing: {record['license_file']}"
                )
            asset = {
                "path": relative,
                "macro": macro,
                "media_type": "font/ttf",
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
                **record,
                "license_file_sha256": sha256(license_file),
            }
        else:
            asset = {
                "path": relative,
                "macro": macro,
                "media_type": {
                    "SVG": "image/svg+xml",
                    "JPG": "image/jpeg",
                    "PNG": "image/png",
                }[kind],
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
                "name": filename,
                "version": "NeuralAmpModelerPlugin-0.7.15",
                "creator": "upstream contributors; individual ownership not fully established",
                "source": "NeuralAmpModelerPlugin upstream Git history",
                "license": "NOASSERTION",
                "status": "blocked-provenance",
            }
        assets.append(asset)
    return assets


def dependency_component(pin: dict) -> dict:
    ref = f"dependency:{pin['id']}"
    component = {
        "type": "application" if pin["id"] == "inno-setup" else "library",
        "bom-ref": ref,
        "name": pin["name"],
        "version": pin["version"],
        "licenses": [license_entry(pin["license"])],
        "properties": [
            property_entry("amphibia:source", pin["source"]),
            property_entry("amphibia:scope", pin["scope"]),
            property_entry("amphibia:release-status", pin["release_status"]),
        ],
    }
    if pin.get("purl"):
        component["purl"] = pin["purl"]
    if pin.get("commit"):
        component["properties"].append(
            property_entry("amphibia:commit", pin["commit"])
        )
    if pin.get("tag"):
        component["properties"].append(property_entry("amphibia:tag", pin["tag"]))
    license_path = LICENSE_FILES.get(pin["id"])
    if license_path and license_path.is_file():
        component["properties"].extend(
            (
                property_entry(
                    "amphibia:license-file", license_path.relative_to(ROOT).as_posix()
                ),
                property_entry("amphibia:license-file-sha256", sha256(license_path)),
            )
        )
    return component


def asset_component(asset: dict) -> dict:
    ref = f"asset:{asset['path']}"
    component = {
        "type": "file",
        "bom-ref": ref,
        "name": asset["name"],
        "version": asset["version"],
        "hashes": [{"alg": "SHA-256", "content": asset["sha256"]}],
        "licenses": [license_entry(asset["license"])],
        "properties": [
            property_entry("amphibia:path", asset["path"]),
            property_entry("amphibia:resource-macro", asset["macro"]),
            property_entry("amphibia:media-type", asset["media_type"]),
            property_entry("amphibia:bytes", asset["bytes"]),
            property_entry("amphibia:creator", asset["creator"]),
            property_entry("amphibia:source", asset["source"]),
            property_entry("amphibia:release-status", asset["status"]),
        ],
    }
    if asset.get("license_file"):
        component["properties"].extend(
            (
                property_entry("amphibia:license-file", asset["license_file"]),
                property_entry(
                    "amphibia:license-file-sha256", asset["license_file_sha256"]
                ),
            )
        )
    return component


def build_sbom(source_commit: str, worktree_status: str) -> tuple[dict, dict]:
    pins = json.loads(PINS_PATH.read_text(encoding="utf-8"))
    assets = compiled_assets()
    version = product_version()
    root_ref = f"pkg:generic/Amphibia@{version}"
    components = [
        dependency_component(pin) for pin in pins["dependencies"]
    ] + [asset_component(asset) for asset in assets]
    component_refs = [component["bom-ref"] for component in components]

    sbom = {
        "$schema": "http://cyclonedx.org/schema/bom-1.5.schema.json",
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "serialNumber": "urn:uuid:"
        + str(uuid.uuid5(uuid.NAMESPACE_URL, f"amphibia:{version}:{source_commit}")),
        "version": 1,
        "metadata": {
            "tools": {
                "components": [
                    {
                        "type": "application",
                        "name": "Amphibia deterministic SBOM generator",
                        "version": GENERATOR_VERSION,
                        "properties": [
                            property_entry(
                                "amphibia:source", "scripts/generate-sbom.py"
                            )
                        ],
                    }
                ]
            },
            "component": {
                "type": "application",
                "bom-ref": root_ref,
                "name": "Amphibia",
                "version": version,
                "licenses": [license_entry("MIT")],
                "properties": [
                    property_entry("amphibia:evidence-id", EVIDENCE_ID),
                    property_entry(
                        "amphibia:candidate",
                        "Amphibia 0.1.0 RC validation build",
                    ),
                    property_entry("amphibia:source-commit", source_commit),
                    property_entry("amphibia:worktree-status", worktree_status),
                    property_entry(
                        "amphibia:release-status", "blocked-from-public-release"
                    ),
                    property_entry("amphibia:signing-status", "unsigned"),
                    property_entry(
                        "amphibia:tone3000-status", "unconfigured-fail-closed"
                    ),
                    property_entry("amphibia:macos-status", "unvalidated"),
                ],
            },
        },
        "components": components,
        "dependencies": [
            {"ref": root_ref, "dependsOn": component_refs},
            *({"ref": ref, "dependsOn": []} for ref in component_refs),
        ],
    }
    asset_manifest = {
        "schema_version": 1,
        "evidence_id": EVIDENCE_ID,
        "candidate": "Amphibia 0.1.0 RC validation build",
        "generator": {
            "name": "scripts/generate-sbom.py",
            "version": GENERATOR_VERSION,
        },
        "source_commit": source_commit,
        "compiled_asset_count": len(assets),
        "release_gate": (
            "blocked"
            if any(asset["status"].startswith("blocked") for asset in assets)
            else "cleared"
        ),
        "assets": assets,
    }
    return sbom, asset_manifest


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--asset-manifest", required=True, type=Path)
    parser.add_argument("--source-commit")
    parser.add_argument("--worktree-status", choices=("clean", "dirty"))
    args = parser.parse_args()

    source_commit = args.source_commit or git("rev-parse", "HEAD")
    if not re.fullmatch(r"[0-9a-f]{40}", source_commit):
        raise ValueError("source commit must be a full lowercase Git object ID")
    worktree_status = args.worktree_status
    if not worktree_status:
        worktree_status = (
            "clean"
            if not git("status", "--porcelain=v1", "--untracked-files=all")
            else "dirty"
        )

    sbom, asset_manifest = build_sbom(source_commit, worktree_status)
    for destination, data in (
        (args.output, sbom),
        (args.asset_manifest, asset_manifest),
    ):
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(
            json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    print(
        f"SBOM generated: {args.output} "
        f"({len(sbom['components'])} components)"
    )
    print(
        f"Compiled asset evidence: {args.asset_manifest} "
        f"({asset_manifest['compiled_asset_count']} assets, "
        f"gate={asset_manifest['release_gate']})"
    )


if __name__ == "__main__":
    main()
