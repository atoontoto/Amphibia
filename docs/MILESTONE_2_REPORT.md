# Milestone 2 report

Evidence date: 2026-07-22

Baseline: Amphibia Milestone 1 commit `e809d0c`, derived from
NeuralAmpModelerPlugin `v0.7.15` (`96337e9ab6e3beb619459779bbb5c47e1b04d8c4`)
and NAM Core `v0.5.3` (`9c7b185de346fe0725dea537bcee4bc38b5bb6d6`).

## Outcome

Milestone 2 implements bounded local NAM/WAV inspection, asynchronous model
and IR preparation, newest-request and processing-generation validation,
block-boundary atomic activation, and worker-thread reclamation. Failed,
cancelled, superseded, or stale work does not replace the active stage. State
parameters restore immediately and non-empty paths use the same asynchronous
pipeline as UI and navigation requests.

The frozen Amphibia 0.1.0 identity, parameter schema, DSP algorithms/order,
local non-recursive browsing behavior, current state header, legacy NAM state
header, and payload layout are unchanged. No network, TONE3000, OAuth, HTTP,
TLS, download, ZIP, library, installer, telemetry, setup, catalog, or unrelated
audio feature was added. No third-party dependency was added.

The implementation portion is complete, but the milestone checklist remains
open on the released-model fixture matrix: no redistributable released
third-party A1/A2 captures were supplied. The tracked Core A1 and A2-shaped
fixtures load, while all three optional local fixture variables report skips.

## Characterization baseline

- Parameters: 13 inherited parameters retain IDs 0-12, names, defaults,
  ranges, steps, enum meanings, and serialized order. The exact table is in
  `DSP_CHARACTERIZATION.md`.
- DSP order: input collapse/gain/calibration; noise-gate trigger; NAM or
  passthrough; noise-gate gain; tone stack; IR when enabled; DC high-pass;
  output gain/broadcast; meters. Tone remains before IR, matching the actual
  inherited source rather than the illustrative ordering in the request.
- Navigation: same-directory, non-recursive, dot-entry skip, inherited
  case-sensitive suffix filtering, iPlug alphabetical menu order, and wrapping
  previous/next. A missing current path maps previous to last and next to first.
- State: writer remains `###Amphibia###`, `1.0.0`, two active paths, then 13
  doubles. `###NeuralAmpModeler###` and headerless historical layouts remain
  readable. Malformed/truncated input now returns `-1` instead of throwing
  through the host callback.
- Model behavior: the existing `nam::get_dsp()` registry is retained. Strict
  A2 description uses Core `is_a2_shape()`; filenames and a WaveNet label are
  insufficient. Missing declared model rate still assumes 48 kHz, and the
  inherited 12-tap resampling wrapper is retained.
- IR behavior: the inherited mono decode, cubic resampling, reversal/gain,
  8192-sample effective-kernel cap, and bypass semantics are retained. New
  preflight rejects malformed RIFF bounds, empty data, stereo, and unsupported
  encodings before the inherited decoder allocates.
- Numerical policy: same-toolchain/same-object comparisons are expected to be
  bit-exact after a settled swap. Cross-toolchain comparisons use `1e-6`
  absolute sample and `1e-5` relative tolerance above `1e-3`; peak/RMS use
  `1e-5` absolute. Finite output is mandatory. No honest whole-plugin
  pre-refactor renderer existed, so no fabricated output hash is reported.

The characterization tests are source-contract tests plus deterministic
worker/WAV runtime tests. Full host state round trips and released-capture
audio renders remain fixture/host-dependent validation, not implied passes.

## Architecture implemented

Each Amphibia instance owns one NAM `AsyncDSPWorker` and one IR
`AsyncDSPWorker`. Each stage has a newest-only pending request, one atomic ready
record, one atomic completion/retirement record, and one atomic active raw
pointer. There is no global loading cache or detached thread.

```text
UI / state / reset
  | submit(path, request ID, processing generation)
  +--> NAM worker: inspect -> parse -> construct -> reset/prewarm --+
  +--> IR worker:  inspect -> decode -> resample -> preallocate -----+
                                                                  |
                                              one-slot ready record
                                                                  v
ProcessBlock: validate IDs -> CAS ready -> exchange active pointer
                                                                  |
                                           one-slot completion record
                                                                  v
worker: acknowledge active path/status -> destroy retired DSP/record
```

Request IDs increase monotonically. Submission replaces a pending older
request; workers check cancellation/supersession at cooperative boundaries and
before publication; audio validates again before activation. Reset increments
the processing generation and submits replacements. Until a matching object
is ready, the incompatible model or IR stage is bypassed rather than used at a
wrong sample rate/block configuration.

Ready publication is release/acquire, removal and active exchange are
acquire-release, and completion publication/reclamation is release/acquire.
Pointer atomics and audio-visible double atomics have compile-time lock-free
requirements. The audio thread cannot make a second swap while completion is
occupied, bounding retired depth at one per stage.

Shutdown invalidates requests, wakes and joins both workers, drains or destroys
worker-owned records, and only then permits final active DSP destruction on
the plug-in destruction thread. Cancellation cannot interrupt one indivisible
NAM Core call, so shutdown may wait for that call, but no worker is detached
and no callback targets a destroyed owner.

## Files changed

### Tests and fixtures

- `tests/characterization/test_inherited_contract.py`
- `tests/characterization/test_state_contract.py`
- `tests/fixtures/README.md`
- `tests/fixtures/discover_local_fixtures.py`
- `tests/milestone2/AsyncLoadingTests.cpp`
- `tests/milestone2/Milestone2Tests.vcxproj`
- `.gitignore` for generated test and isolated plug-in output

### Loading infrastructure

- `NeuralAmpModeler/Loading/LoadTypes.h`
- `NeuralAmpModeler/Loading/AsyncDSPWorker.h`
- `NeuralAmpModeler/Loading/FileInspection.h`
- `NeuralAmpModeler/Loading/FileInspection.cpp`

### Model loading, IR loading, and audio activation

- `NeuralAmpModeler/NeuralAmpModeler.h`
- `NeuralAmpModeler/NeuralAmpModeler.cpp`

These replace synchronous `_StageModel()`/`_StageIR()` preparation and
audio-thread `unique_ptr` staging with `_RequestModel()`/`_RequestIR()`,
worker-only `_PrepareModel()`/`_PrepareIR()`, and bounded
`_ApplyDSPStaging()` activation.

Slim parameter callbacks publish only an atomic revision. `OnIdle()` converts
that revision into a worker-prepared model replacement, keeping Core's
explicitly non-real-time-safe `SetSlimmableSize()` outside `ProcessBlock()`.
The compatible old model remains active until the Slim replacement succeeds.

### State restoration and UI status

- `NeuralAmpModeler/Unserialization.cpp`
- `NeuralAmpModeler/NeuralAmpModelerControls.h`

### Documentation

- `docs/MILESTONE_2_DSP_LOADING_MAP.md`
- `docs/DSP_CHARACTERIZATION.md`
- `docs/DSP_OWNERSHIP.md`
- `docs/THREADING.md`
- `docs/STATE_FORMAT.md`
- `docs/ARCHITECTURE_PLAN.md`
- `docs/THREAT_MODEL.md`
- `docs/UPSTREAM_MODIFICATION_MAP.md`
- `docs/MILESTONES.md`
- `CHANGELOG.md`

### Build system

Only the self-contained Milestone 2 test project and ignore entries were
added. `FileInspection.cpp` follows the existing amalgamated plug-in source
pattern and is included from `NeuralAmpModeler.cpp`; no product project list or
third-party build dependency changed.

## Real-time safety

| Operation | Reachable from `ProcessBlock()` after Milestone 2? | Evidence / qualification |
|---|---|---|
| File I/O | No | Inspection and loaders are called only by workers. |
| NAM JSON parsing | No | `_PrepareModel()` is a NAM-worker factory. |
| NAM construction | No | `nam::get_dsp()`, wrapper construction, reset, and prewarm are worker-only. |
| IR decode | No | `_PrepareIR()` is an IR-worker factory. |
| IR resampling | No | Inherited `ImpulseResponse` construction is worker-only. |
| Convolution preparation | No | Weights and maximum-block silence preallocation occur before publication. |
| Mutex waits | No in the loading/activation path | All path, status, configuration, and worker mutexes are outside the callback; the source review found no loading mutex reachable from the callback. |
| Worker waits | No | Audio performs no condition-variable operation, join, future get, or notification. |
| Large allocation | No for preparation/activation; conditionally reachable in the inherited buffer path | Buffers are reserved at reset and do not shrink. A host exceeding its advertised maximum can still cause one defensive grow; inherited helpers were not globally allocation-trapped. |
| Large DSP destruction | No | Old objects travel through completion and are deleted by workers. Final active objects are deleted after callbacks stop. |
| Logging/string formatting | No | Audio reads/writes only processors, numeric parameters, and fixed-size atomics. UI/status formatting occurs in `OnIdle()`. |
| Slim model rebuild / Core `SetSlimmableSize()` | No | Parameter callbacks increment an atomic revision; `OnIdle()` submits worker preparation with the current value. |

The claim is deliberately limited to the loading, activation, and retirement
bridge. It is not a universal proof that every inherited DSP helper is
allocation-free under a host contract violation.

## Failure invariants

The deterministic fake-DSP suite proves these properties for the generic
bridge used independently by NAM and IR:

- Valid A followed by invalid B leaves A active and serializable.
- A failed compatible Slim re-preparation keeps A active; only a true
  processing-configuration change marks A incompatible and temporarily bypasses it.
- A/B/C supersession permits only the newest C request to activate.
- A record built for an old processing generation fails audio validation.
- Cancelled work cannot publish or update the active path.
- Replaced and stale objects are destroyed on a worker thread, not the
  simulated audio thread.
- The completion slot holds at most one retired object per stage.
- Shutdown joins the worker and produces no late owner/UI callback.
- Two simultaneous worker instances, repeated swaps, invalid inputs,
  reconfiguration, cancellation, and destruction return lifetime counters to
  zero in the bounded stress loop.

Generated WAV tests cover valid mono PCM, sample-rate mismatch, stereo,
empty-data, unsupported encoding, and malformed/truncated RIFF input. NAM Core
loads the two tracked A1/A2-shaped fixtures successfully. Real parser failure,
resampler exception, host audio-continuity, and private/released capture cases
still require the declared integration fixture/host matrix.

## Commands run

Key source and safety searches:

```powershell
rg -n "_StageModel|_StageIR|_ApplyDSPStaging|ProcessBlock" NeuralAmpModeler
rg -n "SerializeState|UnserializeState|###Amphibia###|###NeuralAmpModeler###" NeuralAmpModeler
rg -n "LoadFileAtCurrentIndex|IDirBrowseControlBase|Previous|Next" NeuralAmpModeler iPlug2
rg -n "unique_ptr|shared_ptr|atomic|mutex|thread|future|async|queue" NeuralAmpModeler
rg -n "NAM_ENABLE_A2_FAST|is_a2_shape|wavenet_a2" NeuralAmpModeler NeuralAmpModelerCore
rg -n "mutex|lock_guard|unique_lock|condition_variable|future|get\(|join\(|delete|reset\(|make_unique|make_shared|new " NeuralAmpModeler
```

Automated tests and validation:

```powershell
python tests\characterization\test_inherited_contract.py
python tests\characterization\test_state_contract.py
python tests\identity\validate_identity.py
python tests\fixtures\discover_local_fixtures.py
& tests\milestone2\build\x64\Debug\Milestone2Tests.exe
& tests\milestone2\build\x64\Release\Milestone2Tests.exe
& .build-stage\core-m2\tools\Release\loadmodel.exe NeuralAmpModelerCore\example_models\wavenet_a1_standard.nam
& .build-stage\core-m2\tools\Release\loadmodel.exe NeuralAmpModelerCore\example_models\wavenet_a2_max.nam
```

Test build (run for Debug and Release):

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  tests\milestone2\Milestone2Tests.vcxproj /t:Rebuild `
  /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /m:2 /nologo
```

Product build from `NeuralAmpModeler/` (run for Debug and Release; final Debug
verification also ran the two targets individually with `/m:1`):

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  Amphibia.sln '/t:Amphibia-app;Amphibia-vst3' `
  '/p:Configuration=Release;Platform=x64;PlatformToolset=v145;VST3_64_PATH=D:\amphibia\.no-system-install' `
  /m:2 /nologo
```

Standalone smoke launch:

```powershell
$amphibiaProcess = Start-Process -FilePath 'D:\amphibia\NeuralAmpModeler\build-win\app\x64\Release\Amphibia.exe' -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 5
$amphibiaProcess.Refresh()
Stop-Process -Id $amphibiaProcess.Id -Force
```

Final repository checks:

```powershell
git status
git diff
git diff --cached
git diff --check
git branch --show-current
git remote -v
git log --oneline -n 20
git submodule status --recursive
```

## Builds

| Target | Configuration / toolset | Result | Output | Warnings / SHA-256 |
|---|---|---|---|---|
| Standalone | Release x64 / local v145 override | Pass | `NeuralAmpModeler/build-win/app/x64/Release/Amphibia.exe` | Clean target rebuild emitted 132 inherited numeric-conversion warnings and no errors; SHA-256 `1FBAB0929994D46A532D3A2A496CD586B2D98D4A158A4144716644D86CA4BE89`. |
| VST3 | Release x64 / local v145 override | Pass | `NeuralAmpModeler/build-win/vst3/x64/Release/Amphibia.vst3`; matching bundle staged to `.no-system-install` | Clean target rebuild emitted 120 inherited numeric-conversion warnings and no errors; SHA-256 `0682732376F5E45BB095880C1459E643FF65DCCD8AFE7419AA00AA31EC2D26AF`. |
| Standalone | Debug x64 / local v145 override | Pass, target rebuilt individually | `NeuralAmpModeler/build-win/app/x64/Debug/Amphibia.exe` | Inherited numeric-conversion warnings only, no errors; SHA-256 `63CB52802207928C38653062FCDA3DD76EAB263DC1D77E6F12446483E094ACDB`. |
| VST3 | Debug x64 / local v145 override | Pass, target rebuilt individually | `NeuralAmpModeler/build-win/vst3/x64/Debug/Amphibia.vst3` | Clean target rebuild emitted 120 inherited numeric-conversion warnings and no errors; SHA-256 `631312CBC6F83AFFA9C27AF5E6199F23C8181A204B966ACD78FEE1254CA6FC0A`. |
| Standalone smoke | Release x64 | Pass | Hidden process remained alive for five seconds and was stopped by exact PID | This proves launch/lifetime only, not device or model/IR audio. |

The repository default remains v143. v145 is a documented local verification
override. No artifact was installed into the system VST3 directory.

## Tests

### Passed

- Milestone 1 characterization contract: parameter schema, effective DSP
  ordering, non-recursive navigation, extensions, wrap behavior, retained Core
  paths, resampling constants, and no legacy staging symbols.
- Milestone 2 state compatibility contract: headers/layout, bounded corrupt
  reads, immediate parameter restoration, asynchronous path submission, active
  path serialization, and UI-thread polling.
- Milestone 2 C++ Debug and Release suites: newest wins, valid/invalid
  preservation, generation invalidation, cancellation, clear, shutdown,
  retirement thread, two-worker operation, lifetime/leak counters, bounded
  retirement, stress, and generated WAV preflight.
- Amphibia identity validation: 35 identity-bearing files.
- NAM Core portable `loadmodel`: tracked A1 fixture hash
  `C8D9711F32D032F8AD97CBC3C6706FAF9A505469E8279A9ABFBE123E824CA3F1`
  and tracked A2-shaped fixture hash
  `3D183A6B8F399F886FF3AB1F26AEB906C5916A84B18C159E89C85C17151B5C42`.

### Failed attempts (not counted as test passes)

- NAM Core `run_tests` does not compile on this Windows checkout because its
  allocation tracker unconditionally includes POSIX `dlfcn.h`. The portable
  `loadmodel` target was built and used instead; the submodule was not changed.
- A full Visual Studio solution `Rebuild` attempted unsupported AAX and failed
  because the unlicensed/unavailable AAX SDK is absent. AAX is outside the
  requested product matrix.
- A Debug combined incremental link after Release exposed the inherited
  `NeuralAmpModelerCore/NAM/dsp.cpp` object-name/configuration collision. A full
  Debug compilation refreshed it and the requested app and VST3 targets then
  passed individually with `/m:1`. This build-system risk remains documented.
- Initial sandboxed MSBuild runs could not initialize MSBuild FileTracker;
  identical approved runs outside the filesystem sandbox passed.

### Skipped / fixture-dependent

- `AMPHIBIA_TEST_A1_NAM`: skipped, not supplied.
- `AMPHIBIA_TEST_A2_NAM`: skipped, not supplied.
- `AMPHIBIA_TEST_IR_WAV`: skipped, not supplied.
- Released third-party A1/A2 sample-rate/block-size render matrix: not run.
- Full plug-in state chunk round trip and missing-file restore in a host: not
  run; the committed coverage is a structural source contract.

### Platform-dependent and not run

- macOS app/VST3, ASan, UBSan, and TSan.
- VST3 validator/DAW discovery, editor lifecycle, multiple live host instances,
  and audio-device sample-rate changes.
- Manual guitar/audio continuity, underrun, tone, and UI responsiveness during
  real model/IR preparation because no licensed local integration fixtures and
  test host were available.

## Performance

The deterministic harness measured the complete generic activation call from
outside the simulated audio callback:

- Release x64: final run 600 ns; maximum observed across recorded runs 1,100 ns.
- Debug x64: final run and maximum observed 24,000 ns (earlier runs were
  3,400-4,400 ns, illustrating scheduler/debug-build timing noise).
- Peak completion/retired depth: one per stage by construction and assertion.

Environment: Microsoft Windows 10.0.26200, Intel64 Family 6 Model 140
Stepping 1, 8 logical processors, MSVC 14.51/v145. These fake-DSP measurements
exercise request/generation checks, CAS, pointer exchange, and completion
publication; they are not universal audio deadlines.

Real NAM/IR inspection, preparation, and reclamation durations, underruns, CPU
before/after, peak/RMS, and host UI responsiveness were not measured because
the released/local fixture and host matrix was unavailable. Preparation is
off audio by design, but that is not presented as a duration measurement.

## Git

Milestone 2 logical commits:

- `975388e test: characterize inherited Amphibia DSP behavior`
- `1aa661d refactor: prepare and replace DSP off the audio thread`
- `4118feb fix: keep Slim rebuilds off the audio thread`
- Final documentation/evidence commit: the commit containing this report.

The final branch, remotes, recent log, submodule pins, and clean status are
captured after the documentation commit and reported in the handoff response.
No submodule source change, private fixture, generated audio, user setting, or
build artifact is committed.

## Known limitations

- Released redistributable A1/A2 integration fixtures and the requested
  sample-rate/block-size audio matrix remain absent.
- A single NAM Core parse/construction call is not preemptible; worker shutdown
  joins after that call returns. File byte caps reduce but do not eliminate the
  duration of a large valid model operation.
- The callback loading bridge is bounded, but the full inherited DSP path has
  no global allocation trap. A host exceeding its advertised maximum block can
  trigger a defensive vector grow.
- No macOS, sanitizer, DAW/validator, live multiple-instance, or device-change
  validation was available.
- No full host state-chunk integration test or manual audio/underrun comparison
  was run.
- The inherited Visual Studio projects can reuse the specially named NAM Core
  `dsp.obj` across configurations/targets; building requested targets
  individually after a configuration rebuild is the verified workaround.
- Empty restored paths retain inherited no-request behavior; an explicit
  managed empty-reference semantic belongs to the Milestone 3 state decision.

## Milestone 3 handoff

Milestone 3 can submit any verified individual local file through
`_RequestModel()` or `_RequestIR()` and observe immutable `LoadStatus` plus
committed active metadata. Folder enumeration must preserve the characterized
non-recursive ordering and submit each user selection through those same
request/generation rules.

ZIP scanning, safe selective extraction, hashing, a managed content-addressed
library, pack/model metadata, and deduplication are not implemented. They
should live before the existing bounded `InspectNamFile()`/`InspectWaveFile()`
and worker preparation boundary: promote one verified local file, compute and
retain its hash/metadata outside audio, then submit that path. The new loading
bridge should remain provider-neutral and unaware of archives or library
indices.
