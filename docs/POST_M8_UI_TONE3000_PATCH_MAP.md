# Post-Milestone-8 UI and TONE3000 Configuration Patch map

Status: implemented and validated corrective-patch map

Baseline: completed Amphibia Milestones 0 through 8 on `main`

Evidence date: 2026-07-29

This document was created before corrective implementation and retained as the
audit-to-implementation map. The work is a
post-Milestone-8 corrective patch. It is not Milestone 9 and does not consume,
anticipate, renumber, or complete Milestone 9 scope.

## Architecture reviewed

The audit covered the Milestone 4 OAuth, credential, HTTP, provider, and
managed-promotion foundation; the Milestone 5 hosted-selection coordinator and
variant panel; the Milestone 6 setup, shared settings, standalone audio
ownership, and VST3 configuration panel; the Milestone 7 Windows package and
installer; and the Milestone 8 evidence gates and unsigned candidate.

The implementation review included:

- `NeuralAmpModeler/NeuralAmpModeler.{h,cpp}`;
- `NeuralAmpModeler/NeuralAmpModelerControls.h`;
- `NeuralAmpModeler/ConfigurationServices.h`;
- `NeuralAmpModeler/SharedSettings.{h,cpp}`;
- the Windows standalone host, dialog, setup wizard, resources, and projects;
- every source under `Providers/Tone3000/`, `Network/`, `Credentials/`, and
  the directly used managed-library path;
- `IHostedSelectionCallbackReceiver`,
  `UnavailableHostedSelectionCallbackReceiver`, and the system-browser
  abstraction;
- Windows VST3/APP projects, Inno Setup input, packaging scripts, and the
  Milestone 4 through 8 tests, maps, and reports.

## Reproduction baseline

The checked-in editor is logically fixed at `600 x 400` (`PLUG_WIDTH` and
`PLUG_HEIGHT`). On the audit machine, Windows reported a 1536 x 864 desktop at
100% display scaling. The standalone outer window created for the 600 x 400
editor measured 614 x 458 including native frame and menu.

The existing unsigned Release standalone was launched in safe mode. Native
window creation was observed, but this automation session could capture only
the native frame before the transient iPlug child surface disappeared; that
capture is diagnostic evidence, not a claim of successful visual validation.
The source geometry and pinned iPlug2 paint path reproduce the reported defect
deterministically:

- `Search TONE3000` and `Find IR on TONE3000` have non-empty internal labels;
- their `IVButtonControl` style calls `WithShowLabel(false)`;
- pinned iPlug2 `IVectorBase::DrawLabel()` paints a label only when
  `mStyle.showLabel` is true;
- the same faulty style is used by Configuration, provider, and managed-library
  buttons, producing the reported blank blue rectangles;
- all Settings calibration, provider, library, model-info, and About controls
  are created together without a page/section visibility boundary;
- their hard-coded rectangles occupy intersecting vertical bands.

The initial control tree is attached once by the editor layout callback. Opening
and closing Settings calls visibility/animation methods rather than adding more
top-level controls, so source review does not show repeated-open duplication.
This must be converted into an executable control-count regression check rather
than left as an assumption.

At baseline there is no active Settings section because there is no section
model: the entire legacy Settings canvas is one page. The separate
Configuration overlay likewise renders local folders, managed library, and
TONE3000 together.

## Root cause

The broken UI is caused by a combination of structural and paint-policy defects:

1. **Captions are explicitly suppressed.** Amphibia passes meaningful button
   labels to `IVButtonControl`, then uses `WithShowLabel(false)`. In the pinned
   iPlug2 implementation the label remains stored but is not painted. Disabled
   controls therefore become especially misleading blank rectangles.
2. **There is no Settings page ownership model.** Calibration, model
   diagnostics, About, TONE3000, and managed-library controls are simultaneous
   siblings in one 600 x 400 container. A second full-screen Configuration
   container duplicates local/library/provider presentation rather than
   selecting a bounded section.
3. **Hard-coded bands overlap.** Settings derives calibration from the title,
   places provider and library rectangles around the center, and places model
   and About rectangles in the bottom 108 pixels. These independent
   calculations overlap. Configuration uses fixed Y coordinates through 352
   with long, unbounded status labels and no page separation.
4. **Visibility is container-wide, not section-specific.** Only the whole
   overlay is hidden. Inactive categories remain visible, enabled, hit-testable,
   and ordered in the same container.
5. **Layout is calculated only during attachment.** Main-editor bounds and
   Settings rectangles are derived during initial control creation. There is no
   shared layout object recalculated for section switches or effective UI
   scale, while the Windows host independently changes graphics screen scale on
   `WM_DPICHANGED`.
6. **Standalone and VST3 ownership text is conditional but geometry is shared.**
   The content differs correctly (`APP_API` owns standalone audio; a DAW owns
   VST3 audio), but both variants inherit the same crowded rectangle model.
7. **Provider UI is not live-wired.** The main IR action is a no-op, the NAM
   action only opens an unpopulated selection panel, provider Settings actions
   are empty/disabled, and the selection panel's external-source action is a
   no-op.

This is a release-blocking defect, not normal appearance.

Post-implementation validation confirmed two additional mechanics behind the
reported symptoms:

- on Windows at fractional DPI, the standalone native parent and iPlug child
  remained at a 600 x 400 physical client while graphics were painted at the
  scaled size, clipping the right and bottom edges; the host now sizes both
  native windows from the effective DPI while preserving logical 600 x 400
  coordinates;
- hiding only a section container was insufficient because the completion of
  the overlay animation recursively exposed descendants again; section
  switching now applies visibility and enablement to every page descendant and
  reapplies the selected section after animation completes.

## Current control ownership

| Surface | Owner | Current children/behavior |
| --- | --- | --- |
| Main editor | `Amphibia::mLayoutFunc` | background, title, model/IR browsers, TONE3000 buttons, knobs, toggles, meters, Settings button, and full-screen overlays |
| Legacy Settings overlay | `NAMSettingsPageControl` | calibration, output mode, model information, provider status/actions, managed-library status/actions, About, Configuration launcher, close |
| Configuration overlay | `NAMConfigurationPanelControl` | standalone/DAW audio ownership label, local folders, managed library, provider status/actions, close |
| Hosted variant overlay | `Tone3000SelectionPanelControl` | architecture choice, waiting state, exact variants, explicit download/audition/removal controls |
| Standalone setup | `SetupWizardWin` and `SetupController` | Windows first-run/audio/local/library/provider setup outside the iPlug editor |
| Provider flow | `Tone3000HostedSelectionCoordinator` | cross-instance lease, callback-first ordering, provider authorization, exact tone/model enumeration, explicit download, managed activation handoff |
| Tokens | `PlatformSecureCredentialStore` | per-user access/refresh token credential under the frozen TONE3000 namespace; no plaintext/settings/state fallback |
| Activation | product callback into `_RequestModel()` / `_RequestIR()` | preserves the existing asynchronous preparation and audio-block handoff |

## Current main-window bounds

All coordinates below are logical pixels in the fixed 600 x 400 editor:

- editor: `(0, 0, 600, 400)`;
- outer main area: 20-pixel inset;
- content: 30-pixel inset, `(30, 30, 570, 370)`;
- title: top 50 pixels of content;
- six knob cells: one grid band below the title;
- model row: 200 x 30 centered near the bottom, approximately Y 309-339;
- IR row: the model row shifted down 38 pixels, approximately Y 347-377;
- TONE3000 model button: from the model display right edge plus 7 to content
  right;
- TONE3000 IR button: from the IR display right edge plus 7 to content right;
- meters: 30-pixel strips shifted outside the content edges;
- Settings: fixed corner button;
- Settings, Configuration, TONE3000 selection, and Slim overlays: full editor
  bounds.

The IR row can extend below the content region because it is derived by shifting
the model row rather than allocated by a row layout. Main layout does not expose
a testable central description of these rectangles.

## Current Settings and Configuration bounds

Legacy Settings uses:

- title: top 50 pixels inside a 30-pixel inset;
- calibration/output band: derived below the title with
  `NAM_KNOB_HEIGHT + NAM_SWITCH_HEIGHT + 10`;
- provider: 62-pixel centered area shifted upward 34;
- library: 112-pixel centered area shifted downward 66;
- model info and About: both drawn in the bottom 108 pixels;
- close: fixed corner.

Configuration uses fixed bands:

- header: padded top 42;
- audio ownership: Y 68-91;
- local-folder status: Y 100-140;
- local-folder actions: Y 143-172;
- managed-library status: Y 184-226;
- managed-library actions: Y 230-259;
- TONE3000 status: Y 274-318;
- TONE3000 actions: Y 322-352;
- close: fixed corner.

Long folder/library/provider status is placed in fixed-height labels with no
active-page isolation. Configuration and legacy Settings duplicate several
controls and use different layouts and captions.

## Current visibility and z-order policy

Top-level overlays are attached after main controls, so they cover the editor by
attachment order when shown. Each full overlay is hidden at construction.
Showing legacy Settings unhides its children through container animation.
Showing Configuration hides/unhides the entire second container. There is no
category-level visibility, enablement, keyboard-navigation, or hit-test policy.
The selection panel and Slim overlays are later top-level controls and may sit
above Settings depending on which independent action exposes them.

The corrective layout will have one Settings/configuration overlay with an
explicit section enum. Only the active section's controls will be visible and
enabled; inactive controls will be hidden and non-hit-testable through the
container hierarchy. Overlay switching will close or subordinate incompatible
overlays explicitly instead of relying on incidental attachment order.

## Standalone versus VST3 differences

The core editor and provider/library/local-folder settings are shared.

- Standalone owns audio driver, device, channels, sample rate, buffer size,
  setup completion, and safe mode through the Windows APP host/wizard. Its
  editor Settings page links to the standalone Audio Settings path and must not
  duplicate DAW wording.
- VST3 does not open the setup wizard and must retain a bounded read-only
  statement that the DAW controls devices, routing, sample rate, and buffer
  size.
- Referenced folders, managed-library state, and TONE3000 credential/session
  capability are shared without putting provider secrets into shared settings.
- No standalone-only interactive audio control may appear in VST3.

## Current TONE3000 registration state

`ConfiguredTone3000()` currently returns `UnconfiguredTone3000()`. The shipped
validator therefore reports:

- registration state: `Unconfigured`;
- valid unconfigured descriptor: true;
- provider usable: false;
- callback strategy: `Unavailable`;
- client ID: absent;
- redirect URI: absent.

The corrective source of truth will return a development configuration with:

- registration state: `DevelopmentConfigured`;
- client type: public desktop client;
- OAuth: Authorization Code with PKCE S256;
- client secret: none;
- exact redirect URI:
  `http://127.0.0.1:43821/oauth/callback`;
- callback strategy: fixed IPv4 loopback;
- bind address: `127.0.0.1`;
- port: `43821`;
- path: `/oauth/callback`.

The publishable client ID will exist once in the centralized configuration
implementation. It will not be stored in Credential Manager or exposed as a
user setting. Production validation and branding requirements remain
unsatisfied and will not be misreported.

## Existing callback abstraction

`IHostedSelectionCallbackReceiver` already defines:

- `Start(exactRegisteredRedirect, deadline, completion)`;
- `Cancel()`;
- `Shutdown()`;
- `IsActive()`.

`Tone3000HostedSelectionCoordinator::Begin()` already acquires the hosted-flow
lease, calls `receiver.Start(...)`, and only after successful return invokes
`provider.BeginAuthorization(...)`. `Tone3000Provider::BeginAuthorization()`
creates state/PKCE and opens the browser. This ordering is the correct seam for
listener readiness before browser launch.

The shipped implementation is
`UnavailableHostedSelectionCallbackReceiver`, which always fails closed. There
is no product loopback socket, readiness confirmation, HTTP request parser,
Retry surface, or port-conflict category. `OAuthCallback` already enforces an
exact callback base, bounded URI/query values, percent-decoding validity,
duplicate-field rejection, state presence, and unambiguous code/error results.
`OAuthController` owns deadline, constant-time state validation, single use,
supersession, cancellation, and shutdown.

The corrective receiver will bind the real socket directly and retain it; it
will not probe then close. It will bind exclusively to `127.0.0.1:43821`,
accept one bounded `GET /oauth/callback?...` request, return a minimal no-store
response, and release ownership on every terminal path. Provider/controller
state validation remains authoritative after transport parsing.

## Exact files changed

Compilation and direct tests required the following directly related files.

### UI and product wiring

- `NeuralAmpModeler/NeuralAmpModeler.{h,cpp}`;
- `NeuralAmpModeler/NeuralAmpModelerControls.h`;
- `NeuralAmpModeler/UI/EditorLayout.h`;
- `NeuralAmpModeler/Standalone/Windows/AmphibiaAPPDialog.cpp` for native
  parent/child DPI sizing.

### Provider and callback

- `NeuralAmpModeler/Providers/Tone3000/Tone3000Configuration.{h,cpp}`;
- `NeuralAmpModeler/Providers/Tone3000/Tone3000HostedSelectionCoordinator.*`
  only if readiness/error propagation needs a narrow extension;
- `NeuralAmpModeler/Platform/FixedLoopbackCallbackReceiver.{h,cpp}`;
- provider/platform ownership in `NeuralAmpModeler.{h,cpp}`;
- APP and VST3 project/filter files for the new product source.

### Tests and packaging evidence

- existing Milestone 4-6 suites where their owned contracts are extended;
- Milestone 4 native layout/callback tests and the post-M8 static contract test;
- `scripts/package-windows.ps1` and Milestone 7/8 tests only if public-client
  scanning or new source/evidence inventory requires a direct update;
- generated ignored artifacts under `artifacts/release/0.1.0`, never committed.

### Documentation

- `docs/POST_M8_UI_TONE3000_PATCH_MAP.md`;
- `docs/POST_M8_UI_TONE3000_PATCH_REPORT.md`;
- the directly relevant documentation enumerated in the corrective request,
  including TONE3000, OAuth, HTTP, threading, threat model, first-run,
  troubleshooting, release checklist, privacy, security, and changelog.

## Centralized layout direction

One testable layout model will own named metrics for outer margin, header,
footer, section navigation, content, row height/gap, label width, normal/small
button width, text/status areas, close button, and scale. It will produce:

- main-editor rectangles;
- Settings shell/navigation rectangles;
- per-section rectangles for General, Local Files, Managed Library, TONE3000,
  Calibration, Diagnostics, and About.

The logical 600 x 400 size may remain fixed. Effective graphics/window scaling
will scale the complete layout uniformly rather than mixing logical rectangles
with separately scaled native coordinates. The supported scale matrix will be
exercised without claiming a scale that cannot keep essential content bounded.

Buttons will use an explicit readable style with a painted label, bounded font,
padding, and distinct enabled/disabled treatment. Disabled captions remain
present and are paired with status/help text rather than communicated only by
color.

## Explicit non-goals

This patch does not implement new DSP, parameters, NAM architectures, IR
formats, native provider search, `/tones/search`, catalog mirroring, background
sync, updates, telemetry, analytics, advertising, subscriptions, account
proxying, a private backend, server behavior, macOS completion, signing,
publication, GitHub Releases, Git tags, or unrelated dependencies/assets.

It does not change identity, version, plug-in/manufacturer codes, FUIDs, project
GUIDs, installer AppId, namespaces, state format, parameter contract, DSP order,
loading behavior, local workflows, ZIP policy, content addressing, offline
restoration, installer family, or artifact names. Provider activity remains
outside `ProcessBlock()`.

## Future Milestone 9 compatibility

- No Milestone 9 feature is implemented by this patch.
- No Milestone 9 acceptance criterion is marked complete.
- No roadmap milestone is renumbered.
- Provider-neutral interfaces remain extensible.
- The Settings section architecture remains extensible.
- The implementation will not introduce a temporary workaround that must be
  removed before Milestone 9.
- No API-incompatible change will be introduced without direct necessity.
- No speculative Milestone 9 class or placeholder will be added.
- No Milestone 9 documentation will be rewritten as completed.
- No `MILESTONE_9_REPORT.md`, completion marker, Git tag, or release artifact
  will be created.

## Corrective implementation result

`EditorLayout.h` now computes the main editor and seven Settings sections from
named metrics at 100%, 125%, 150%, and 200% logical scale. Only General, Local
Files, Managed Library, TONE3000, Calibration, Diagnostics, or About can own
visible/enabled page descendants at one time. Main and Settings buttons paint
their captions, including disabled captions.

The Windows standalone was visually inspected at the machine's actual 125%
display scale. The complete main editor and all seven Settings pages were
reviewed from real Release windows; the ignored evidence is under
`artifacts/post-m8-*.png`. VST3 uses the same tested editor layout and retains
the DAW-audio ownership message, but no REAPER installation was available for
host-dependent visual inspection.

`ConfiguredTone3000()` now provides one centralized public desktop
development registration (`t3k_pub_…`, no client secret) and the exact fixed
redirect `http://127.0.0.1:43821/oauth/callback`. The product receiver retains
an exclusive socket bound only to `127.0.0.1:43821`, confirms readiness before
browser launch, accepts one bounded exact-path GET, and releases the socket on
success, failure, cancellation, timeout, or shutdown. No alternate port,
address, path, or callback strategy is selected.

## Follow-up input-dispatch repair (2026-07-29)

The post-patch regression had two connected causes:

1. `Tone3000SelectionPanelControl::Draw()` rendered `Close` when a selection
   was Cancelled or Failed, but `OnMouseDown()` handled the same
   hit rectangle only in `WaitingForBrowser`. After cancel, the visible
   full-window panel stayed attached, topmost, and hit-testable, so it consumed
   every later main-editor or Settings click.
2. iPlug2 `IButtonControlBase` sets a clicked button value to `1` and resets it
   only when the default click animation ends. Direct Amphibia action lambdas
   replaced `DefaultClickActionFunc`, so those controls performed their action
   but remained in their dark/pressed visual state.

`UI/Tone3000SelectionPanelState.h` now supplies one pure terminal-close
predicate used by both drawing and mouse dispatch. The terminal Close action
hides the panel and redraws the editor without invoking provider cancellation
again. `MakeMomentaryButtonAction()` composes the affected direct IButton/ISVG
actions with iPlug2's default animation, preserving the immediate application
action while restoring the normal value-reset lifecycle. It covers the main
TONE3000 actions, Settings/navigation/provider/library/local-folder actions,
and application SVG URL/close/gear buttons.

The direct regression files are:

- `NeuralAmpModeler/UI/Tone3000SelectionPanelState.h`;
- `NeuralAmpModeler/NeuralAmpModelerControls.h`;
- `NeuralAmpModeler/NeuralAmpModeler.cpp`;
- `tests/milestone5/SelectionTests.cpp` (10 passing groups, including
  Cancelled/Failed/FetchingTone/Ready state coverage);
- `tests/characterization/test_post_m8_contract.py`.

The post-M8 static contract, Milestone 5 Debug suite, Debug standalone, and
Release VST3 build passed. The standalone host and 600 x 400 iPlug child were
identified and sent direct Windows input messages, but the automation compositor
could not capture the GPU child reliably; this remains source/native coverage
rather than a new visual-manual claim. No installer, ZIP, SBOM, scan, or hash
artifact was regenerated for this source-only follow-up.
