# Upstream modification map

Status: Milestone 1 implementation record built on the Milestone 0 investigation

Baseline: upstream `v0.7.15`, commit `96337e9ab6e3beb619459779bbb5c47e1b04d8c4`

## Milestone 1 changes now present

- `config.h` is the central product/version/plug-in-ID registry; the C++ class is `Amphibia` while source filenames remain inherited.
- State writing uses `###Amphibia###` with layout version `1.0.0`; reading accepts both Amphibia and the inherited NAM header without changing the payload layout.
- Windows output names, project GUIDs, solution, version resources, and installer placeholder carry Amphibia identity.
- Apple bundle IDs, product outputs, AU symbols/codes, plists, interface metadata, and schemes carry Amphibia identity under a temporary technical namespace.
- The inherited Windows/macOS application icons are deselected. Remaining inherited UI art/fonts are development-only release blockers pending provenance or replacement.
- Root repository policy, attribution, privacy/security, contribution/build guidance, issue forms, identity checks, and build-only CI replace upstream-facing repository metadata.
- No DSP order, model/IR loading design, online provider behavior, OAuth, ZIP, signing, notarization, or release publication was implemented.

Retained names include the `NeuralAmpModeler/` source directory, C++ filenames, `.vcxproj`/Xcode project filenames, platform metadata filenames, `NeuralAmpModelerCore`, historical `.RPP` fixtures, and upstream links used for attribution. These are deliberate merge/provenance seams rather than emitted product identity.

This map identifies the inherited behavior and exact areas expected to change. It is deliberately anchored to symbols as well as line numbers because upstream rebases move lines.

## Repository provenance

| Item | Pinned value |
|---|---|
| Main remote | `upstream = https://github.com/sdatkinson/NeuralAmpModelerPlugin.git` |
| Main commit | `96337e9ab6e3beb619459779bbb5c47e1b04d8c4` (`v0.7.15`) |
| Main history | 298 commits; initial commit `11b9c0c3bebc4bd77aecb68653ed4ee17641946a`, 2022-09-23 |
| NAM Core | `9c7b185de346fe0725dea537bcee4bc38b5bb6d6` (`v0.5.3`) |
| AudioDSPTools | `0827c6c2fc0deced568536142ea86f189e0b98a1` (`v0.1.1`) |
| iPlug2 fork | `66f9060b5287afdb2ddb83aab5a2a2a8e66a4d5e` |
| Root Eigen | `ed8cda3ce4cb9d12640f9236fed968655a93b604` |

Only `upstream` is configured. Milestone 1 will add `origin` only when an actual Amphibia GitHub repository exists; no fictitious URL will be committed.

## Runtime flow

| File / symbol | Current behavior | Planned Amphibia treatment |
|---|---|---|
| `NeuralAmpModeler/NeuralAmpModeler.cpp:322`, `ProcessBlock()` | Applies staging, then gate, NAM, tone stack, optional IR, DC blocker, output gain/metering. | Preserve DSP order and parameter behavior; replace staging with bounded real-time exchange. |
| `NeuralAmpModeler/NeuralAmpModeler.cpp:595`, `_ApplyDSPStaging()` | Moves staged `unique_ptr`s onto active members in the audio callback. Replacing an active pointer can invoke its destructor there. | Exchange ready immutable objects and enqueue old objects for non-RT reclamation. |
| `NeuralAmpModeler/NeuralAmpModeler.cpp:745`, `_StageModel()` | Synchronously reads JSON via NAM Core, validates one input/output, constructs `ResamplingNAM`, resets/prewarms, then stages. | Split inspection/preparation into cancellable workers. Publish only a fully ready newest generation. |
| `NeuralAmpModeler/NeuralAmpModeler.cpp:790`, `_StageIR()` | Synchronously reads mono WAV and builds/resamples `dsp::ImpulseResponse`. | Decode, validate, bound, and prepare on worker; keep old IR on failure. |
| `NeuralAmpModeler/NeuralAmpModeler.cpp:400`, `OnReset()` | Resets active/staged model and can reconstruct sample-rate-specific IR state. | Move heavyweight rebuilds out of callback paths; use generation-safe sample-rate replacements. |
| `NeuralAmpModeler/NeuralAmpModeler.h:93`, `ResamplingNAM` | Wraps NAM Core for host/model sample-rate differences; missing model sample rate falls back to 48 kHz. | Retain initially and characterize. Surface assumed sample rate in diagnostics. |
| `AudioDSPTools/dsp/ImpulseResponse.cpp:13` | Loads and optionally cubic-resamples IR; caps weights using `mMaxLength`; processes mono then copies output. | Retain DSP sound until characterized; add file-size/sample-count guards before construction. |

## Local browsing and navigation

| File / symbol | Current behavior | Planned Amphibia treatment |
|---|---|---|
| `NeuralAmpModeler/NeuralAmpModelerControls.h:264`, `NAMFileBrowserControl` | Derives from iPlug `IDirBrowseControlBase` with `showFileExtensions=false`, `scanRecursively=false`. | Preserve compact workflow and non-recursive semantics via `LocalFileProvider`. |
| `NAMFileBrowserControl::OnMouseDown()` | File button opens file dialog; mac standalone may select a directory through `NAM_PICK_DIRECTORY`. | Preserve file dialog; make directory-library choice explicit and portable rather than compile-time surprising behavior. |
| `NAMFileBrowserControl::LoadFileAtCurrentIndex()` | Loads the selected indexed file and invokes completion handler. | Route through the async coordinator while retaining current active asset until success. |
| Previous/next handlers near lines 286-329 | Decrement/increment with wraparound. | Preserve wraparound and characterize sort order before changing data structure. |
| `iPlug2/IGraphics/IControl.cpp:1041`, `SetupMenu()` | Scans registered paths, gathers leaf menu items, and resets selected index. | Do not patch iPlug globally; implement behavior in Amphibia's local provider/controller. |
| `iPlug2/IGraphics/IControl.cpp:1120`, `ScanDirectory()` | Skips dot-prefixed entries and filters extension; recursion depends on constructor flag. | Preserve relevant filtering, add deterministic tests, and define symlink/reparse-point policy. |

## UI entry points

| File / area | Current behavior | Planned change |
|---|---|---|
| `NeuralAmpModeler/NeuralAmpModeler.cpp:110-300`, graphics layout | Fixed 600×400 compact UI with model/IR file controls and web “Get” actions. | Rename/rebrand while retaining compact controls; add an expandable content browser. |
| Model completion handler near line 180 | Calls `_StageModel()` directly and reports errors. | Submit async request and bind progress/error state by generation. |
| IR completion handler near line 200 | Calls `_StageIR()` directly and restores old path on failure. | Same transactional semantics through worker result. |
| `NeuralAmpModeler/NeuralAmpModelerControls.h` | Contains both browser and custom drawing controls in one large header. | Keep initial diff small, then split Amphibia content/browser controls after the renamed build. |
| `NeuralAmpModeler/resources/*` | NAM artwork, fonts, icons, platform resources. | Replace brand assets. Do not ship inherited visual assets without provenance/license confirmation. |

## State and standalone

| File / symbol | Current behavior | Planned change |
|---|---|---|
| `NeuralAmpModeler/NeuralAmpModeler.cpp`, `SerializeState()` | Milestone 1 writes `###Amphibia###`, state-layout `1.0.0`, then the inherited path/parameter payload. | A richer managed-reference schema remains Milestone 3; never include secrets. |
| `NeuralAmpModeler/NeuralAmpModeler.cpp`, `UnserializeState()` | Detects Amphibia or legacy NAM headers, then uses the inherited known-version reader. | Add binary/fixture compatibility tests before changing the schema in Milestone 3. |
| `NeuralAmpModeler/Unserialization.cpp:268` | Implements historical parameter layouts and invokes staging after reading paths. | Isolate as `LegacyNamStateReader`; make file restore asynchronous. |
| `iPlug2/IPlug/APP/IPlugAPP_host.cpp:104-163` | Loads/saves `settings.ini` under a `BUNDLE_NAME` directory. | Keep device preferences; change namespace and add a separate atomically written session state file. |
| `iPlug2/IPlug/APP/IPlugAPP_main.cpp:39-45` | Windows singleton mutex and window lookup use `BUNDLE_NAME`. | New Amphibia-specific interprocess names; verify side-by-side NAM operation. |

## Product identity, builds, and packaging

| Area | Current inherited values / issue | Milestone 1+ action |
|---|---|---|
| `NeuralAmpModeler/config.h` | Amphibia name/manufacturer/version, four-character IDs, explicit VST3 IDs, C++ class, bundle names, AU/AAX symbols, build provenance constants. | Frozen for Milestone 1; unresolved owner URLs remain empty. |
| `iPlug2/IPlug/IPlug_include_in_plug_src.h:106-107` | Default VST3 FUIDs combine fixed words with `PLUG_MFR_ID` and `PLUG_UNIQUE_ID`. | Define explicit new processor/controller FUIDs in product config and test host isolation. |
| `NeuralAmpModeler/projects/*.vcxproj` | NAM filenames, targets, output paths; Windows project GUIDs include VST3 `{079FC65A-F0E5-4E97-B318-A16D1D0B89DF}` and app `{41785AE4-5B70-4A75-880B-4B418B4E13C6}`. | Rename projects/targets and generate new IDE GUIDs. These are not installer identity. |
| `NeuralAmpModeler/projects/NeuralAmpModeler-macOS.xcodeproj/project.pbxproj` | NAM targets/products and `com.StevenAtkinson.*` bundle identifiers. | Rename targets/schemes/products and assign new reverse-DNS IDs. |
| `NeuralAmpModeler/resources/*.plist`, `*.xib`, `*.storyboard`, `*.entitlements` | NAM executable, bundle, factory, class, and resource names. | Rename or remove unused target resources; verify no inherited identity remains. |
| `NeuralAmpModeler/installer/Amphibia.iss` | Stable Amphibia `AppId`, product/output paths, and placeholder metadata. | Installer implementation, signing, VM install/uninstall, and side-by-side checks remain Milestone 8. |
| `NeuralAmpModeler/scripts/*` | Scripts infer `BUNDLE_NAME` but also contain NAM filenames and package prefixes. | Rename and make release inputs reproducible; remove unavailable formats from distributions. |
| `.github/workflows/build-windows.yml`, `build-macos.yml`, `test.yml` | Least-privilege development build and static identity checks; no release job. | Validate hosted runs, then add plugin validation/security/artifact gates in Milestone 9. |
| Removed upstream release workflow | Public/draft release mutation is intentionally absent. | Add a protected release workflow only after signing and validation policy exists. |

## NAM architecture map

| Upstream area | Finding | Planned change |
|---|---|---|
| `NeuralAmpModelerCore/NAM/get_dsp.cpp` and registry | Top-level architectures include Linear, LSTM, ConvNet, WaveNet, and SlimmableContainer. | Continue using the registry rather than an Amphibia-only model implementation. |
| `NeuralAmpModelerCore/NAM/wavenet/a2_fast.*` | Detects strict A2 shapes and constructs Nano (3 channels) or Standard (8 channels) fast implementations under `NAM_ENABLE_A2_FAST`. | Reuse detection for trustworthy display descriptors; preserve generic WaveNet fallback. |
| `NeuralAmpModeler/config/*.props`, `*.xcconfig` | Enables `NAM_ENABLE_A2_FAST` for native builds and compiles `a2_fast.cpp`. | Verify this survives project rename on both platforms. |
| `NeuralAmpModelerCore/example_models/wavenet_a2_max.nam` and Core tests | Upstream fixture/test coverage exists for A2-related behavior. | Add Amphibia integration fixtures including a released real A2 model whose redistribution terms permit CI use. |

See `NAM_ARCHITECTURES.md` for exact labeling and validation policy.

## Modification boundaries

- Prefer new Amphibia-owned adapters and services over changes inside `iPlug2`, NAM Core, or AudioDSPTools.
- Changes to a submodule require a separately documented upstreamable patch and pinned fork; never edit a detached submodule without recording its provenance.
- Characterization tests precede changes to DSP order, default parameters, navigation order, resampling, or legacy state.
- Provider-specific JSON, OAuth, branding, and endpoints remain under `content/tone3000`; DSP and UI domain models must not include TONE3000 wire types.
- Security policy is enforced below the provider adapter as well as in it, so a future provider cannot bypass path, size, hash, or archive controls.

## Known upstream gaps / release blockers

- Model/IR preparation and active-object retirement are not fully real-time safe.
- Standalone plugin state is not persisted.
- Windows installer identity is implicit.
- Current CI does not execute pluginval and lacks security/release integrity gates.
- ASIO SDK redistribution obligations need legal review.
- Inherited fonts/artwork lack adequate provenance records in the repository.
- macOS signing/notarization and Windows code signing are not configured as releasable pipelines.
