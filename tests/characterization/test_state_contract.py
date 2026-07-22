#!/usr/bin/env python3
"""Static compatibility assertions for the preserved state prefix and optional local-reference tail."""

from __future__ import annotations

import pathlib
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    source = (ROOT / "NeuralAmpModeler/NeuralAmpModeler.cpp").read_text(encoding="utf-8")
    restore = (ROOT / "NeuralAmpModeler/Unserialization.cpp").read_text(encoding="utf-8")
    config = (ROOT / "NeuralAmpModeler/config.h").read_text(encoding="utf-8")

    writer = source[source.index("bool Amphibia::SerializeState"): source.index("int Amphibia::UnserializeState")]
    positions = [
        writer.index('WDL_String header("###Amphibia###")'),
        writer.index("AMPHIBIA_STATE_VERSION"),
        writer.index("chunk.PutStr(mNAMPath.Get())"),
        writer.index("chunk.PutStr(mIRPath.Get())"),
        writer.index("SerializeParams(chunk)"),
    ]
    require(positions == sorted(positions), "current state payload prefix order changed")
    require('#define AMPHIBIA_STATE_VERSION "1.1.0"' in config, "managed-reference state version missing")
    require(writer.index('SerializeParams(chunk)') < writer.index('###AmphibiaLocalReferences###'),
            "optional managed references no longer follow the complete 1.0 prefix")
    require('references["schema"] = 1' in writer and '"sha256"' in writer,
            "managed hash-reference schema missing")
    require('"###NeuralAmpModeler###"' in source, "legacy NAM header is no longer accepted")
    require("_UnserializeStateWithUnknownVersion(chunk, startPos)" in source, "headerless legacy fallback removed")
    require("length < 0 || length > chunk.Size() - contentsPos" in restore, "truncated string guard missing")
    require('throw std::runtime_error("Truncated parameter state")' in restore, "truncated parameter guard missing")

    apply_config = restore[restore.index("void Amphibia::_UnserializeApplyConfig"): restore.index("// Unserialize NAM Path")]
    require(apply_config.index("OnParamReset") < apply_config.index("_RequestModel"),
            "parameters no longer restore before async model submission")
    require(apply_config.index("OnParamReset") < apply_config.index("_RequestIR"),
            "parameters no longer restore before async IR submission")
    require("_StageModel" not in restore and "_StageIR" not in restore,
            "state restoration still reaches synchronous staging")
    require('payload.GetLength() > 65536' in source, "managed state extension is not bounded")
    require('references.value("schema", 0) == 1' in source, "managed state extension is not versioned")
    require('kind == "clear"' in source and "SubmitClear(mProcessingConfiguration)" in source,
            "explicit managed-state clear does not use the asynchronous clear gateway")
    require("catch (...) { return compatibilityEnd; }" in source,
            "optional-tail corruption no longer preserves the valid compatibility prefix")
    print("Milestone 3 state compatibility contract: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, ValueError) as error:
        print(f"Milestone 3 state compatibility contract: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
