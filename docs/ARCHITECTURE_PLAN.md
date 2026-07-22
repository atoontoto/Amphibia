# Amphibia architecture plan

Status: Milestone 0 design baseline with Milestone 2 implementation addendum

Evidence date: 2026-07-21

Upstream baseline: `sdatkinson/NeuralAmpModelerPlugin` `v0.7.15` at `96337e9ab6e3beb619459779bbb5c47e1b04d8c4`

This document is a design gate, not an implementation claim. No Amphibia product code is introduced in Milestone 0.

Milestone 2 implementation note (2026-07-22): ADR-002 and ADR-003 are now
implemented at the existing plug-in seam as two independent, per-instance
`AsyncDSPWorker` objects rather than one combined `ProcessingAssetSet`. Model
and IR requests, preparation, activation, and reclamation remain independent,
so a slow NAM request cannot prevent an IR request from becoming ready. The
one-element ready and completion mailboxes preserve the original bounded
handoff intent. The provider-neutral coordinator and proposed source-tree
migration remain Milestone 3 work; `DSP_OWNERSHIP.md` is the authoritative
record of the concrete Milestone 2 ownership protocol.

## Product boundary

Amphibia will be a distinct, free, open-source, non-commercial desktop application and VST3 derived from Neural Amp Modeler (NAM). It will preserve the upstream local `.nam` and mono `.wav` workflow while adding an optional provider-neutral browser, managed downloads, and TONE3000 as the first hosted provider.

The initial supported products are:

- Windows x64 standalone application and VST3.
- macOS standalone application and VST3, with universal binaries where the pinned toolchain supports them.
- Local files always available without authentication or network access.
- Hosted content opt-in only; no catalog mirroring, silent downloads, or redistributed models.

AU, AAX, iOS, and web targets are not release targets for Amphibia. Their inherited identifiers still have to be neutralized or removed so they cannot accidentally ship as NeuralAmpModeler artifacts.

## Investigation findings that shape the design

1. The pinned plugin is the current stable upstream release and pins NAM Core `v0.5.3`. It builds the A2 fast path on Windows and macOS. A2 is a recognized WaveNet shape, not a separate top-level `.nam` `architecture` string.
2. Upstream model and IR loading is not asynchronous. File parsing, model construction/prewarming, WAV decoding, resampling, and convolution preparation run synchronously in the caller of `_StageModel()` or `_StageIR()`.
3. `ProcessBlock()` calls `_ApplyDSPStaging()`. Moving a staged `unique_ptr` over the active object can destroy the old model or IR on the audio thread. Amphibia must move both preparation and retirement off that thread.
4. The upstream file browser adds only the selected file's containing directory and constructs `IDirBrowseControlBase` with recursive scanning disabled. Previous/next wrap over that non-recursive, sorted menu. This behavior is part of the compatibility contract.
5. Plugin state stores exact model and IR paths plus parameters under the `###NeuralAmpModeler###` header and contains several backward readers. A failed restore leaves the plugin operational and marks the file as failed.
6. iPlug's standalone host writes only device, driver, sample-rate, buffer, channel, and MIDI preferences to `settings.ini`. It does not call the plugin's state serializer. Amphibia needs explicit standalone session persistence.
7. The Windows installer has no explicit Inno Setup `AppId`; macOS and VST3 identities derive from inherited NAM values. Shipping before the identity audit would create collision and upgrade risk.
8. TONE3000's free native-app terms permit hosted prompt flows and bounded list endpoints, but do not grant unrestricted native search. The initial “Search TONE3000” action must therefore launch the hosted `select_tone` flow.

## Architectural decisions

| ID | Decision | Reason and consequence |
|---|---|---|
| ADR-000 | Keep the pinned upstream submodules and source layout until the first green renamed build. | Reduces rebase risk and makes behavioral comparison possible. |
| ADR-001 | Put all catalog and download behavior behind `IContentProvider`. | Local browsing remains first-class and a provider can be disabled without touching DSP code. |
| ADR-002 | Use an immutable, fully prepared `ProcessingAssetSet` handed to the audio thread through a bounded real-time bridge. | The audio thread performs no I/O, JSON/WAV parsing, allocation-heavy preparation, locking, networking, or destruction. |
| ADR-003 | Retire replaced DSP objects on a non-real-time reclamation thread. | An atomic pointer exchange alone is insufficient if the previous object's destructor runs in `ProcessBlock()`. |
| ADR-004 | Preserve exact local navigation semantics in `LocalFileProvider`: same directory, matching extension, non-recursive, deterministic natural/locale-independent ordering, wraparound. | Existing users must not lose the compact local workflow. Sorting will be characterized against upstream before replacement. |
| ADR-005 | Introduce an Amphibia versioned state envelope and keep a legacy NAM reader. | New provider metadata and managed-cache references need structure, while old sessions should remain recoverable where practical. |
| ADR-006 | Store access/refresh tokens only in the OS credential store; persist only an opaque account key and non-secret metadata in product state. | A plugin state chunk may be copied, shared, or inspected and is not a secret store. |
| ADR-007 | Download an explicitly selected individual model/IR, not a whole-tone archive. | This matches the provider's documented Select example, minimizes data, and avoids accidental redistribution. |
| ADR-008 | Use SQLite only if a Milestone 3 benchmark proves JSON plus atomic replacement inadequate. | Avoids adding an unnecessary dependency; the initial library index can be a small schema-versioned JSON file guarded by a single-writer service. |
| ADR-009 | Treat “A1,” “A2 Nano,” and “A2 Standard” as inspected descriptors; otherwise show the underlying architecture as Custom/Unknown. | A WaveNet string by itself cannot prove A1 or A2. |
| ADR-010 | Keep all provider UI non-modal with respect to audio processing. | Authentication, downloads, cancellation, and failures must not mute or replace the current valid DSP asset. |

## Component model

```text
UI / host state callbacks
        |
        +--> ContentBrowserController
        |       +--> LocalFileProvider
        |       +--> IContentProvider --> Tone3000Provider
        |       +--> DownloadCoordinator --> SecureArchiveImporter
        |       +--> LibraryService --> atomic JSON index + managed files
        |       +--> CredentialStore / OAuthLoopbackServer
        |
        +--> AssetLoadCoordinator (worker queue, cancellation, generations)
                    |
                    +--> NamInspector --> NAM Core construction/prewarm
                    +--> IrInspector  --> WAV decode/resample/prepare
                    +--> StateResolver (exact path, cache key, recovery UI)
                    |
                    +--> RealtimeAssetExchange --> ProcessBlock
                                                     |
                                             ReclamationQueue
                                                     |
                                           non-RT destruction
```

### `IContentProvider`

The product layer consumes stable domain types rather than TONE3000 JSON:

```cpp
struct ContentQuery;
struct ContentPage;
struct ContentItem;
struct AssetVariant;
struct DownloadRequest;
struct DownloadReceipt;

class IContentProvider {
public:
  virtual ProviderCapabilities Capabilities() const = 0;
  virtual AsyncResult<ContentPage> Browse(const ContentQuery&, CancellationToken) = 0;
  virtual AsyncResult<ContentItem> Resolve(ContentId, CancellationToken) = 0;
  virtual AsyncResult<DownloadReceipt> Download(const DownloadRequest&, CancellationToken) = 0;
  virtual ~IContentProvider() = default;
};
```

`ProviderCapabilities` advertises hosted selection, pagination, exact variants, authentication, and supported asset types. It does not promise arbitrary search. TONE3000's initial adapter will expose hosted selection plus post-selection detail/model resolution. `LocalFileProvider` exposes directory navigation and never needs authentication.

### Loading and real-time handoff

Each request gets a monotonically increasing generation. Workers may finish out of order, but only the newest non-cancelled generation may publish.

1. UI or state restore submits a path/cache reference without altering the active DSP.
2. A worker applies path policy, bounded-size checks, format inspection, and content hashing.
3. A worker constructs and prewarms the NAM processor or decodes/resamples/prepares the IR for the current sample rate and maximum block size.
4. The corresponding per-stage worker publishes one immutable activation record to its one-element atomic ready mailbox.
5. At a block boundary, `ProcessBlock()` exchanges the active pointer using bounded operations only.
6. The old pointer is returned in that stage's one-element completion mailbox and destroyed by its non-real-time worker.
7. If the completion mailbox is occupied, activation is deferred; audio continues with the last valid object. A newer pending request replaces the older pending request before preparation.

The concrete pointer atomics are required to be always lock-free by compile-time
assertions and the bridge has deterministic stress coverage. `ProcessBlock()`
contains no loading-related mutex acquisition, wait, file/network activity,
parser work, allocation, string operation, or memory reclamation. General
inherited DSP helpers and a defensive host-contract buffer grow are separately
documented limitations in `THREADING.md`; Milestone 2 does not claim a global
allocator trap over the complete inherited signal path.

`OnReset()` presents a related hazard because upstream resets/rebuilds staged and active assets. Amphibia will create sample-rate-specific prepared replacements off-thread and atomically adopt them. Until ready, it may retain a compatible current asset or use a documented bypass/fallback; it must not construct an IR on the audio callback.

Milestone 2 implements the conservative option: configuration generation is
incremented, existing stage objects are retained but marked incompatible and
bypassed, and matching replacements are prepared asynchronously. The audio
callback never resets or reconstructs the old objects.

## State model

The proposed Amphibia envelope is length-delimited and versioned:

```text
magic = "###Amphibia###"
schema_version
parameter_state
model_reference { kind, exact_path?, provider?, asset_id?, variant_id?, sha256?, display }
ir_reference    { kind, exact_path?, provider?, asset_id?, variant_id?, sha256?, display }
ui_state        { selected_tab, browser_filters }
```

Secrets, bearer tokens, authorization codes, PKCE verifiers, and presigned URLs are forbidden in this envelope. Hosted references resolve to a verified local managed-cache file. Restore is asynchronous and generation-safe. If a file is missing or invalid, parameters still restore, the current/empty safe DSP state remains usable, and the UI offers locate, re-download, or clear.

The reader order will be:

1. Current Amphibia schema.
2. Older Amphibia schemas through explicit migrations.
3. Existing NAM `###NeuralAmpModeler###` state using the retained upstream reader.
4. Existing unknown-version NAM fallback, only as long as its tests prove it safe.

Standalone session state will use the same serialized plugin state but live beside—not inside—the iPlug audio/MIDI `settings.ini`. Writes use temp-file, flush, and atomic replace. VST3 hosts remain authoritative for plugin state.

## Filesystem layout

Final directory APIs will use platform-known folders; the paths below are logical examples.

```text
Windows: %LOCALAPPDATA%\Amphibia\
macOS:   ~/Library/Application Support/Amphibia/

Amphibia/
  state/standalone-state.bin
  library/index-v1.json
  library/models/<sha256>.nam
  library/irs/<sha256>.wav
  downloads/<request-id>.partial
  logs/amphibia.log
```

OAuth credentials are stored outside this tree in Windows Credential Manager or macOS Keychain under a new Amphibia service identifier. The installer/uninstaller must not remove this user data by default. A separately confirmed “remove user data” action may be added later.

## Proposed source tree

The existing submodules remain at repository root. Product code will migrate incrementally to:

```text
Amphibia/
  Amphibia.cpp
  Amphibia.h
  config.h
  dsp/
    AssetLoadCoordinator.*
    NamInspector.*
    IrInspector.*
    ProcessingAssetSet.*
    RealtimeAssetExchange.*
  content/
    ContentTypes.*
    IContentProvider.h
    LocalFileProvider.*
    ContentBrowserController.*
    library/LibraryService.*
    network/HttpClient.*
    network/DownloadCoordinator.*
    security/SecureArchiveImporter.*
    tone3000/Tone3000Provider.*
    tone3000/Tone3000OAuth.*
  platform/
    CredentialStore.h
    KnownFolders.*
    OpenExternalBrowser.*
    LoopbackServer.*
    macos/CredentialStoreMac.mm
    windows/CredentialStoreWin.cpp
  state/
    AmphibiaState.*
    LegacyNamStateReader.*
  ui/
    CompactView.*
    ContentBrowserView.*
    StatusModel.*
  resources/
  installer/
  projects/
tests/
  unit/
  integration/
  fixtures/
docs/
tools/
```

The exact project file organization will be chosen in Milestone 1 after the renamed baseline builds. Moving everything before that proof would make build failures harder to attribute.

## UI integration

The compact main view retains model and IR file buttons, labels, previous/next actions, clear actions, and all DSP controls. A browse/search affordance opens a larger content surface; it does not replace the compact controls.

The content surface has explicit Local and Online sources. Local is usable offline and preserves current navigation. Online shows provider attribution at the entry point and item level, communicates login/download states, and never describes Amphibia as officially endorsed or partnered without written approval.

Selection is two-step for hosted content: select a tone/IR, then select one exact variant when multiple models exist. The UI records and restores that exact variant ID and hash.

## Validation gates

- Characterization tests lock current parameter defaults, DSP ordering, local navigation, and legacy state parsing before refactoring.
- Real A1 and released A2 fixtures load and process finite audio at multiple sample rates and block sizes.
- Audio-thread instrumentation fails tests on allocation, mutex acquisition, file/network calls, or destruction in the callback.
- Cancellation and rapid switching never publish a stale generation.
- Missing files, corrupt `.nam`, malformed/oversized WAV, interrupted downloads, expired tokens, unavailable provider, and offline startup retain a valid audio state.
- Standalone settings and session state survive restart; missing audio hardware falls back without losing content state.
- Product IDs, paths, credentials, mutexes, installers, and bundles do not collide with NeuralAmpModeler.

## Milestone 0 open decisions

- The repository owner/organization and final reverse-DNS namespace are not provided. Milestone 1 must allocate identifiers after that owner string is chosen.
- No production TONE3000 public client ID or registered desktop redirect URI is present. Hosted authentication cannot be release-tested until those non-secret registration values are supplied.
- TONE3000 documentation does not promise wildcard/random loopback ports for production desktop clients. The loopback design remains conditional; see `TONE3000_INTEGRATION.md`.
- A production HTTP/TLS implementation and optional archive implementation are not selected. License, platform, certificate, redirect, and cancellation behavior must be evaluated before addition.
- Existing fonts, icons, and artwork have no adjacent license/provenance records in this checkout. They are quarantined from Amphibia release artifacts until replaced or documented.

## Primary evidence

- Upstream plugin release: <https://github.com/sdatkinson/NeuralAmpModelerPlugin/releases/tag/v0.7.15>
- Pinned NAM Core release: <https://github.com/sdatkinson/NeuralAmpModelerCore/releases/tag/v0.5.3>
- NAM file specification: <https://neural-amp-modeler.readthedocs.io/en/latest/model-file.html>
- A2 release announcement: <https://www.neuralampmodeler.com/post/a2-is-released>
- TONE3000 API: <https://www.tone3000.com/api>
- TONE3000 API terms: <https://www.tone3000.com/api/terms>
- iPlug2 documentation: <https://iplug2.github.io/docs/>
# Milestone 3 local content slice

Milestone 3 adds a provider-neutral local content boundary under `NeuralAmpModeler/Library/`: source/plan/result types, path policy, recursive scanner, ZIP inventory/extractor, OS SHA-256, schema-1 JSON index, managed object transactions, and a joined import/maintenance worker. UI drag/drop and Settings actions are adapters; they do not own archive or database policy. `ManagedLibrary` is the only publisher of permanent objects/metadata, and `_RequestModel()` / `_RequestIR()` remain the only product loading gateways.

The managed root is optional and separate from referenced browsing. Its content-addressed objects and pack associations are suitable inputs for a future provider adapter, but no provider/network concept enters this slice. The existing upstream-heavy DSP/UI translation unit includes the library implementation at its established amalgamation boundary; vendored C readers are explicit app/VST3 build inputs. Future extraction into a standalone library target is allowed if it preserves these APIs and merge boundaries.
