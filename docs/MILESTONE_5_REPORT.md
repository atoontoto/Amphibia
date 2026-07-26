# Milestone 5 report - hosted TONE3000 exact selection

Date: 2026-07-26

Status: implementation complete within the available registration boundary; live production authorization remains blocked and is not claimed.

## Outcome

Milestone 5 turns the Milestone 4 provider foundation into an additive hosted-selection feature model:

```text
explicit Search/Find action
  -> TONE3000 hosted select_tone request
  -> exact callback-selected tone ID
  -> exact tone metadata
  -> bounded all-page exact variant list
  -> native compatibility/attribution panel
  -> explicit exact-variant action
  -> refreshed exact model identity
  -> streamed staging/validation/SHA-256/promotion/provenance
  -> optional separate managed activation request
  -> Active only after Milestone 2 confirmation
```

The repository build is intentionally Unconfigured. `Search TONE3000` and `Find IR on TONE3000` are visible but disabled with the required explanatory tooltip. Construction, scan, state restore, and audio processing start no provider receiver, browser, credential operation, DNS, HTTP request, download, or login UI.

The original referenced-file model/IR controls, local folder previous/next behavior, parameters, DSP order, A1/A2 loaders, managed imports, ZIP behavior, state readers/writer, product identity, and settings namespace are unchanged.

## Registration and callback result

`Tone3000Configuration` now centralizes:

- `Tone3000RegistrationState::{Unconfigured, DevelopmentConfigured, ProductionConfigured}`;
- callback strategy;
- publishable client ID and exact redirect;
- approved application name and brand-asset flag;
- final privacy/repository/support fields; and
- production/development/test restrictions.

The validator rejects missing or control-character-bearing fields, client-secret-looking IDs, non-HTTPS provider endpoints, a redirect inconsistent with its strategy, test HTTP/development mocks in Production, incomplete public fields, and a Production client ID that is not publishable. Invalid provider configuration disables only the integration.

No client ID, registered redirect, callback claim, brand asset, privacy URL, final repository URL, or support contact was invented. `ConfiguredTone3000()` therefore returns Unconfigured.

`IHostedSelectionCallbackReceiver` defines the bounded receiver lifecycle. The shipped receiver fails closed because TONE3000 registration has not confirmed fixed loopback, random loopback, or custom URI for Amphibia. Deterministic tests inject a development receiver and prove start-before-browser, exact redirect handoff, completion, cancellation, stale callback suppression, and shutdown behavior. This is not a live callback pass.

## Hosted request constraints

Only current documented hosted values are used:

| Entry | Prompt | Format | Gears | Architecture |
| --- | --- | --- | --- | --- |
| NAM A1/custom | `select_tone` | `nam` | `amp_amp-cab` | omitted |
| NAM A2 | `select_tone` | `nam` | `amp_amp-cab` | `2` |
| IR | `select_tone` | `ir` | `cab` | omitted |

Omission is kept as the documented A1/custom view; it is not falsely called an all-architecture query. The native pre-selection offers A1/custom and A2 separately. There is no `/tones/search` call, native catalog search, scraping, crawling, mirroring, recommendation, automatic matching, or whole-tone ZIP.

## Coordinator and lifecycle

`Tone3000HostedSelectionCoordinator` owns one copied task snapshot and explicit states for waiting, exact metadata, listing, ready, exact refresh, downloading, downloaded, preparing, active, failed, and cancelled. It provides:

- five-minute hosted-flow deadline passed to the receiver;
- task IDs and weak-lifetime/generation checks;
- callback cancellation as a normal terminal result;
- reopen of the same authorization URL only on explicit action;
- exact tone ID validation and exact API re-resolution;
- bounded exact-model listing and stable deterministic sorting;
- compatible/unavailable classification with a visible reason;
- one-entry preselection without automatic download;
- local bounded filter over only the fetched pack;
- safe grouping from relative path, then format/architecture fallback;
- explicit Download Only and Download and Audition;
- exact-model refresh and identity/format/architecture recheck before download;
- exact cached association recognition by provider/tone/model tuple;
- downloaded-only previous/next navigation;
- explicit local-copy removal callback;
- separate download, preparing, and active stages; and
- shutdown/cancel stale suppression.

A Windows named semaphore or macOS `flock` lease prevents two instances from owning the same hosted browser/callback flow. It is separate from the Milestone 4 credential refresh lock and can be released from an asynchronous callback thread.

## Exact metadata and pagination

Tone parsing preserves bounded plain-text identity, title, creator, description, format, gear, source page, tags, optional image URLs, and unknown-preserving license values. Images are not fetched in this milestone.

Models are paged at the documented maximum of 300, with hard limits of 16 pages and 4,800 variants. The adapter rejects:

- missing or duplicate model IDs;
- page number mismatch;
- changing total/total-pages metadata;
- repeated non-empty page signatures;
- empty intermediate pages;
- more records than the declared total;
- a final count different from the declared total; and
- response/tree/string/count limits inherited from Milestone 4.

Short-lived `model_url` values remain only in provider task memory. They are not written to library metadata, state, filenames, diagnostics, or settings.

## Native UI

The fixed 600 x 400 product UI now contains visible additive buttons beside the existing selectors:

- `Search TONE3000`
- `Find IR on TONE3000`

The existing local file controls remain primary and retain Load, directory/file chooser, local Previous/Next, filename/menu, Clear, drag/drop, and inherited Get links.

The native bounded panel supports:

- A1/custom versus A2 hosted pre-selection;
- waiting text, Open Browser Again, Cancel, and Escape;
- title, creator, format, gear, description, license, and source action;
- pack-local text filter;
- six bounded visible variant rows;
- architecture, format, optional declared bytes, compatibility, downloaded, and active labels;
- explicit Download and Audition / Download Only;
- Previous/Next downloaded variant;
- Remove Local Copy; and
- Open on TONE3000.

Remote text is displayed as bounded plain text, never HTML or commands. Provider model ID is retained for diagnostics/identity but not exposed as a primary label. The initial UI does not fetch avatars/images because image cache policy and approved branding are not complete.

Because the live provider object/approved receiver cannot be constructed in this repository build, all provider actions remain disabled. The compiled panel and coordinator contract are ready for owner-reviewed wiring once registration exists; the report does not claim a clickable live flow.

## Download, library, and activation

Only an explicitly selected compatible exact variant can download. The provider first refreshes the exact model and then uses the Milestone 4 individual downloader:

```text
bearer-authorized individual model URL
  -> randomized payload.part
  -> actual-byte cap
  -> verified.nam or verified.wav
  -> Milestone 2 inspection
  -> streaming SHA-256
  -> managed content-addressed promotion
  -> schema-2 provider association
```

The coordinator does not call DSP. For audition it emits a task ID, managed SHA-256, and NAM/IR kind. The product integration must resolve that hash and use `_RequestModel()` or `_RequestIR()`. `NotifyActivation` changes Preparing to Active only after the existing loader confirms success; a failure returns to Downloaded and preserves the old stage.

Exact cache lookup matches `(tone3000, tone_id, model_id)`. Removal rejects an active protected hash, removes the exact association, preserves another association/pack reference or pinned object, and deletes an unshared object only after metadata commit. Disconnect remains credential-only and leaves downloaded bytes/attribution intact.

Library schema stays at 2 and DAW state stays at 1.1. Hosted panel/filter/request/download transient state is not serialized. Offline project restoration continues to resolve only the managed SHA-256 and performs no provider work.

## Security and privacy review

- No OAuth/client secret, credential, signed URL, callback URI, or real user fixture was added.
- No provider operation is reachable from `ProcessBlock()`.
- No automatic browser launch occurs at startup, scan, UI open, or project restore.
- No embedded login/webview, LAN listener, permanent server, telemetry, upload, scraping, or catalog cache exists.
- OAuth state/verifier/code remain single-use and bounded.
- Callback receiver begins before browser launch and stops on every terminal flow.
- Cross-instance hosted ownership and credential refresh ownership are separate.
- Exact tone/model identity is checked again before download.
- Download success cannot imply DSP activation.
- Remote descriptions/tags/paths remain bounded plain text.
- Source links are provider-preserved public URLs; signed model URLs are transport-only.

## Deterministic evidence

Milestone 5 native tests cover:

1. production/development/unconfigured validation;
2. completely inert unconfigured selection;
3. documented NAM hosted constraints;
4. exact tone and all-variant listing before any download;
5. A1/A2/unknown compatibility behavior;
6. explicit exact refresh/download/audition and independent Active confirmation;
7. documented A2 and IR constraints;
8. already-downloaded content requiring explicit audition;
9. cancel plus stale callback suppression;
10. callback failure reason preservation;
11. bounded pack-local filter and grouping; and
12. cross-instance hosted-selection lease contention/release.

The inherited Milestone 4 suite additionally covers secure download/promotion/provenance and now verifies exact association lookup, active-hash removal protection, exact removal, and preservation of unrelated provider content.

Final local Windows evidence:

| Check | Result |
| --- | --- |
| Milestone 5 Debug clean rebuild and native run | 9/9 tests passed; 0 compiler warnings, 0 errors |
| Milestone 4 Debug native run | 20/20 tests passed |
| Milestone 3 Debug rebuild and native run | passed, including provider provenance/removal and performance checks; 0 compiler warnings, 0 errors |
| Milestone 2 native run | passed; maximum measured activation call 11,800 ns |
| Identity validator | passed; 35 identity-bearing files checked |
| Inherited characterization contract | passed |
| State compatibility contract | passed |
| Windows x64 Release VST3 | built successfully; 0 errors, 12 inherited numeric-conversion warnings |
| Windows x64 Release standalone | built successfully; 0 errors, 23 inherited numeric-conversion warnings |

The first sandboxed Milestone 3 execution could not write its normal Windows temporary library and reported an import failure. The same rebuilt executable passed outside the restricted filesystem sandbox; this was an execution-environment restriction, not a product/test failure. MSBuild FileTracker likewise required the already-documented unsandboxed build context for some test projects.

## Contract preservation

No intentional changes were made to:

- Amphibia name/version/codes/FUIDs/project GUIDs/AppId/bundle/settings identity;
- parameter count, order, IDs, defaults, or serialization;
- DSP order or `ProcessBlock()` provider reachability;
- A1, A2, resampling, Slim, IR, and failure-preserves-active behavior;
- local NAM/IR dialogs, folder navigation, previous/next, drag/drop, or referenced paths;
- managed object hashing/path identity, ZIP reader policy, or schema-1 migration;
- state 1.0 prefix, 1.1 tail, legacy NAM readers, or offline restore; or
- Milestone 4 HTTPS, credential, redirect, rate, retry, and download security boundaries.

## Explicit non-claims and remaining blockers

Milestone 5 does not claim:

- a production TONE3000 client registration;
- a provider-approved fixed/random loopback or custom URI receiver;
- live authorization, token, metadata, rate-limit, or download behavior;
- approved TONE3000 logo/brand asset packaging;
- final privacy/repository/support registration values;
- macOS HTTP transport, product integration, build, or runtime validation;
- interactive standalone/VST3/DAW browser callback validation;
- remote avatar/image fetching or cache cleanup;
- automatic missing-object redownload;
- installer, signing, notarization, release readiness, or partnership/endorsement.

The live integration is blocked, not simulated. The safe next step is to obtain written registration details and approved assets, implement exactly that receiver, inject it at product construction, route its activation callback through the existing managed request methods, and run development/live cross-platform host validation without weakening the current gate.
