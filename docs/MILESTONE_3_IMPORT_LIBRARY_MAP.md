# Milestone 3 import and managed-library map

Status: pre-implementation design gate

Evidence date: 2026-07-22

Baseline: Amphibia Milestone 2 (`fcc503e`), derived from NeuralAmpModelerPlugin
`v0.7.15`, NAM Core `v0.5.3`, AudioDSPTools `v0.1.1`, and the pinned iPlug2
fork `66f9060`.

This map was written before Milestone 3 product implementation. It records the
existing paths, ownership contracts, dependency decision, and upstream merge
boundaries that the importer and library must preserve.

## Existing local selection flows

### Individual NAM selection

`NAMFileBrowserControl` is an iPlug `IDirBrowseControlBase` configured for the
`nam` suffix, hidden extensions, and non-recursive scanning. Its file button
calls `IGraphics::PromptForFile()`. A successful selection clears the prior path
list, registers the selected file's directory, rebuilds the inherited sorted
menu, selects the file, and invokes the layout completion handler. The handler
calls `Amphibia::_RequestModel()`.

`_RequestModel()` captures the current `ProcessingConfiguration` under
`mProcessingConfigurationMutex` and submits the path and current Slim value to
the per-instance model `AsyncDSPWorker`. The worker performs bounded inspection,
NAM Core parse/construction, topology validation, reset/prewarm, and publishes a
one-slot activation record. Only `ProcessBlock()` adopts it, at a block boundary.

### Individual IR selection

The IR control is the same class configured for `wav`. Its completion handler
calls `_RequestIR()`, which submits the current processing configuration to the
independent per-instance IR worker. Bounded RIFF inspection, decode, resampling,
kernel preparation, and maximum-block allocation happen on that worker before
block-boundary activation.

### Folder browser and previous/next

The inherited browser is not a recursive importer. Selecting a file replaces
the registered path with its containing directory. With `NAM_PICK_DIRECTORY`,
the macOS standalone build may select a directory and then selects its first
matching file. `IDirBrowseControlBase` was constructed with
`scanRecursively=false`; dot-prefixed entries are skipped and suffix matching is
the inherited case-sensitive behavior. Previous and next decrement/increment
the menu index and wrap. A missing current item maps previous to the last entry
and next to the first. Milestone 3 must not mix managed content into this list.

### Existing drag and drop

The product controls do not override `OnDrop()` or `OnDropMultiple()` today.
iPlug already supplies the platform seam:

- Windows `IGraphicsWin` forwards one or multiple dropped paths.
- macOS `IGraphicsMac_view` forwards one or multiple dropped paths.
- `IGraphics::OnDrop*()` hit-tests the target control.
- `IControl` exposes virtual `OnDrop(const char*)` and
  `OnDropMultiple(const std::vector<const char*>&)` methods.

Amphibia can therefore implement target-specific intent without changing the
iPlug submodule: a single matching file on a model/IR slot references and loads
it; multiple files, directories, or ZIPs are sent to an import-review surface.

## Milestone 2 loading and ownership requirements

`_RequestModel()` and `_RequestIR()` are the only product loading gateways for
referenced and managed files. Library code may resolve and verify a path, but it
must never construct or publish live DSP.

Each stage owns an `AsyncDSPWorker` with one newest-only pending request, one
ready record, one completion record, and monotonic request/configuration IDs.
The audio callback validates both IDs, atomically adopts the prepared raw
pointer, and publishes the former pointer for worker reclamation. It performs no
file I/O, parsing, locking, string work, allocation-heavy preparation, or large
destruction. A failed, stale, superseded, cancelled, or wrong-generation request
does not replace the active object. Library tasks must use a separate joined
worker so archive or folder work cannot starve either DSP worker.

## Active, requested, and serialized paths

`AsyncDSPWorker` owns immutable request paths and copied `LoadStatus` snapshots.
The product's `mNAMPath` and `mIRPath`, protected by `mPathMutex`, are committed
only after successful audio-boundary activation and worker acknowledgement.
They are the active paths used by UI and serialization. Requested, ready,
failed, cancelled, and stale paths are not serialized.

The current writer is exactly:

```text
string      ###Amphibia###
string      1.0.0
string      active NAM path or empty
string      active IR path or empty
double[13]  parameters in frozen EParams order
```

Readers also accept `###NeuralAmpModeler###` and headerless historical layouts.
Parameters restore immediately; non-empty paths are submitted asynchronously.
The Milestone 3 extension must be length-delimited and optional after this
payload so old Amphibia/NAM states remain readable. Managed identity is the
SHA-256 object hash, not an absolute managed path. Explicit clear must remain
distinct from an absent optional extension and historical empty/no-request
behavior.

## Platform filesystem facilities

The product already compiles `iPlug2/IPlug/IPlugPaths.cpp` on Windows and macOS.
`iplug::AppSupportPath(path, false)` returns `%LOCALAPPDATA%` on Windows and the
per-user Application Support directory on macOS. Milestone 3 appends
`Amphibia/Library`; it does not use an executable, Program Files, plug-in bundle,
or application bundle path. `std::filesystem::u8path` is already used at the
model/IR boundary and remains the product path representation.

Windows-specific hardening may use Win32 file attributes and reparse-point
checks. macOS uses `lstat`/filesystem status without following links. Directory
imports ignore symlinks, junctions, and other reparse points by default.

## Existing data and dependency facilities

- JSON: the pinned NAM Core checkout already provides nlohmann/json and the
  product already uses it for state migrations and NAM metadata.
- Hashing: no source SHA-256 implementation exists. PowerShell hashing used by
  prior reports is not a runtime facility.
- Archives: no ZIP/archive library or product archive code exists.
- Database: no SQLite or other database facility exists.
- Atomic persistence: no library index implementation exists. `std::filesystem`
  provides same-filesystem rename but Amphibia must add temp-file, flush,
  replace, backup, lock, and recovery policy.
- Application data: `AppSupportPath()` is the existing cross-platform known
  folder abstraction.
- Drag/drop: iPlug single/multiple drop dispatch is present on Windows/macOS as
  described above.

## Dependency decision

### Archive reader: minizip-ng 4.2.2 plus zlib 1.3.2

Pin the exact upstream release tags `zlib-ng/minizip-ng` `4.2.2` and
`madler/zlib` `1.3.2`; vendor only reproducible source needed by the native
builds and retain their zlib licenses. No runtime download is permitted.

Build minizip-ng as a ZIP reader only. Disable writer APIs in the Amphibia
surface, traditional/AES encryption, archive hash extensions, OpenSSL, bzip2, LZMA, Zstandard, iconv,
and compatibility command-line tools. Enable ordinary stored/Deflate ZIP,
ZIP64, Unicode names, central-directory metadata, CRC validation, and streaming
entry reads. Calls are confined to the library worker. Amphibia reads selected
entries in bounded chunks so it can apply cooperative cancellation and actual
byte limits between reads; one library call is not assumed preemptible.

Reasons for selection:

- Maintained portable C code with active Windows/macOS builds and a small
  reader surface compared with a multi-format archive framework.
- ZIP64, Unicode filenames, streaming reads, entry metadata, and CRC support.
- zlib license is compatible with Amphibia's MIT distribution.
- Exact source releases can be vendored and compiled without a package manager.
- The decompressor is established upstream code; Amphibia does not implement a
  custom Deflate codec.

Security qualification: minizip-ng is a mechanism, not the policy boundary.
Amphibia must reject encryption, unsupported methods, multi-disk input, links,
special files, inconsistent sizes, unsafe paths, duplicate normalized paths,
case collisions, excessive counts/depth/sizes/ratios, and CRC failures before
promotion. The 4.2 line contains path/symlink hardening, but extraction never
uses its convenience extract-to-path API: selected data is streamed to safe,
generated staging filenames. The open 2026 leak report and future advisories
remain upgrade-review inputs. Fuzz and malicious-archive tests are mandatory.

### Metadata: durable JSON manifests, no SQLite

Use the already-pinned nlohmann/json and a schema-versioned metadata snapshot.
The initial expected library is small enough that a single-writer service can
copy an immutable in-memory snapshot, write canonical JSON to a randomized
temporary file, flush it, atomically replace `metadata/index-v1.json`, and
retain one verified backup. Pack/object/association collections remain
conceptually separate even though stored in one document. Liveness is derived
from associations, pins, and active/pending leases rather than a manually
maintained reference count.

JSON avoids adding a second persistence dependency and makes recovery/audit
straightforward. Its tradeoff is serialized writes and whole-index replacement;
this is accepted for schema 1 and must be benchmarked. SQLite remains the
documented migration candidate if index size or cross-process write contention
makes bounded atomic JSON inadequate. Unknown future schema versions are
rejected without deletion.

### SHA-256: platform cryptography APIs

Use Windows CNG (`BCrypt` SHA-256) and macOS CommonCrypto SHA-256 behind one
streaming `ContentHasher` interface. Both are system libraries already present
on supported platforms, add no vendored crypto, accept bounded incremental
updates, and allow cancellation between reads. Canonical object IDs are 64
lowercase hexadecimal characters. Tests use standard vectors and streamed
files. A dependency-free custom cryptographic implementation is rejected.

## Dependency alternatives considered

| Capability | Alternative | Decision |
|---|---|---|
| ZIP | Handwritten central-directory and Deflate implementation | Rejected: violates the no-custom-decompressor rule and expands parser risk. |
| ZIP | Legacy zlib `contrib/minizip` | Rejected: weaker maintenance/security history and less suitable metadata/safety surface. |
| ZIP | `richgel999/miniz` | Rejected for this milestone because ZIP64 metadata work remained visibly active and policy hooks are less complete. |
| ZIP | libarchive 3.8.8 | Capable and maintained, but its multi-format surface and larger build/dependency set are unnecessary for ZIP-only local import. |
| ZIP | OS shell/archive APIs | Rejected: inconsistent Windows/macOS metadata, cancellation, link, and limit behavior. |
| Metadata | SQLite | Deferred: excellent transactions/locking but a new compiled dependency is not yet justified by expected index scale. |
| Metadata | One manifest per object/pack only | Rejected as sole index: browsing and atomic multi-association publication become harder to reconcile. |
| Hashing | Custom SHA-256 | Rejected: system crypto is available and preferable. |

## Candidate implementation locations

```text
NeuralAmpModeler/Library/
  ContentHash.*             platform SHA-256 and canonical hex
  LibraryTypes.h            source modes, errors, tasks, plans, records
  LibraryPaths.*            known folders and descendant/path policy
  LibraryIndex.*            schema-1 JSON snapshot and atomic transaction
  ManagedLibrary.*          object promotion, deduplication, leases, verify
  ImportWorker.*            joined newest-task worker and progress snapshots
  FolderScanner.*           bounded recursive review plan
  ZipArchive.*              minizip-ng scan and selective stream extraction
  PathSafety.*              archive/folder normalization and collision policy
  CaptureRole.*             advisory inference and user override
ThirdParty/minizip-ng/       pinned reader sources and license
ThirdParty/zlib/             pinned Deflate sources and license
tests/milestone3/            generated unit/security/transaction fixtures
```

Product integration remains narrow: `NeuralAmpModeler.cpp/.h` owns the library
service and import worker, routes managed selections to `_RequestModel()` or
`_RequestIR()`, polls progress in `OnIdle()`, and appends optional managed
references to state. `NeuralAmpModelerControls.h` adds drop intent and the
minimal review/library/settings surface. Existing model/IR preparation sources
remain untouched except for receiving resolved ordinary file paths.

## Library layout and schema direction

```text
<AppSupport>/Amphibia/Library/
  objects/nam/<first-two-hex>/<sha256>.nam
  objects/ir/<first-two-hex>/<sha256>.wav
  packs/<pack-key>/manifest.json
  metadata/index-v1.json
  metadata/index-v1.json.bak
  metadata/schema-version
  staging/<unique-task>/
  quarantine/
  logs/
```

Physical names are derived only from a verified hash and validated type.
Original names and nested relative paths are metadata. Object publication is a
same-filesystem no-replace/verify operation after copy/extraction, inspection,
hash, copied-hash verification, and free-space checks. Metadata publishes only
after every selected object's physical outcome is known. Pack removal and
cleanup are separate transactions. Unknown files are reported, not deleted.

Schema 1 has separate object, pack, pack-entry, user-classification, pin, and
recent-activation records. Pack keys are stable random IDs for ordinary group
imports and archive-hash-derived IDs plus a disambiguator for ZIP associations.
Multiple source paths/names can associate with one object hash. A process-wide
service serializes threads; an Amphibia-namespaced interprocess library write
lock serializes writers; readers use the last complete snapshot. Unique staging
directories and atomic object creation make concurrent identical imports safe.

## Limits fixed for schema 1

- ZIP input: 2 GiB.
- Archive/folder entries: 10,000.
- Directory depth: 32.
- Normalized relative path: 512 UTF-8 bytes; component: 255 UTF-8 bytes.
- NAM: 512 MiB (matching Milestone 2).
- WAV: 64 MiB for permanent promotion (matching the actual Milestone 2 bound,
  deliberately stricter than the brief's 2 GiB suggestion).
- Metadata text: 10 MiB each, at most 32 README/license candidates.
- Images: listed as unsupported metadata and not extracted in schema 1.
- Total selected/extracted bytes: 8 GiB, additionally capped by available
  space with a safety reserve.
- Compression ratio: 200:1 when uncompressed size is nonzero.
- Local logs: bounded; source paths are basename/relative-path redacted.

Limits apply to declared scan data and actual streamed bytes. There is no
disable-all-limits setting.

## Upstream merge boundaries

- Do not edit iPlug2, NAM Core, AudioDSPTools, or their directory behavior.
- Preserve the `NAMFileBrowserControl` non-recursive menu and previous/next
  implementation for referenced folders.
- Keep `_RequestModel()`, `_RequestIR()`, request IDs, processing generations,
  one-slot activation/completion, worker ownership, and reclamation unchanged.
- Keep DSP order, A1/A2 construction, IR decoding/resampling/kernel limits,
  parameter IDs/order, product identifiers, and legacy readers unchanged.
- Keep archive/library types provider-neutral; no TONE3000, network, OAuth,
  URL, cloud, telemetry, or hosted-content concepts enter Milestone 3.
- Compile new sources explicitly with isolated intermediate outputs; do not
  reintroduce the inherited cross-configuration `dsp.obj` collision.

## Required proof before completion

Generated tests must cover hashing, normalization, collisions, links/special
entries, ZIP64, Unicode, CRC/truncation/encryption/method failures, scan limits,
actual-byte limits, cancellation, selective extraction, copied-hash checking,
deduplication and shared associations, JSON transactions/reopen/rollback/future
schema, recovery/verification, two writers, state compatibility, and managed
load routing. Product builds and Milestone 2/identity/characterization tests
must remain green. macOS and host/manual gaps must be reported, never inferred.
