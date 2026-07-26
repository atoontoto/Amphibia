# Milestone 5 TONE3000 hosted-selection map

Status: pre-implementation UX, ownership, and integration gate

Evidence date: 2026-07-26

Baseline: completed Amphibia Milestones 0-4 on `main`, product version
`0.1.0`. This map was created before any Milestone 5 product UI change.

TONE3000 is an optional independent service. This design does not claim
endorsement, production registration, broader catalog access, or approval of
any callback or brand treatment.

## Confirmed registration and release status

The checked-in product configuration is `UnconfiguredTone3000()`. There is no
publishable client ID, registered redirect URI, approved desktop callback
method, approved application name, approved brand asset, final privacy URL,
final repository URL, or support contact in the repository. No client secret
is accepted by the client configuration.

Consequences for the default build:

- local NAM/IR loading and the managed library remain fully available;
- the two hosted-selection entry points can be visible but must be disabled
  with `TONE3000 integration is not configured in this build.`;
- no browser, listener, credential lookup, DNS, HTTP, or download may start;
- production callback and live-provider validation remain blocked;
- approved branding remains a public-release blocker; no logo is vendored or
  inferred from a website.

Milestone 5 will distinguish `Unconfigured`, `DevelopmentConfigured`, and
`ProductionConfigured`. Development mode is visibly labelled and accepts only
an explicitly registered development redirect. Production mode is usable only
after every registration/branding/URL field passes centralized validation.

## Existing compact controls

The fixed 600 x 400 layout is assembled in
`NeuralAmpModeler/NeuralAmpModeler.cpp` and uses controls in
`NeuralAmpModeler/NeuralAmpModelerControls.h`.

### NAM selector

`kCtrlTagModelFileBrowser` owns a `NAMFileBrowserControl` over `modelArea`.
Its existing child controls are:

- load-file button (or directory chooser on the inherited macOS standalone
  `NAM_PICK_DIRECTORY` path);
- previous local NAM;
- next local NAM;
- filename/menu button;
- mutually visible Clear or inherited `Get NAM Models` browser-link button;
- single/multiple drop handling.

`IDirBrowseControlBase` remains non-recursive. Previous/next wrap within the
current local directory. A choice submits `_RequestModel()`; none of these
controls becomes a provider-pack navigator.

### IR selector

`kCtrlTagIRFileBrowser` owns the same `NAMFileBrowserControl` over `irArea`
with `.wav`, `_RequestIR()`, previous/next local IR, Clear or inherited
`Get IRs`, and independent drag/drop. `kIRToggle` remains a separate bypass
switch. Provider audition must not replace or merge these navigation rules.

### Active display and loading status

The filename controls show the committed model/IR basename. `_PollLoadingStatus()`
maps Milestone 2 `Inspecting`, `Preparing`, `ReadyToActivate`, `Active`, and
`Failed` snapshots to those controls. Active paths and managed hashes are
committed only after `Active`. A failed request preserves the prior DSP and
the label identifies the still-active basename.

`OnUIOpen()` reconstructs both selectors from committed paths and retained DSP
state. Provider status/panels must follow the same reconstruction rule and
must never be owned only by raw control pointers.

## Existing Settings diagnostics

`NAMSettingsPageControl` receives `RemoteProviderUIActions`. Milestone 4 wires
only an information callback returning text equivalent to:

```text
TONE3000 integration | Configuration: Not configured |
Connection: Disconnected | API: Offline | Secure storage: Available
```

Connect, Disconnect, Test connection, and Cancel controls exist but are
disabled because their actions and enable flags are empty. Settings refreshes
provider information when opened. Milestone 5 will bind these to copied
provider/coordinator state without displaying tokens, OAuth state, codes,
signed URLs, email, or local staging paths. Account-only Connect remains
disabled unless provider registration explicitly supports promptless account
authorization; selection-triggered authorization is sufficient.

## Existing Milestone 4 provider contracts

### Authorization

`AuthorizationPurpose` contains `ConnectAccount`, `SelectTone`, and the
disabled/ambiguous `ResolveKnownTone`. `AuthorizationRequest` carries purpose,
optional exact tone ID, gear filters, content format, and optional NAM
architecture. `OAuthController` builds only documented values. `SelectTone`
uses `prompt=select_tone`; the unresolved `load_tone`/`get_tone` flow remains
disabled.

Every authorization owns a generated flow ID, CSPRNG state, PKCE verifier and
S256 challenge, and five-minute deadline. Begin supersedes older pending
material. Cancel/shutdown consumes it. Browser launch is behind
`platform::ISystemBrowser::OpenExplicit()` and validates an HTTPS authorization
URL before using OS APIs.

### Callback result

`ParseOAuthCallback()` accepts a maximum 4 KiB callback matching the exact
configured redirect base and rejects malformed/duplicate fields. It produces
`OAuthCallbackValues { state, code?, error?, toneId?, cancelled }`.
`OAuthController::HandleCallback()` consumes pending state before terminal
validation and returns `ConsumedAuthorization { flowId, request,
authorizationCode, verifier, selectedToneId? }`. `Tone3000Provider` exchanges
the code, stores tokens only in the platform credential store, and reports
`AuthorizationResult { selectedToneId?, cancelled }` to its owner.

There is no production callback receiver. Milestone 5 must add an injected
single-flow receiver interface plus a fail-closed unconfigured implementation.
An OS listener or URI registration cannot be selected until TONE3000 confirms
the registered strategy. Deterministic tests may inject a development/mock
receiver; this is not a live-provider claim.

### Exact metadata and variants

`FetchTone(toneId)` calls only `GET /api/v1/tones/{id}` and returns
provider-neutral `RemoteTone`: exact ID, title, bounded plain description,
creator identity/profile/avatar, format, exact gear role, unknown-preserving
license, canonical source page, bounded tags, and image URLs.

`Tone3000ApiClient::FetchModels()` calls only
`GET /api/v1/models?tone_id={id}&page={n}&page_size={n}` with a documented
maximum page size of 300. It returns `RemoteModel`: exact tone/model IDs, name,
format, A1/A2/unknown architecture, declared size, relative path, and transient
download URL. The public provider method currently requests one NAM page;
Milestone 5 must move content kind and bounded all-page traversal into the
selection coordinator, detect repeated/duplicate/malformed pages, and discard
URLs after task completion. `/tones/search` is not and will not be exposed.

`Tone3000ApiClient::FetchModel()` provides exact-model refresh immediately
before download. Milestone 5 must verify that refreshed tone ID and model ID
still match the selected row before accepting bytes.

### Individual download and promotion

`Tone3000DownloadCoordinator` streams one exact `model_url` to randomized
`.part` storage, applies declared/actual byte and free-space limits, closes and
validates NAM/WAV, hashes, and calls `ManagedLibrary::PromoteProviderFile()`.
The result carries the managed object SHA-256. It never activates DSP.

`PromoteProviderFile()` reuses the Milestone 3 inspection/copy/second-hash/
content-addressed transaction and records schema-2 provider provenance.
Identical bytes deduplicate while distinct provider associations remain.
`ManagedLibrary::Resolve(hash, expectedType)` verifies that a managed path is
contained, regular, present, and of the expected type.

### Provenance and active managed identity

Schema 2 retains provider/tone/model IDs and titles, creator, provider license,
canonical source page, format, gear, architecture, verification timestamps,
and object hash. It forbids signed/download URLs and credentials. State 1.1
continues to serialize only the active managed SHA-256; offline restore resolves
that hash without browser, credential, or network work.

`Amphibia::_RequestManaged()` resolves an expected NAM/IR object and then calls
`_RequestModel()` or `_RequestIR()`. `mPendingManagedModel/IR` becomes active
only after the matching Milestone 2 worker reports `Active`; failures clear
pending identity and retain the old active processor. Milestone 5 audition
must use this path exactly.

## Exact Milestone 5 source boundaries

Planned new platform-neutral files:

- `Providers/HostedSelectionTypes.h`: selection kind, pack snapshot, bounded
  variants, compatibility, attribution, action/progress, and copied UI state;
- `Providers/IHostedSelectionCallbackReceiver.h`: exact-redirect,
  single-flow/deadline/cancel/shutdown receiver contract;
- `Providers/Tone3000/Tone3000HostedSelectionCoordinator.*`: task ownership,
  auth callback, exact-tone lookup, bounded pagination, compatibility,
  exact-model refresh, download, and copied status;
- `Providers/Tone3000/Tone3000Registration.*`: centralized registration state
  and validation;
- `Platform/UnavailableHostedSelectionCallbackReceiver.*`: fail-closed product
  receiver until an approved platform strategy is supplied;
- `UI/Tone3000SelectionModel.*` if UI-only filtering/grouping cannot remain a
  small header-only control model.

Existing files requiring integration changes:

- `ProviderTypes.h`, `IRemoteToneProvider.h`, `Tone3000Configuration.h`,
  `OAuthController.*`, `Tone3000ApiClient.*`, `Tone3000Provider.*`;
- `LibraryTypes.h`, `ManagedLibrary.*`, and state code only where exact
  non-secret provider attribution/removal/re-download support needs a stable
  query; schema-1 migration and state 1.0 prefix stay unchanged;
- `NeuralAmpModeler.h/.cpp` for provider ownership, explicit UI actions,
  message-thread polling, managed activation handoff, and shutdown order;
- `NeuralAmpModelerControls.h` for the visible Search/Find buttons, waiting
  surface, bounded exact-variant panel, attribution, and Settings account
  section;
- Windows app/VST3 projects and a new Milestone 5 test project for the new
  sources. Project-default v143 and provider-disabled builds remain intact.

No provider edit belongs in `ProcessBlock()`, NAM/IR DSP classes, ZIP importer,
or `NAMFileBrowserControl`'s local menu/navigation logic.

## Hosted entry points and documented filters

The visible primary entry points are placed adjacent to, not inside or instead
of, the local selectors:

- `Search TONE3000` for hosted NAM selection;
- `Find IR on TONE3000` for hosted cabinet/IR selection.

The NAM selector documentation supports `format=nam`, gear `amp` or
`amp-cab`, and a singular architecture `1`, `2`, or `custom`. Omitting
architecture currently means A1 plus Custom and excludes A2. No multi-value
syntax is documented. Therefore the UI must offer an explicit small choice
between the documented A1/Custom path and A2 path unless TONE3000 confirms a
combined value. A1 is never excluded merely because A2 is supported.

The IR entry uses only documented `format=ir` and cabinet gear constraints.
No provider filter is inferred from filenames and no native search request is
made. The only text filter is labelled `Filter variants in this pack` and
operates on the already fetched bounded in-memory list.

## Final state flow

```text
explicit Search/Find click on message thread
  -> validate registration/platform and acquire hosted-flow ownership
  -> task ID + AuthorizationRequest{SelectTone, documented filters}
  -> approved receiver Start(exact registered redirect, deadline)
  -> PKCE/state + explicit system-browser launch
  -> waiting state (Open Browser Again / Cancel)
  -> receiver delivers one bounded URI
  -> exact callback/state/code/tone_id validation and receiver stop
  -> token exchange/secure credential storage as required
  -> exact tone metadata validation
  -> bounded all-page exact model/IR listing, no download
  -> compatibility classification and native variant panel
  -> explicit exact row plus Download Only or Download and Audition
  -> exact-model refresh and tone/model identity comparison
  -> one individual download -> validation -> SHA-256 -> managed promotion
  -> schema-2 provenance and downloaded UI state
  -> optional explicit `_RequestManaged()`
  -> Milestone 2 Preparing -> audio-boundary activation -> Active
```

Download success and DSP activation success are separate states. The old
model/IR, the other stage, all unrelated parameters, and the selected IR bytes
remain untouched on failure. Amp+cab assistance may offer IR bypass, but may
not delete the IR or silently alter unrelated state.

## Cancellation and error flow

One coordinator-owned generation covers receiver, auth exchange, metadata,
pagination, rate wait, exact refresh, download, validation, hashing, and
promotion. Cancel marks it terminal, cancels the active lower-layer work,
removes `.part` data best-effort, and prevents any late generation from
opening/updating a panel or activating DSP. A successful promotion is not
rolled back when cancellation arrives before optional activation.

User denial/cancellation is a normal Cancelled result. Wrong/missing/replayed
state, missing/invalid tone ID, wrong route, duplicate callback, stale flow,
or tone/model substitution fails without exposing raw query data. Provider,
rate-limit, network, download, promotion, and DSP preparation failures remain
distinct user messages. Authorization is never retried automatically.

Closing the editor hides controls but does not transfer ownership to them.
Reopening reconstructs copied waiting/panel/progress state. Destroying the
plugin invalidates the receiver first, cancels provider work, joins provider
workers, then releases library and DSP ownership.

## Offline, disconnect, and removal flow

Offline project restoration uses the state SHA-256, local provenance snapshot,
and `_RequestManaged()` only. It opens no browser and makes no provider call.
A missing object is reported and can offer explicit Download Again, Choose
Replacement, or Keep Missing; no adjacent variant is substituted.

Disconnect removes the OS credential and cached account identity, cancels
provider tasks, disables new hosted selections, and preserves downloaded bytes,
provenance, and offline state resolution.

Remove Local Copy identifies the exact provider association. It must protect
active/pending/shared objects and distinguish association removal from physical
object deletion. It never deletes provider-side content. This requires a
transactional managed-library association-removal/query API rather than direct
filesystem deletion.

## Multiple instances

The credential namespace and refresh lock already coordinate token rotation.
Milestone 5 needs a separate bounded interprocess hosted-flow ownership lock;
it must not reuse the audio, library-writer, or credential-refresh locks. A
second instance reports that selection is active elsewhere and cannot replace
the first flow's verifier/state or consume its callback. Identical model bytes
still converge through SHA-256 plus the library writer lock; both instances
may resolve/load the same managed object.

Thread simulations can prove single-owner and stale-generation logic, but a
real process/DAW matrix must be reported separately.

## Host/editor and platform lifecycle risks

- iPlug controls are recreated on editor reopen; no worker may retain an
  `IControl*`, `IGraphics*`, or editor callback.
- VST3 construction/scan/state restore must remain inert. Hosts may close an
  editor while keeping the processor alive, switch projects, delete an
  instance, or terminate during a native call.
- Standalone owns the same plugin provider state but may minimize while the
  external browser is open. Device/audio operation remains independent.
- Browser relaunch is explicit only; waiting state never loops or reopens it.
- Native DNS/TLS cancellation can last until finite transport timeout.
- macOS currently lacks `NSURLSession`, product-source integration, validated
  browser/Keychain runtime, and an approved callback registration. Provider
  controls remain disabled there; local macOS behavior must still compile.
- Production callback and brand compliance cannot be claimed from mocks.

## Variant panel model

The native panel is bounded and reconstructible. It shows plain bounded tone
title/description/tags, creator, gear, format, provider-supplied license or
`License information unavailable`, validated source link, returned/compatible
counts, selected exact variant, and separate download/preparation/active state.
Remote HTML/Markdown is never executed. Image loading is deferred initially;
neutral placeholders avoid an unreviewed image parser/network surface.

Rows show safe name, architecture, format, declared size, relative/group path,
downloaded/active/compatibility state, while exact provider IDs remain
diagnostics-only. Unsupported rows remain visible with a reason. A bounded
visible window avoids one heavyweight control per remote entry. One-entry
packs may preselect but never auto-download.

Grouping uses the explicit relative path/common prefix and architecture only;
otherwise it falls back to stable Unicode-safe name/path ordering. It never
manufactures continuous parameters from filenames. Previous/next downloaded
variant controls traverse only already downloaded compatible entries in this
pack and never invoke a network request.

Keyboard behavior where supported: Escape closes/cancels the panel-owned
operation; arrows move the selected row; Enter invokes only the focused
explicit action; status is present as text, not color alone. Double-click does
not download.

## Branding gate

No approved TONE3000 logo or documented asset packaging permission is present.
Milestone 5 can implement truthful text attribution and all creator/license/
source fields, but `docs/TONE3000_BRANDING_COMPLIANCE.md` must leave logo,
clearspace, dark/light asset, and any provider-required visual treatment
blocked until official assets or an approved text-only exception are supplied.
The UI must not say partnered, official, or endorsed.

## Acceptance evidence strategy

Deterministic tests use injected provider, browser, callback receiver, clock,
and files only. They cover hosted success/cancel/security terminal cases,
pagination loops/duplicates/bounds, compatibility, a 300-row page, local
filter/grouping, exact-refresh substitution, download-only versus audition,
old-DSP preservation, provenance/offline/disconnect/removal, state secret
canaries, lifecycle generations, and thread-simulated multiple instances.

Live-provider validation remains:

```text
Live-provider validation not run: registered client configuration unavailable.
```

Windows standalone/VST3 builds and the previous milestone suites remain
required. No macOS/provider, audible-host, or browser round-trip success may be
claimed without the corresponding environment and registration.
