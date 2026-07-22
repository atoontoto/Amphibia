# Milestone 3 final report: local importing and managed library

Date: 2026-07-22

## Outcome

Milestone 3 adds a completely local, optional managed-content path alongside the inherited referenced-file and non-recursive folder browser. Amphibia can scan and import individual NAM/WAV files, recursive folders, and local ZIP packs; review supported candidates before writing; selectively extract ZIP entries; validate and hash promoted objects; deduplicate bytes; retain pack-relative associations; show library statistics and recent names; verify/clean the library; and restore managed model/IR identities from state.

Every activation still enters `_RequestModel()` or `_RequestIR()`. Milestone 2 continues to own bounded DSP inspection/preparation, request IDs, processing generations, newest-request selection, audio-boundary activation, retirement, and non-audio reclamation. Referenced files remain in place and the existing previous/next browser is unchanged.

No HTTP, TLS, OAuth, TONE3000, remote image, catalog, account, telemetry, installer, updater, upload, fingerprinting service, new model architecture, or DSP algorithm was added. Milestone 4 was not started.

## Dependency decision

| Role | Selection | Pin/license | Build policy |
| --- | --- | --- | --- |
| ZIP | minizip-ng | 4.2.2, zlib license | Vendored source; reader APIs only; no runtime download |
| DEFLATE | zlib | 1.3.2, zlib license | Vendored minimal inflate/checksum source set |
| Metadata | nlohmann/json already present in iPlug2 dependencies | Existing project dependency, MIT | Atomic schema-versioned JSON snapshot; no new database runtime |
| SHA-256/RNG | Windows CNG / macOS CommonCrypto and OS RNG | Platform facilities | Streaming hash; no bundled crypto implementation |

minizip-ng was selected instead of handwritten decompression for ZIP64 metadata, Unicode filename handling, streamed reading, CRC enforcement, and maintained cross-platform parsing. Writer compression, traditional/AES encryption, archive crypto, OpenSSL, bzip2, LZMA, zstd, and iconv integrations are disabled. The combined minizip reader/writer translation unit remains necessary for its reader API, but Amphibia exposes and calls no archive-writing path.

Remaining dependency risk is the vendored C parser surface and platform variation in unusual ZIP metadata. The generated suite covers the supported Windows path; macOS compilation and a broader external malformed-ZIP corpus remain matrix work. Exact source, licenses, options, and update procedure are recorded in `docs/DEPENDENCIES.md`, `THIRD_PARTY_NOTICES.md`, and each vendored README/license.

## Library design

Default roots are `%LOCALAPPDATA%\Amphibia\Library` on Windows and `~/Library/Application Support/Amphibia/Library` on macOS through iPlug2 application-support path discovery. Relocation is intentionally read-only rather than partially implemented.

Objects use `objects/nam/<first-two-hex>/<sha256>.nam` and `objects/ir/<first-two-hex>/<sha256>.wav`. The authoritative schema-1 snapshot is `metadata/index-v1.json`; a previous snapshot is retained as `.bak`; `metadata/schema-version` is descriptive. Objects, packs, pack entries, and user classifications are separate records. Associations, not a manually incremented count, determine liveness.

Hashing is 1 MiB streaming SHA-256 with cooperative cancellation and canonical lowercase hex. Imports inspect the source, hash it, copy through an exclusive create-new staging handle, inspect and hash the copy again, then publish on the same filesystem. Existing objects are size/hash checked. Metadata is serialized to a cryptographically random, exclusively created temporary file, flushed, then atomically replaced. Writers use an in-process mutex and OS file lock. Unknown/future/corrupt schemas are rejected without destructive migration.

Startup rejects redirected library roots before creating children, validates managed subdirectory containment, and clears only direct staging children without traversing symlink/reparse entries. Verification checks containment, existence, size, bounded format validity, optional SHA-256, associations, and orphan object files. Cleanup protects associated, pinned, and caller-supplied active hashes. Source files, source folders, and ZIP archives are never modified or deleted.

## Import workflows

- Referenced individual file: the inherited chooser or a single slot drop submits the original `.nam`/`.wav` path. No managed copy is made.
- Managed individual file: Settings Import files, or a reviewed multi-drop, scans then copies selected content through staging and the managed object transaction. Slot-originated import activates the first object matching that slot after import.
- Multiple files: iPlug2's native picker is single-select, while multi-file drag/drop is supported. The immutable plan tracks each path/type/size/status and imports only selected candidate indices; invalid items are reported without blocking other valid entries.
- Folder import: explicit recursive scan only; the inherited browser remains non-recursive. Links/reparse points are skipped, traversal is bounded/cancellable, UTF-8 relative paths are retained, and sources remain untouched.
- ZIP import: hashes and scans the central directory first. The review shows counts plus a bounded candidate name/type/size preview. No candidate is extracted until confirmation.
- Selective extraction: only selected NAM/WAV candidate indices are reopened and streamed to generated staging names. Nested source paths are metadata, never output filenames.

The core plan/API supports arbitrary selection sets and Import selected. The compact current UI selects all default valid NAM/WAV candidates and offers confirm/cancel; it does not yet expose per-row checkboxes or a searchable library tree.

## ZIP security

| Case | Handling | Verification |
| --- | --- | --- |
| Absolute/root paths | Unsafe; never extracted | Generated test |
| Drive paths | Unsafe | Generated test |
| UNC/device paths | Unsafe after separator normalization | Path/generated test |
| Traversal/current/empty components | Unsafe | Generated test |
| Symlinks | Unix mode/link metadata unsafe | Generated symlink-mode entry |
| Hard links | Link metadata/special mode unsafe | Parser policy; no generated hard-link fixture |
| Device/special files | Non-regular Unix modes unsafe | Generated special-mode coverage shares policy |
| Duplicate normalized paths | Both entries unsafe | Generated test |
| Case collisions | Both entries unsafe on all platforms | Generated test |
| ADS/colon syntax | Unsafe | Generated test |
| Encryption | Unsupported; never opened | Generated encrypted-flag test |
| Unsupported compression | Unsupported | Generated method-99 test |
| Entry-size limits | Per-type declaration and actual-byte checks | Generated policy test |
| Total-size limits | Overflow-safe declared, selected, and actual checks | Generated policy test |
| Compression ratio | Maximum 200:1 declaration policy | Code path; no real high-ratio DEFLATE fixture |
| Depth/path/component limits | Rejected during normalization | Generated test |
| File-count limit | Scan aborts | Generated test |
| CRC/declared-size failures | Extraction aborts; staging removed | Generated corrupt-CRC test |
| Cancellation | Checked during hash, scan, each extraction block, validation, copy, commit boundaries | Generated tests |
| Disk full | Free-space preflight plus checked OS writes | Policy exercised indirectly; no deterministic disk-full simulator |
| Existing staging output | Exclusive create-new; no overwrite | Generated repeat-extraction test |
| Source archive mutation | Opened read-only; before/after hash stable | Generated test |

## State changes

The frozen header remains `###Amphibia###`. The state-layout version is `1.1.0`; product identity/version and all frozen IDs remain unchanged. Writer 1.1 emits the complete 1.0 prefix in its original order, then appends:

```text
###AmphibiaLocalReferences###
{"schema":1,"model":{"kind":"managed","sha256":"..."},"ir":{"kind":"referenced"}}
```

Each slot is `managed`, `referenced`, or `clear`. The optional tail is bounded at 64 KiB. Managed hashes resolve beneath the library root and re-enter the normal request gateway. Clear uses the asynchronous clear request. An invalid/truncated optional tail is ignored after a valid 1.0 prefix; required-prefix corruption still fails. Existing Amphibia 1.0, legacy `###NeuralAmpModeler###`, and headerless readers remain intact.

## Real-time safety

| Operation | Reachable from `ProcessBlock()`? | Owner |
| --- | --- | --- |
| Archive scan/extraction | No | Import worker |
| Hashing | No | Import worker/startup non-audio path |
| JSON metadata access | No | Import worker/constructor path |
| Folder traversal/copy/delete/free-space checks | No | Import worker |
| Library verification/path resolution | No | Import worker or UI/state submission |
| Model preparation | No | Milestone 2 NAM worker |
| IR preparation | No | Milestone 2 IR worker |
| Large/stale DSP destruction | No | Milestone 2 worker/reclaimer |
| Prepared pointer activation | Yes, bounded | Existing Milestone 2 audio-boundary exchange |

The import worker is separate from both DSP workers, never detaches, publishes copied progress/results by task ID, and cancels/joins before library destruction.

## Files changed

- Import/library infrastructure: `NeuralAmpModeler/Library/LibraryTypes.*`, `ContentHash.*`, `PathSafety.*`, `FolderScanner.*`, `ImportWorker.*`.
- Archive handling: `ZipArchive.*`, vendored `ThirdParty/minizip-ng`, and minimal `ThirdParty/zlib`.
- Library/metadata: `ManagedLibrary.*`, `LibraryIndex.*`, `LibraryPaths.cpp`.
- UI/loading/state: `NeuralAmpModeler.cpp`, `.h`, `NeuralAmpModelerControls.h`, `config.h`.
- Tests: `tests/milestone3/*` and updated identity/state/characterization contracts.
- Build system: Windows props/projects and macOS xcconfig/Xcode source phases. The colliding NAM Core `dsp.cpp` now emits a target/configuration-local `nam-core-dsp.obj`; v143 remains the project default and v145 was command-line verification only.
- Documentation/legal: Milestone map, local library/schema/recovery/ZIP docs, architecture/threading/state/threat/milestone/upstream/dependency records, notices, privacy, and changelog.

## Commands run

Principal final commands (run from `D:\amphibia`) were:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-app /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-app:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-app /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-vst3 /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-vst3 /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone2\Milestone2Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:1 /nologo
& .\tests\milestone2\build\x64\Debug\Milestone2Tests.exe
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone3\Milestone3Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:1 /nologo
& .\tests\milestone3\build\x64\Debug\Milestone3Tests.exe
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone3\Milestone3Tests.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /m:1 /nologo
& .\tests\milestone3\build\x64\Release\Milestone3Tests.exe
python tests\identity\validate_identity.py
python tests\characterization\test_inherited_contract.py
python tests\characterization\test_state_contract.py
```

MSBuild commands were rerun outside the filesystem sandbox when its FileTracker initialization returned `E_ACCESSDENIED`. VST3 post-build installation was disabled; nothing was installed system-wide.

## Builds

| Target | Configuration | Result | Output | Warnings |
| --- | --- | --- | --- | ---: |
| Standalone | Release x64/v145 override | Pass, clean rebuild | `build-win/app/x64/Release/Amphibia.exe` | 132 inherited/upstream numeric warnings on full rebuild |
| Standalone | Debug x64/v145 override | Pass after Release | `build-win/app/x64/Debug/Amphibia.exe` | 23 inherited/upstream numeric warnings on changed-source build |
| VST3 | Release x64/v145 override | Pass | `build-win/vst3/x64/Release/Amphibia.vst3` | 12 inherited/upstream numeric warnings on changed-source build |
| VST3 | Debug x64/v145 override | Pass after Release | `build-win/vst3/x64/Debug/Amphibia.vst3` | 12 inherited/upstream numeric warnings on changed-source build |
| Milestone 3 tests | Debug x64 | Pass | `tests/milestone3/build/x64/Debug/Milestone3Tests.exe` | 0 final incremental warnings |
| Milestone 3 tests | Release x64 | Pass | `tests/milestone3/build/x64/Release/Milestone3Tests.exe` | 4 vendored minizip C warnings on full compile; 0 final incremental |

The Release-to-Debug sequences passed after isolating `nam-core-dsp.obj`, proving that the known cross-configuration object collision is not present in the modified projects. Artifact hashes are recorded in the Git/final-verification section below.

## Tests

Passed:

- Milestone 2 async loader regression, including newest request, generation, failure preservation, worker lifetime, and bounded audio activation (maximum observed activation call: 44,000 ns in the final regression run).
- Identity validator: 35 identity-bearing files.
- Inherited parameter/DSP/navigation/state characterization.
- Milestone 3 state structural compatibility: 1.0 prefix, optional schema/hash tail, async restore/clear gateways, bounds, and optional-tail fallback.
- SHA-256 standard vectors, streamed file, canonical conversion, cancellation, and read failure.
- Folder: empty/mixed/nested/Unicode/case-insensitive extensions, count bound, cancellation, and symlink skip when the OS permits link creation.
- ZIP: empty/root/nested/mixed/metadata/unsupported/junk, traversal/absolute/drive/UNC/ADS/depth, case collision, symlink mode, encryption, unsupported method, count/entry/total bounds, invalid central directory, CRC failure, scan/extraction cancellation, selective extraction, exclusive no-overwrite, containment, and source hash stability.
- Library: valid/invalid promotion, same-byte dedup, different-byte separation, multiple associations, content path, schema reopen/future/corrupt rejection, pack removal, active-hash cleanup protection, startup staging recovery, link-safe cleanup, cancellation without metadata publication, verification cancellation, corruption/orphan reporting, and worker lifecycle.

No unresolved final test failed. During development, generated Unicode scan and undersized-total tests exposed UTF-8 conversion and unsigned-underflow defects; both were fixed before the final pass. A pre-fix standalone link exposed the inherited `dsp.obj` path collision; the projects now isolate that object and pass Release-to-Debug builds.

Skipped/not claimed:

- macOS compile/runtime, sanitizer, and macOS case-sensitive filesystem runs (no macOS environment).
- Deterministic disk-full injection, real ZIP64 payload generation, a representable hard-link ZIP fixture, process-kill transaction interruption, and multi-process stress.
- Runtime host serialization round trips and real Core-loaded managed NAM activation; structural state tests and Milestone 2 fake-DSP tests cover the gateways, but no redistributable real NAM fixture is in the repository.
- Interactive GUI drag/drop, UI-close/application-close, and DAW restart tests. No skipped item is counted as passed.

## Performance

Release x64 smoke measurements used Windows NT 10.0.26200, `Intel64 Family 6 Model 140 Stepping 1`, 8 logical processors, and the workspace filesystem (storage media type was unavailable to the sandbox). Generated data was local and uncompressed STORE ZIP content; these are not guarantees:

| Measurement | Result |
| --- | ---: |
| SHA-256, 32 MiB | 66.03 ms / 484.62 MiB/s |
| Recursive folder scan, 500 tiny files | 140.59 ms |
| ZIP central scan, 100 tiny entries | 2.06 ms |
| Selective extraction, 100 tiny STORE entries | 147.78 ms |
| JSON commit, 500 object records | 10.55 ms |
| JSON reopen, 500 object records | 8.63 ms |

Peak memory, UI frame latency, large compressed-pack throughput, and representative verification throughput were not instrumented. Product code uses 1 MiB hash/copy and 256 KiB extraction buffers rather than whole-file reads.

## Manual verification

Automated generated local content verified nested folder/ZIP import, mixed NAM/WAV classification, cancellation, deduplication, invalid-file rejection, source preservation, and cleanup/recovery behavior. Artifact builds were not installed or launched interactively. Drag/drop hit-testing, chooser appearance, DAW restoration, closing the UI/application during active work, and audible managed NAM/IR loading were not manually claimed. No private model, IR, source archive, extracted pack, or generated library was committed.

## Git and final verification

Branch, remotes, submodules, artifact hashes, and logical Milestone 3 commit IDs at the final audit are:

```text
branch: main
remotes:
  origin   https://github.com/atoontoto/Amphibia.git
  upstream https://github.com/sdatkinson/NeuralAmpModelerPlugin.git
submodules:
  AudioDSPTools           0827c6c2fc0deced568536142ea86f189e0b98a1
  NeuralAmpModelerCore    9c7b185de346fe0725dea537bcee4bc63ed5798f
  eigen                   ed8cda3ce4cb9d12640f9236fed968655a93b604
  iPlug2                  66f9060b5287afdb2ddb83aab5a2a2a8e66a4d5e

SHA-256:
  Release standalone  2F8066BFEF70FD4D13D8E7EB30E33A339FA4D394AFB26E87DF77F888DF3E968E
  Debug standalone    AFE2FE59C3245E152865CF40A494ACC1A9166C417DF5461F9BE8100159FA02BF
  Release VST3        01CD70A27878961CC5BC7CE2318979DBF536C27A85C23A598FDC3B121DF714CC
  Debug VST3          E8050037E549087C33A469952F602901103C68D983EE3BFAC16CAE58DB59589B
  Release M3 tests    20E59D58427FDE58BC68FA2C5E9671AEAB16AD3062BF0F6C06BCFB43FE2891E4
  Debug M3 tests      4899DF3431FCA1F447E14947C793239889C2D2A2ED915FC27862A710B0A0F3E3

Milestone 3 commits before this final documentation record:
  7232ec2 docs: map Amphibia local import and library boundaries
  c8d2e38 feat: add secure managed library and ZIP import core
  f6b2320 feat: integrate local imports managed loading and state
  804c358 test: cover archive security recovery and deduplication
  f7c9469 fix: complete amalgamated and minimal dependency build
```

The working tree was clean after the final documentation commit; generated build directories are ignored. The documentation commit that contains this report is intentionally identified in the handoff rather than self-referenced here.

## Known limitations

- The compact review previews candidates and supports confirm/cancel of the default selected set; it does not yet provide per-row checkboxes, detailed architecture/WAV properties before extraction, or a three-button selected/all-valid UI.
- The native iPlug2 chooser is single-file; multiple-file importing is available by drag/drop.
- Settings exposes counts, the three most recent names, and maintenance/import actions, but not a searchable managed tree, pack-specific navigator, pin/classification editor, or one-click recent audition.
- README/license entries are classified and bounded during scans but their content is not yet ingested into pack metadata. Pack metadata currently retains source kind/hash, display name, nested associations, timestamps, and optional schema fields.
- Referenced-file mode preserves/stores its active path in state and uses Milestone 2 inspection metadata, but there is no persistent referenced-content catalog/hash/classification record.
- A second running instance serializes writes safely, but its in-memory read snapshot does not automatically refresh after another process commits until it performs a write/restarts. Process-level collision and stale-lock recovery stress remain unimplemented.
- JSON snapshot cost scales with library size; schema 1 has no paging or database migration. Repair/quarantine and safe relocation remain future explicit workflows.
- macOS project integration was added but not validated on macOS. Linux is future-only.
- Persistent diagnostic logs are not emitted, so there is no log viewer/clear-log UI; the reserved logs directory remains empty.

## Milestone 4 handoff

Milestone 4 can build on provider-neutral `ImportPackPlan`, `ArchiveEntryDescriptor`, `LibraryObject`, `PackRecord`, and `PackEntryRecord`; promotion by validated local path; streaming SHA-256/content-addressed deduplication; source-hash and nested pack associations; copied task-ID progress/status; and managed state restoration by SHA-256. Any future downloaded NAM or IR must be promoted into staging and then submitted through `_RequestModel()` or `_RequestIR()` exactly as local imports are. No provider, network, token, or TONE3000 behavior belongs in the Milestone 3 implementation.
