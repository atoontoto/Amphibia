# Milestone 2 DSP loading map

Status: inherited Milestone 1 implementation, recorded before the Milestone 2 loading refactor

Evidence date: 2026-07-22

Baseline: Amphibia `e809d0c`, upstream plug-in `v0.7.15`, NAM Core `v0.5.3`

## Model-load call path

```text
file dialog / previous / next (UI thread)
  -> NAMFileBrowserControl::LoadFileAtCurrentIndex()
  -> layout completion handler
  -> Amphibia::_StageModel()
       filesystem path conversion
       nam::get_dsp(path)
         filesystem::exists + ifstream
         nlohmann JSON parse
         version and architecture registry validation
         weights allocation/copy
         model construction and Core prewarm
       one-input/one-output validation
       ResamplingNAM construction
       ResamplingNAM::Reset() and model ResetAndPrewarm()
       optional Slim value application
       mStagedModel assignment and selected-path/UI update
  -> next ProcessBlock()
       Amphibia::_ApplyDSPStaging()
       mModel = move(mStagedModel)
```

State restoration reaches the same synchronous path through
`UnserializeState()` -> `_UnserializeStateWithKnownVersion()` or
`_UnserializeStateWithUnknownVersion()` -> `_UnserializeApplyConfig()` ->
`_StageModel()`. The file-dialog path normally runs on the UI/message thread;
state restoration runs on the host's state callback thread. Neither is
guaranteed to be a disposable worker thread.

File I/O and JSON parsing occur in NAM Core `get_dsp.cpp`. Model/config/weight,
resampler, and prewarm allocations occur in Core construction and
`ResamplingNAM::Reset()`. A2 shape recognition and selection of the fast path
occur inside Core's WaveNet config construction when `NAM_ENABLE_A2_FAST` is
defined; Amphibia does not have a separate A2 loader.

Exceptions derived from `std::runtime_error` are caught in `_StageModel()` and
returned as an unstructured string. The previous path is restored, but any
previously staged (not yet active) model is cleared. The already active model
is normally preserved. Other exception classes, allocation failure, and
malformed state-version conversion are not caught there.

## IR-load call path

```text
file dialog / previous / next (UI thread)
  -> NAMFileBrowserControl::LoadFileAtCurrentIndex()
  -> layout completion handler
  -> Amphibia::_StageIR()
       filesystem path conversion
       dsp::ImpulseResponse(file, host sample rate)
         dsp::wav::Load()
           ifstream + RIFF/fmt/fact/data parse
           complete mono sample decode and vector allocation
         ImpulseResponse::_SetWeights()
           optional cubic resampling and temporary allocations
           convolution weight/history configuration
       mStagedIR assignment and selected-path/UI update
  -> next ProcessBlock()
       Amphibia::_ApplyDSPStaging()
       mIR = move(mStagedIR)
```

State restoration invokes `_StageIR()` synchronously through
`_UnserializeApplyConfig()`. The inherited decoder accepts mono PCM 16/24/32,
IEEE float 32, and selected extensible encodings; stereo is rejected. It reads
signed chunk sizes and allocates from the declared data size without a
Milestone 2 preflight bound.

`ImpulseResponse` retains raw decoded samples, cubic-resamples when the file
rate differs from the host rate, caps the effective convolution weight count
at 8192, reverses the weights, and applies the inherited
`10^(-18/20) * 48000 / hostRate` gain. There is no normalization pass beyond
that inherited gain rule.

`_StageIR()` catches `std::runtime_error`, maps it to `ERROR_OTHER`, and restores
the prior path on failure. The active IR normally remains. A failed attempt
destroys a newly allocated staged IR synchronously on the caller thread.

## Processing, reset, and ownership

The effective `ProcessBlock()` sequence is:

```text
external input
  -> mono collapse + input gain/calibration
  -> noise-gate trigger analysis
  -> active NAM (or passthrough fallback)
  -> noise-gate gain
  -> tone stack/EQ
  -> active mono IR convolution when enabled
  -> 5 Hz DC high-pass filter
  -> output gain + mono-to-output broadcast
  -> input/output meters
```

`mModel` and `mIR` own the active processors. `mStagedModel` and `mStagedIR`
own fully constructed replacements. The staging members are written by UI or
state callbacks and read by the audio callback without a synchronization
protocol beyond unrelated atomic flags. `_ApplyDSPStaging()` runs in every
audio block and moves a staged `unique_ptr` onto the active `unique_ptr`.
Overwriting an active pointer destroys the former model/IR in the audio
callback. Clear requests do the same by assigning `nullptr` there.

`OnReset()` synchronously calls `_ResetModelAndIR()`. It resets/prewarms the
active or staged model and, for an IR sample-rate change, copies raw IR data,
constructs a replacement, resamples, and prepares weights. Host callback
threading differs by format/host, so this work cannot be treated as safely
asynchronous and can overlap loading state.

The main input/output vectors and pointer arrays are also resized from
`ProcessBlock()` when the observed block size changes. This is a pre-existing
general buffer-allocation risk, separate from model/IR preparation, which
Milestone 2 should reduce by reserving/preparing at reset and must document if
any fallback growth remains.

## Exact real-time-safety and correctness hazards

- File I/O, JSON parsing, model construction, prewarm, WAV decode, IR
  resampling, and convolution preparation block the caller of `_StageModel()`
  or `_StageIR()` and can freeze the UI or host state callback.
- Active model/IR replacement and clearing can run large destructors in
  `ProcessBlock()`.
- Staged `unique_ptr` publication is a data race: non-audio code writes while
  `ProcessBlock()` reads/moves the same object.
- A later completion has no request ID, so out-of-order work cannot be
  identified or rejected once preparation becomes asynchronous.
- Selected and active paths are conflated and can be serialized or displayed
  before the next block actually activates the object.
- Reset has no processing-generation token; a result prepared for an old
  sample rate/block size could be adopted after reconfiguration.
- Core version checking uses a mutex during parsing. It is not currently in
  `ProcessBlock()`, and must remain off the audio path.
- IR chunk sizes are signed and insufficiently bounded before allocation.
- UI messaging is performed directly by synchronous loader methods; workers
  must not inherit that behavior.
- Shutdown has no worker protocol because no workers exist yet.

## Files and methods to modify

- `NeuralAmpModeler/NeuralAmpModeler.h/.cpp`: replace staging, separate
  requested/active paths, submit loads, activate at block boundaries, handle
  reconfiguration, poll UI-safe status, and shut workers down.
- `NeuralAmpModeler/Unserialization.cpp`: submit asynchronous restores after
  immediate parameter restoration.
- `NeuralAmpModeler/NeuralAmpModelerControls.h`: display requested/loading,
  active, and failure states without changing enumeration or wraparound.
- New `NeuralAmpModeler/Loading/` code: request/status/error/configuration
  types, bounded real-time exchange, dedicated model and IR workers,
  inspection, preparation, cancellation/supersession, and off-thread
  reclamation.
- Windows/macOS project files: compile only the narrowly scoped new sources.
- `tests/`: lock the inherited contracts and exercise the exchange with fake
  DSP objects plus available Core/generated fixtures.

No iPlug2, NAM Core, or AudioDSPTools submodule edit is planned. No state
payload, identifier, navigation recursion, model architecture semantics, IR
sound rule, network/provider, installer, or product-version change is needed.
