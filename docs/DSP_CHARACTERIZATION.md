# DSP characterization baseline

Status: pre-refactor Milestone 1 behavior

Evidence date: 2026-07-22

## Parameter contract

Automation-visible ordering is the `EParams` integer ordering below. iPlug's
parameter index is the inherited stable ID for this plug-in configuration.

| ID | Name | Default | Minimum | Maximum | Step / values |
|---:|---|---:|---:|---:|---|
| 0 | Input | 0.0 | -20.0 | 20.0 | 0.1 dB |
| 1 | Threshold | -80.0 | -100.0 | 0.0 | 0.1 dB |
| 2 | Bass | 5.0 | 0.0 | 10.0 | 0.1 |
| 3 | Middle | 5.0 | 0.0 | 10.0 | 0.1 |
| 4 | Treble | 5.0 | 0.0 | 10.0 | 0.1 |
| 5 | Output | 0.0 | -40.0 | 40.0 | 0.1 dB |
| 6 | NoiseGateActive | true | false | true | Boolean |
| 7 | ToneStack | true | false | true | Boolean |
| 8 | IRToggle | true | false | true | Boolean |
| 9 | CalibrateInput | false | false | true | Boolean |
| 10 | InputCalibrationLevel | 12.0 | -60.0 | 60.0 | 0.1 dBu |
| 11 | OutputMode | 1 | 0 | 2 | Raw, Normalized, Calibrated |
| 12 | Slim | 0.0 | 0.0 | 1.0 | 0.01 |

Current state writes all 13 values in this order after the two paths. Parameter
round trips are expected to preserve values quantized to the declared steps.

## Signal-flow contract

The source-of-truth sequence is the sequence documented in
`MILESTONE_2_DSP_LOADING_MAP.md`. Notably, the inherited tone stack is before
the IR, and the noise gate is split into pre-model trigger analysis and
post-model gain. Milestone 2 must not reorder either detail.

No model means passthrough at the NAM position. Mono internal processing
collapses connected plug-in inputs by their average (standalone sums them),
then broadcasts the mono result to all connected outputs. Standalone output is
clamped to [-1, 1]; plug-in output is not. IR bypass skips convolution without
unloading it.

## Model cases

- Core's tracked `wavenet_a1_standard.nam` is the inherited A1-shape fixture.
- Core's tracked `wavenet_a2_max.nam` is the generated A2 detector fixture.
- Both use the same `nam::get_dsp()` registry path. Strict A2 recognition is
  `nam::wavenet::a2_fast::is_a2_shape()`; a filename or `WaveNet` alone is not
  sufficient.
- A valid model becomes staged only after Core construction, topology checks,
  wrapper construction, reset/prewarm, and Slim application.
- Runtime error preserves the already active model. The inherited code may
  discard an unrelated not-yet-active staged model; the async replacement must
  instead enforce newest-request semantics.
- Missing declared model rate is treated as 48 kHz. Host/model mismatch uses
  `ResamplingContainer<double, 1, 12>`.
- Reset uses the host sample rate and maximum block size and prewarms again.
- Repeated replacement is intended to preserve the new model's sound, but the
  inherited replacement destructor is not real-time safe.

Real released third-party A1/A2 captures are optional local fixtures selected
with `AMPHIBIA_TEST_A1_NAM` and `AMPHIBIA_TEST_A2_NAM`. Absence is a reported
skip, never a pass for released-architecture integration coverage.

## IR cases

The inherited loader accepts mono PCM 16/24/32, mono IEEE float 32, and a
limited extensible variant. Stereo, A-law, mu-law, unsupported bit depth,
empty/truncated RIFF, missing fmt/data, and malformed chunk order fail. A
sample-rate mismatch uses the inherited cubic resampler. The effective kernel
is capped at 8192 samples and retains the inherited reversal and gain rule.
Longer decoded audio remains retained as raw data even though the effective
kernel is capped.

Milestone 2 generated tests use small deterministic WAVs and do not add a
binary fixture. Valid, mismatched-rate, empty-data, stereo, unsupported-depth,
and truncated cases are generated in a temporary directory.

## Navigation contract

- The selected file's directory replaces the prior path list.
- Scanning is non-recursive and skips dot-prefixed entries.
- Matching is the inherited case-sensitive suffix check against `nam` or
  `wav`; it does not independently validate a dot separator.
- Menu insertion requests alphabetical sorting from iPlug2. The flattened menu
  order drives previous/next.
- Previous at index zero wraps to the last item; next at the last item wraps to
  zero.
- With a missing/unmatched current file, selected index becomes -1. Previous
  then selects the last item and next selects the first.
- A directory change clears and rebuilds the list. No recursive behavior is
  introduced.

## State contract

Current state is `###Amphibia###`, version `1.0.0`, active model path, active IR
path, then 13 parameter doubles. The legacy `###NeuralAmpModeler###` header is
accepted with the inherited version readers. Headerless older layouts use the
oldest reader. State path restoration is synchronous in the baseline and must
become asynchronous without delaying parameter restoration.

Malformed/truncated chunks and malformed version strings are not consistently
caught in the inherited code. Milestone 2 should prevent host crashes while
retaining the current payload schema and explicit legacy reader.

## Numerical baseline and tolerances

The loading refactor does not change model, resampler, tone stack, IR, filter,
or gain algorithms. Therefore a single prepared object processed with the same
configuration is expected to be bit-exact on the same compiler/backend. Swap
timing tests ignore the boundary block and compare settled blocks.

Cross-toolchain/Core fixture comparisons are tolerance-based because Eigen,
fast-math, SIMD, and A2 fast/generic scheduling can change rounding:

- finite output is mandatory; NaN and infinity counts are zero;
- absolute sample tolerance: `1e-6` for ordinary deterministic plugin-path
  comparisons;
- relative tolerance: `1e-5` when magnitude exceeds `1e-3`;
- peak and RMS summaries: `1e-5` absolute;
- Core's own stricter A2 fast-path tests remain authoritative for fast versus
  generic A2 equivalence.

Tracked Core fixture hashes and generated-fixture rules are in
`tests/fixtures/README.md`. No pre-refactor plug-in renderer existed, so no
honest inherited whole-plugin output hash can be recorded in this checkout.
The refactor test instead compares synchronous factory output with the same
prepared object after asynchronous handoff. Local full-model renders must
record sample rate, block size, input/model/IR hashes, parameter vector, peak,
RMS, finite counts, CPU, build, platform, and host with the result.

## Milestone 2 post-refactor evidence

The source-contract characterization and state-contract tests pass after the
refactor. The Debug and Release deterministic worker/WAV suites pass; their
fake-DSP lifetime accounting returns to zero and retired depth never exceeds
one. The tracked Core A1 and A2-shaped fixtures both load successfully through
Core's portable `loadmodel` tool with the hashes recorded in
`tests/fixtures/README.md`.

The optional local variables `AMPHIBIA_TEST_A1_NAM`,
`AMPHIBIA_TEST_A2_NAM`, and `AMPHIBIA_TEST_IR_WAV` were not set, producing
three explicit skips. Consequently no released third-party A1/A2 render
matrix, full inherited IR audio comparison, peak/RMS comparison, or host audio
continuity result is claimed. Exact pass, skip, build, and platform details are
recorded in `MILESTONE_2_REPORT.md`.
