#!/usr/bin/env python3
"""Deterministic credential/path scan for Amphibia source and release artifacts."""

from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import zipfile


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE_ID = "RC-EVIDENCE-2026-07-28"
SCANNER_VERSION = "1.0.0"

SECRET_PATTERNS = {
    "private-key": re.compile(
        rb"-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----"
    ),
    "aws-access-key": re.compile(rb"\b(?:AKIA|ASIA)[0-9A-Z]{16}\b"),
    "github-token": re.compile(
        rb"\b(?:gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,})\b"
    ),
    "slack-token": re.compile(rb"\bxox[baprs]-[A-Za-z0-9-]{20,}\b"),
    "generic-bearer": re.compile(rb"\bBearer [A-Za-z0-9._~+/-]{24,}={0,2}\b"),
    "openai-style-key": re.compile(rb"\bsk-[A-Za-z0-9_-]{20,}\b"),
}

SENSITIVE_SUFFIXES = {
    ".pfx",
    ".p12",
    ".p8",
    ".jks",
    ".keystore",
    ".kdbx",
    ".ovpn",
}

FORBIDDEN_ARCHIVE_SUFFIXES = {
    ".pdb",
    ".ilk",
    ".lib",
    ".exp",
    ".nam",
    ".wav",
    ".pfx",
    ".p12",
    ".p8",
    ".key",
    ".pem",
    ".log",
}

PRIVATE_PATH_PATTERNS = {
    "windows-user-profile": re.compile(rb"(?i)[A-Z]:\\Users\\[^\\\x00]{1,80}\\"),
    "workspace-path": re.compile(rb"(?i)(?:D:\\amphibia|/home/[^/\x00]+/amphibia)"),
}


def tracked_files() -> list[Path]:
    process = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    return [
        ROOT / item.decode("utf-8", "surrogateescape")
        for item in process.stdout.split(b"\0")
        if item
    ]


def content_findings(data: bytes, scope: str, path: str) -> list[dict]:
    findings = []
    for rule, pattern in SECRET_PATTERNS.items():
        if pattern.search(data):
            findings.append({"rule": rule, "scope": scope, "path": path})
    return findings


def private_path_findings(data: bytes, scope: str, path: str) -> list[dict]:
    findings = []
    expanded = data
    if b"\x00" in data:
        expanded += data.replace(b"\x00", b"")
    for rule, pattern in PRIVATE_PATH_PATTERNS.items():
        if pattern.search(expanded):
            findings.append({"rule": rule, "scope": scope, "path": path})
    return findings


def scan_source() -> tuple[int, int, list[dict]]:
    files_scanned = 0
    bytes_scanned = 0
    findings: list[dict] = []
    for path in tracked_files():
        relative = path.relative_to(ROOT).as_posix()
        if path.suffix.lower() in SENSITIVE_SUFFIXES:
            findings.append(
                {"rule": "sensitive-file-type", "scope": "source", "path": relative}
            )
        if not path.is_file() or path.stat().st_size > 20 * 1024 * 1024:
            continue
        data = path.read_bytes()
        files_scanned += 1
        bytes_scanned += len(data)
        findings.extend(content_findings(data, "source", relative))
    return files_scanned, bytes_scanned, findings


def scan_archive(path: Path) -> tuple[int, int, list[dict]]:
    files_scanned = 0
    bytes_scanned = 0
    findings = []
    with zipfile.ZipFile(path) as handle:
        for info in handle.infolist():
            name = info.filename.replace("\\", "/")
            pure = PurePosixPath(name)
            if pure.is_absolute() or re.match(r"^[A-Za-z]:/", name):
                findings.append(
                    {
                        "rule": "absolute-archive-path",
                        "scope": "archive",
                        "path": f"{path.name}:{name}",
                    }
                )
            if pure.suffix.lower() in FORBIDDEN_ARCHIVE_SUFFIXES:
                findings.append(
                    {
                        "rule": "forbidden-archive-file-type",
                        "scope": "archive",
                        "path": f"{path.name}:{name}",
                    }
                )
            if info.is_dir() or info.file_size > 20 * 1024 * 1024:
                continue
            data = handle.read(info)
            files_scanned += 1
            bytes_scanned += len(data)
            display = f"{path.name}:{name}"
            findings.extend(content_findings(data, "archive", display))
            findings.extend(private_path_findings(data, "archive", display))
    return files_scanned, bytes_scanned, findings


def scan_artifacts(root: Path, output: Path | None) -> tuple[int, int, list[dict]]:
    files_scanned = 0
    bytes_scanned = 0
    findings = []
    for path in sorted(root.iterdir()):
        if not path.is_file() or (output and path.resolve() == output.resolve()):
            continue
        if path.suffix.lower() == ".zip":
            count, size, archive_findings = scan_archive(path)
            files_scanned += count
            bytes_scanned += size
            findings.extend(archive_findings)
            continue
        data = path.read_bytes()
        files_scanned += 1
        bytes_scanned += len(data)
        findings.extend(content_findings(data, "artifact", path.name))
        findings.extend(private_path_findings(data, "artifact", path.name))
    return files_scanned, bytes_scanned, findings


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    source_files, source_bytes, source_findings = scan_source()
    artifact_files = artifact_bytes = 0
    artifact_findings: list[dict] = []
    artifact_root = args.artifacts.resolve() if args.artifacts else None
    if artifact_root:
        if not artifact_root.is_dir():
            raise FileNotFoundError(f"artifact root not found: {artifact_root}")
        artifact_files, artifact_bytes, artifact_findings = scan_artifacts(
            artifact_root, args.output
        )

    findings = sorted(
        source_findings + artifact_findings,
        key=lambda item: (item["scope"], item["path"], item["rule"]),
    )
    evidence = {
        "schema_version": 1,
        "evidence_id": EVIDENCE_ID,
        "candidate": "Amphibia 0.1.0 RC validation build",
        "scanner": {
            "name": "Amphibia credential/path/archive pattern scanner",
            "version": SCANNER_VERSION,
            "source": "scripts/scan-release-secrets.py",
        },
        "scope": {
            "tracked_source_files": source_files,
            "tracked_source_bytes": source_bytes,
            "artifact_and_archive_files": artifact_files,
            "artifact_and_archive_bytes": artifact_bytes,
            "artifact_root": "release-version-root" if artifact_root else "not-scanned",
        },
        "rules": sorted(
            [
                *SECRET_PATTERNS,
                *PRIVATE_PATH_PATTERNS,
                "sensitive-file-type",
                "absolute-archive-path",
                "forbidden-archive-file-type",
            ]
        ),
        "result": "PASS" if not findings else "FAIL",
        "finding_count": len(findings),
        "findings": findings,
        "limitations": [
            "Pattern scanning does not prove that every possible secret format is absent.",
            "Independent Gitleaks history/worktree evidence is required for the RC review.",
            "Encrypted or compressed installer internals are not decoded by this scanner.",
        ],
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    print(
        f"Release credential/path scan: {evidence['result']} "
        f"({source_files + artifact_files} files, {len(findings)} findings)"
    )
    if findings:
        for finding in findings:
            print(
                f"  {finding['rule']}: {finding['scope']}:{finding['path']}",
                file=sys.stderr,
            )
        raise SystemExit(1)


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        print(f"Release credential/path scan: ERROR: {error}", file=sys.stderr)
        raise SystemExit(2)
