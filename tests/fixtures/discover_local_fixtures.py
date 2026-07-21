#!/usr/bin/env python3
"""Report optional private fixture availability without leaking full paths."""

from __future__ import annotations

import hashlib
import os
import pathlib


FIXTURES = {
    "AMPHIBIA_TEST_A1_NAM": ".nam",
    "AMPHIBIA_TEST_A2_NAM": ".nam",
    "AMPHIBIA_TEST_IR_WAV": ".wav",
}


def digest(path: pathlib.Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest().upper()


def main() -> int:
    found = 0
    skipped = 0
    for variable, suffix in FIXTURES.items():
        raw = os.environ.get(variable)
        if not raw:
            print(f"SKIP {variable}: environment variable is not set")
            skipped += 1
            continue
        path = pathlib.Path(raw)
        if not path.is_file() or path.suffix.lower() != suffix:
            print(f"SKIP {variable}: configured fixture is missing or has the wrong extension")
            skipped += 1
            continue
        print(f"FOUND {variable}: {path.name} sha256={digest(path)}")
        found += 1
    print(f"Local fixture discovery: found={found} skipped={skipped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
