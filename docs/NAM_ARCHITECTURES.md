# NAM A1/A2 architecture plan

Status: Milestone 0 source and format investigation

## Baseline capability

The pinned application uses NeuralAmpModelerCore `v0.5.3`. Native Windows/macOS project configuration defines `NAM_ENABLE_A2_FAST` and compiles `NAM/wavenet/a2_fast.cpp`. Core's fast dispatcher recognizes the released A2 WaveNet shape for:

- A2 Nano: 3 channels.
- A2 Standard: 8 channels.

If the feature flag is absent or a WaveNet does not match the strict optimized shape, the generic WaveNet implementation remains the compatibility path. The upstream plugin's stable release therefore contains A2 capability; Amphibia does not need to invent a second model engine.

This finding is supported by the [NAM plugin v0.7.15 release](https://github.com/sdatkinson/NeuralAmpModelerPlugin/releases/tag/v0.7.15), [NAM Core v0.5.3](https://github.com/sdatkinson/NeuralAmpModelerCore/releases/tag/v0.5.3), and the official [A2 release announcement](https://www.neuralampmodeler.com/post/a2-is-released). Amphibia's own release claim remains gated on its integration tests.

## Why `architecture == "WaveNet"` is not enough

The [NAM model file specification](https://neural-amp-modeler.readthedocs.io/en/latest/model-file.html) defines the top-level architecture and configuration/weights. A2 files use WaveNet configuration; A2 is detected from the configuration shape. Older and custom WaveNet files can share the same top-level architecture string.

Consequently, Amphibia will not label every WaveNet as A1 and will not trust a provider's `architecture_version` as the sole local validation.

## Descriptor policy

`NamInspector` will parse bounded metadata before constructing DSP and produce:

```text
NamModelDescriptor
  file_format_version
  registered_architecture      # Linear/LSTM/ConvNet/WaveNet/SlimmableContainer/unknown
  family                       # A2Nano/A2Standard/LegacyOrCustom/Unknown
  declared_sample_rate?
  effective_sample_rate
  input_count
  output_count
  content_sha256
  provider_claimed_family?     # diagnostic only
```

Rules:

1. Use Core's canonical parser/registry for actual construction.
2. For a WaveNet, use the same strict `a2_fast::is_a2_shape()` logic when available. Three channels means A2 Nano; eight means A2 Standard.
3. If strict A2 recognition fails, display `WaveNet (legacy/custom)` unless additional authoritative Core metadata proves a more specific label. Do not guess “A1.”
4. Other registered architectures display their actual name and `legacy/custom` where helpful.
5. Unsupported file format versions, unregistered architectures, bad topology, invalid/non-finite weights, or excessive dimensions fail before publication.
6. Provider metadata disagreement is visible in diagnostics and fails activation when it would cross the requested A1/A2 security/compatibility boundary.

## A1/A2 loading plan

There is one `INamProcessor`/Core construction path:

1. Bounded read and JSON structural preflight on a worker.
2. Build descriptor and validate supported file semantic version.
3. Call `nam::get_dsp()` on a worker.
4. Require exactly one input and one output, matching upstream.
5. Wrap in the existing `ResamplingNAM` behavior initially.
6. Reset and prewarm completely for current sample rate/max block size on the worker.
7. Process a short finite-value probe outside the audio thread.
8. Publish as the newest immutable prepared generation.

The old active processor remains in use throughout steps 1–7. Failure reports a bounded user-facing reason and does not clear the valid processor.

## Sample-rate and block-size behavior

Upstream treats a missing model sample rate as 48 kHz and resamples around the model when host/model rates differ. Amphibia initially preserves this behavior for compatibility, with diagnostics that distinguish declared from assumed sample rate.

Integration tests cover host rates 44.1, 48, 88.2, 96, and 192 kHz where practical and blocks including 1, small odd sizes, common powers of two, and the configured maximum. Reset/rebuild must not run in the real-time callback; a new prepared generation is handed off after the host format changes.

## Test fixtures and acceptance

Required fixtures:

- At least one redistributable real legacy/A1 model.
- At least one redistributable released A2 Nano or Standard model.
- Core's generated/known A2-shape fixture for detector regression.
- Custom WaveNet that must not be mislabeled A2.
- Unsupported version, unknown architecture, wrong input/output count, truncated JSON, huge declared arrays, and non-finite/corrupt weights.

Acceptance:

- Both real A1 and real A2 load, prewarm, and produce finite, non-silent output from a deterministic signal.
- A2 fast output remains within Core's established tolerance of generic WaveNet for representative blocks.
- Rapid A1↔A2 switching never activates a stale selection and never frees a processor on the audio thread.
- Failure at every load phase keeps the prior valid model and IR.
- UI architecture labels derive from inspection and clearly separate provider claims from local truth.

Fixture licenses and source hashes must be captured in the dependency/asset manifest before committing model binaries.
