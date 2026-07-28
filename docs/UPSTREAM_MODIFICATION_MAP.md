# Upstream modification map

Status: Milestone 4 implementation record built on the Milestone 0 investigation

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

## Milestone 2 changes now present

- `NeuralAmpModeler/Loading/AsyncDSPWorker.h` owns newest-only pending work,
  one ready record, one completion/retirement record, request/configuration
  generation validation, cancellation, shutdown, and non-audio destruction.
- `NeuralAmpModeler/Loading/FileInspection.*` performs bounded NAM and RIFF/WAVE
  preflight before the inherited parsers or DSP constructors run.
- `_PrepareModel()` and `_PrepareIR()` retain NAM Core and AudioDSPTools sound
  implementations but move parse, construction, reset, prewarm, decode,
  resample, and convolution allocation to dedicated per-instance workers.
- `_ApplyDSPStaging()` is retained as an upstream-merge seam, but now performs
  only bounded atomic activation at a block boundary. It does not move a
  `unique_ptr` or destroy a replaced processor.
- `OnReset()` captures a new processing generation, reserves plug-in buffers,
  bypasses incompatible stage objects, and submits asynchronous replacements.
- Current and legacy state payload layouts are unchanged. Parameters restore
  immediately; non-empty paths enter the same asynchronous pipeline used by
  the compact UI and previous/next navigation.
- No provider, network, download, cache, credential, archive, standalone-state,
  or new schema work is included.

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

| File / symbol | Pre-Milestone 2 inherited behavior | Milestone 2 disposition |
|---|---|---|
| `NeuralAmpModeler/NeuralAmpModeler.cpp`, `ProcessBlock()` | Applies staging, then gate, NAM, tone stack, optional IR, DC blocker, output gain/metering. | DSP order and parameter behavior are retained; each stage obtains its active raw pointer atomically after bounded activation. |
| `NeuralAmpModeler/NeuralAmpModeler.cpp`, `_ApplyDSPStaging()` | Moves staged `unique_ptr`s onto active members in the audio callback. Replacing an active pointer can invoke its destructor there. | Validates request/configuration IDs, exchanges ready pointers, and returns old pointers through one-slot completion mailboxes. Workers reclaim them. |
| `NeuralAmpModeler/NeuralAmpModeler.cpp`, `_PrepareModel()` (replaces `_StageModel()`) | Synchronously reads JSON via NAM Core, validates one input/output, constructs `ResamplingNAM`, resets/prewarms, then stages. | Worker-only bounded preflight, JSON/schema/A2 inspection, Core construction, topology validation, reset/prewarm, and Slim setup. |
| `NeuralAmpModeler/NeuralAmpModeler.cpp`, `_PrepareIR()` (replaces `_StageIR()`) | Synchronously reads mono WAV and builds/resamples `dsp::ImpulseResponse`. | Worker-only RIFF/WAVE preflight, decode, resample, weight preparation, and maximum-block silence preallocation; failure preserves the active IR. |
| `NeuralAmpModeler/NeuralAmpModeler.cpp`, `OnReset()` | Resets active/staged model and can reconstruct sample-rate-specific IR state. | Captures a new generation, reserves plug-in buffers, marks active stages incompatible, and requests worker-prepared replacements without touching active DSP internals. |
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
| Model completion handler | Calls `_StageModel()` directly and reports errors. | Calls `_RequestModel()`; `OnIdle()` polls copied worker status and active metadata. |
| IR completion handler | Calls `_StageIR()` directly and restores old path on failure. | Calls `_RequestIR()`; requested failure never changes the committed active path. |
| `NeuralAmpModeler/NeuralAmpModelerControls.h` | Contains both browser and custom drawing controls in one large header. | Keep initial diff small, then split Amphibia content/browser controls after the renamed build. |
| `NeuralAmpModeler/resources/*` | NAM artwork, fonts, icons, platform resources. | Replace brand assets. Do not ship inherited visual assets without provenance/license confirmation. |

## State and standalone

| File / symbol | Current behavior | Planned change |
|---|---|---|
| `NeuralAmpModeler/NeuralAmpModeler.cpp`, `SerializeState()` | Milestone 1 writes `###Amphibia###`, state-layout `1.0.0`, then the inherited path/parameter payload. | A richer managed-reference schema remains Milestone 3; never include secrets. |
| `NeuralAmpModeler/NeuralAmpModeler.cpp`, `UnserializeState()` | Detects Amphibia or legacy NAM headers, then uses the inherited known-version reader. | Add binary/fixture compatibility tests before changing the schema in Milestone 3. |
| `NeuralAmpModeler/Unserialization.cpp` | Implements historical parameter layouts and invokes staging after reading paths. | Retained migrations now use bounded reads and submit non-empty paths asynchronously; richer reader isolation remains Milestone 3. |
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
| `NeuralAmpModeler/installer/Amphibia.iss` | Stable Amphibia `AppId`, fixed x64 app/VST3 paths, components/tasks, version policy, first-run, and safe retention cleanup. | Amphibia-owned release boundary; retain the AppId and compile only from the allowlisted stage. VM/signing evidence remains a release gate. |
| `scripts/package-windows.ps1` | Clean-tree Release build/test/PE validation, explicit stage/archive construction, manifests/checksums, pinned Inno compiler, and optional signing. | Amphibia-owned; never replace with inherited broad build-tree copy scripts. |
| `.github/workflows/package-windows.yml` | Least-privilege unsigned development packaging and short-retention artifact upload; no publication permission. | Keep non-publishing until protected signing/release policy is complete. |
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

- Complete inherited signal-path allocation instrumentation is still required before a product-wide real-time-safety claim; the Milestone 2 loading/activation/retirement bridge itself is bounded and lock-free at the callback boundary.
- Standalone plugin state is not persisted.
- Windows installer identity and paths are explicit; clean-VM coexistence is pending.
- Current CI does not execute pluginval and lacks security/release integrity gates.
- ASIO SDK redistribution obligations need legal review.
- Inherited fonts/artwork lack adequate provenance records in the repository.
- macOS signing/notarization and Windows code signing are not configured as releasable pipelines.
# Milestone 3 overlay

| Area | Amphibia change | Upstream-sync treatment |
| --- | --- | --- |
| `NeuralAmpModeler/Library/**` | New local import, archive policy, hashing, storage, metadata, recovery and worker slice | Amphibia-owned; retain and adapt call-site conflicts explicitly. |
| `NeuralAmpModeler.cpp/.h` | Optional library ownership, Settings/drop adapters, hash-state restore, successful-activation bookkeeping | Sensitive merge seam around existing M2 workers and state; never replace wholesale. |
| `NeuralAmpModelerControls.h` | Drop callback and compact read-only library settings surface | Reapply around upstream control/layout changes. Preserve inherited browser behavior. |
| `config.h` | State layout `1.1.0`; product SemVer/identity untouched | Keep Amphibia header and IDs frozen. |
| Windows/macOS project/config files | Reader-only archive sources, include paths, feature disables, OS crypto link | Reconcile generated upstream projects; do not copy old GUIDs or enable AAX. |
| `ThirdParty/minizip-ng`, `ThirdParty/zlib` | Exact vendored reader subset and licenses | Amphibia pin; update only through reviewed dependency change. |
| `tests/milestone3`, local-library docs | Generated security/transaction fixtures and design record | Amphibia-owned. |

# Milestone 4 overlay

| Area | Amphibia change | Upstream-sync treatment |
| --- | --- | --- |
| `NeuralAmpModeler/Providers/**` | Provider-neutral domain/interface plus isolated TONE3000 configuration, OAuth, wire parsing, API client, provider session, and download coordinator | Amphibia-owned. Never merge provider JSON or bearer tokens into inherited DSP/UI/state types. |
| `NeuralAmpModeler/Network/**` | Injected HTTP contract, URL/redirect/redaction policy, rate/retry policy, and Windows WinHTTP implementation | Amphibia-owned. Reconcile only platform/project link changes; retain certificate verification and bounds. |
| `NeuralAmpModeler/Credentials/**` | Windows Credential Manager/macOS Keychain source and cross-process refresh lock | Amphibia-owned. Never replace with inherited settings persistence or plaintext. |
| `NeuralAmpModeler/Platform/SystemBrowser.*` | Strict explicit external authorization URL launch | Amphibia-owned platform seam; do not call from inherited state/load/audio paths. |
| `LibraryIndex.*`, `ManagedLibrary.*` | Schema-2 provider associations and randomized provider staging/promotion | Sensitive extension of the Milestone 3 transaction seam; preserve schema-1 migration and object paths. |
| `NeuralAmpModeler.cpp`, `NeuralAmpModelerControls.h` | Minimal disabled/unconfigured Settings diagnostics; no final catalog flow | Reapply around upstream layout changes without turning construction/restoration into provider work. |
| Windows props/projects | Adds WinHTTP/Winsock/Credential/Shell links and Milestone 4 sources | Reconcile generated projects explicitly; v143 remains the project default. |
| `tests/milestone4`, provider documentation | Deterministic mock/loopback security and lifecycle evidence | Amphibia-owned; no live credentials/content in fixtures. |

The macOS product project does not yet compile the new provider/network source
set or an `NSURLSession` adapter. Keychain/browser source is present but is not
a platform build claim. This is an explicit Milestone 5/platform integration
handoff, not an upstream omission to paper over.

# Milestone 6 overlay

| Area | Amphibia change | Upstream-sync treatment |
| --- | --- | --- |
| `Standalone/*.h,*.cpp` | Product setup types/controller, command line, settings store, transactional audio, test/meter, folder validation | Amphibia-owned; preserve schema and RT boundary. |
| `Standalone/Windows/AmphibiaAPP*`, `SetupWizardWin.*` | Product-owned Windows APP shell and native wizard/menu/diagnostics integration | Reconcile upstream APP fixes manually; do not revert projects to direct generic APP sources. |
| `ConfigurationServices.h`, `SharedSettings.*` | Narrow host/product library bridge and cross-surface shared preferences | Keep outside DSP/provider wire types; VST3 writes shared fields only. |
| `NeuralAmpModeler.cpp/.h`, `NeuralAmpModelerControls.h` | Explicit configuration panel, preferred-browser seeding, library status/actions | Sensitive inherited UI seam; keep setup I/O out of `ProcessBlock()` and project state. |
| app/VST3 Visual Studio projects | Windows APP source substitution; shared settings sources in both; no wizard in VST3 | Reconcile generated projects explicitly and retain v143 default/AAX disabled. |
| `tests/milestone6`, characterization contract | Fake audio/settings/controller tests and source-level VST3/RT ownership checks | Amphibia-owned; do not describe fake devices/static checks as hardware/host validation. |

The macOS APP project still uses the pinned generic sources and has no Milestone
6 wizard integration. This is an explicit platform limitation. Windows
installer, packaging, component selection, manifests/checksums, and uninstall
retention are implemented in Milestone 7. Signing hooks are present, but no
signing identity or clean-VM evidence exists. macOS packaging remains
unimplemented.

# Milestone 7 overlay

| Area | Amphibia change | Upstream-sync treatment |
| --- | --- | --- |
| `installer/Amphibia.iss` | Frozen AppId, fixed x64 destinations, component/task policy, first-run, downgrade refusal, safe default-preserve uninstall | Amphibia-owned; never regenerate from the inherited NAM/template installer. |
| `scripts/package-windows.ps1` | Version-derived allowlisted stage, Release/PE/runtime checks, archives, provenance, checksums, optional signing | Amphibia-owned; review any new staged path and forbid broad wildcard input. |
| `tests/milestone7`, package workflow | Installer/artifact contract and non-publishing CI evidence | Keep exact compiler pin and artifact name `unsigned-development`. |
| Windows deployment docs | User/admin ownership, retention, signing and release gates | Keep aligned with settings/library/provider ownership contracts. |
