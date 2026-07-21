#!/usr/bin/env python3
"""Characterize frozen Amphibia Milestone 1 source contracts.

These checks deliberately name the inherited implementation. Runtime loading
tests complement them; this suite catches accidental parameter, signal-order,
navigation, identity, and A2-wiring drift in every lightweight checkout.
"""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def ordered(text: str, snippets: list[str], contract: str) -> None:
    cursor = -1
    for snippet in snippets:
        position = text.find(snippet, cursor + 1)
        require(position >= 0, f"{contract}: missing {snippet!r}")
        require(position > cursor, f"{contract}: out of order at {snippet!r}")
        cursor = position


def test_parameter_contract(header: str, source: str) -> None:
    ordered(
        header,
        [
            "kInputLevel = 0",
            "kNoiseGateThreshold",
            "kToneBass",
            "kToneMid",
            "kToneTreble",
            "kOutputLevel",
            "kNoiseGateActive",
            "kEQActive",
            "kIRToggle",
            "kCalibrateInput",
            "kInputCalibrationLevel",
            "kOutputMode",
            "kSlim",
            "kNumParams",
        ],
        "parameter IDs",
    )
    initializers = [
        'GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);',
        'GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);',
        'GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);',
        'GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);',
        'GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);',
        'GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);',
        'GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);',
        'GetParam(kEQActive)->InitBool("ToneStack", true);',
        'GetParam(kIRToggle)->InitBool("IRToggle", true);',
        'InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu")',
        'GetParam(kOutputMode)->InitEnum("OutputMode", 1, {"Raw", "Normalized", "Calibrated"});',
        'GetParam(kSlim)->InitDouble("Slim", 0.0, 0.0, 1.0, 0.01);',
    ]
    normalized = re.sub(r"\s+", " ", source)
    for initializer in initializers:
        require(re.sub(r"\s+", " ", initializer) in normalized, f"parameter schema drift: {initializer}")
    require('const bool kDefaultCalibrateInput = false;' in source, "CalibrateInput default changed")
    require('const double kDefaultInputCalibrationLevel = 12.0;' in source, "calibration default changed")


def test_dsp_order(source: str) -> None:
    block = source[source.index("void Amphibia::ProcessBlock"): source.index("void Amphibia::OnReset")]
    ordered(
        block,
        [
            "_ProcessInput(",
            "_ApplyDSPStaging();",
            "mNoiseGateTrigger.Process(",
            "mModel->process(",
            "mNoiseGateGain.Process(",
            "mToneStack->Process(",
            "mIR->Process(",
            "mHighPass.Process(",
            "_ProcessOutput(",
            "_UpdateMeters(",
        ],
        "DSP order",
    )


def test_navigation(controls: str, iplug: str) -> None:
    require("IDirBrowseControlBase(bounds, fileExtension, false, false)" in controls, "navigation became recursive")
    require("if (mSelectedItemIndex < 0)\n        mSelectedItemIndex = nItems - 1;" in controls, "previous wrap changed")
    require("if (mSelectedItemIndex >= nItems)\n        mSelectedItemIndex = 0;" in controls, "next wrap changed")
    require("f && f[0] != '.'" in iplug, "dot-file filtering changed")
    require("strlen(a) == strlen(mExtension.Get())" in iplug, "extension suffix matching changed")
    require("sort alphabetically" in iplug, "alphabetical menu insertion changed")


def test_state_and_architecture(source: str, config: str, core: str) -> None:
    require('WDL_String header("###Amphibia###");' in source, "Amphibia state writer changed")
    require('"###NeuralAmpModeler###"' in source, "legacy state reader removed")
    require('#define AMPHIBIA_STATE_VERSION "1.0.0"' in config, "state layout version changed")
    require('#define PLUG_VERSION_STR "0.1.0"' in config, "product version changed")
    require("NAM_ENABLE_A2_FAST" in core and "is_a2_shape" in core, "Core A2 fast-path wiring missing")


def main() -> int:
    header = read("NeuralAmpModeler/NeuralAmpModeler.h")
    source = read("NeuralAmpModeler/NeuralAmpModeler.cpp")
    controls = read("NeuralAmpModeler/NeuralAmpModelerControls.h")
    iplug = read("iPlug2/IGraphics/IControl.cpp")
    config = read("NeuralAmpModeler/config.h")
    core = read("NeuralAmpModelerCore/NAM/wavenet/model.cpp")

    test_parameter_contract(header, source)
    test_dsp_order(source)
    test_navigation(controls, iplug)
    test_state_and_architecture(source, config, core)
    print("Milestone 1 characterization contract: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"Milestone 1 characterization contract: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
