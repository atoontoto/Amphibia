# Milestone 6 standalone setup-wizard map

Status: implemented; closure evidence is in `MILESTONE_6_REPORT.md`

Evidence date: 2026-07-26

Baseline: completed Amphibia Milestones 0-5 on `main`, product version `0.1.0`,
derived from NeuralAmpModelerPlugin `v0.7.15` and iPlug2 `66f9060`.

This map was created before any Milestone 6 product-code edit. Milestone 6 is
configuration orchestration only. It does not change product identity, DSP,
parameter/state compatibility, managed-object identity, provider wire behavior,
or installer/release scope.

## Existing standalone startup sequence

The app project currently compiles iPlug2's generic APP sources directly:
`IPlugAPP.cpp`, `IPlugAPP_host.cpp`, `IPlugAPP_dialog.cpp`, and
`IPlugAPP_main.cpp`.

On Windows, `WinMain()` in `iPlug2/IPlug/APP/IPlugAPP_main.cpp`:

1. acquires the `BUNDLE_NAME` singleton mutex;
2. creates `IPlugAPPHost`;
3. calls `Init()`;
4. calls `TryToChangeAudio()`;
5. creates the main dialog/editor; and
6. enters the Win32 message loop.

`IPlugAPPHost::Init()` creates the plug-in instance in the host constructor,
sets the host identity, calls `InitState()`, constructs the selected RtAudio
backend, enumerates audio and MIDI devices, opens selected MIDI endpoints,
resets parameters, and activates the processor. `TryToChangeAudio()` then
looks up device names and calls `InitAudio()`.

The generic Windows `WinMain()` receives `lpszCmdParam` but does not parse it.
The macOS `main(argc, argv)` uses arguments only for inherited optional AUv3
registration and otherwise gives setup no command-line seam.

## Existing standalone audio-device configuration

`iPlug2/IPlug/APP/IPlugAPP_host.h` defines `IPlugAPPHost::AppState` with:

- driver index;
- input and output device names;
- input and output first/second channel numbers;
- sample rate and buffer size; and
- MIDI device/channel preferences.

The Windows backends exposed by the pinned host are DirectSound and ASIO. ASIO
uses the selected output device as its duplex device. macOS exposes CoreAudio.
No driver is bundled or downloaded. Buffer choices are the inherited fixed set
from 32 through 8192 samples. Sample rates are the intersection reported by the
selected input and output devices.

The existing Preferences dialog in `IPlugAPP_dialog.cpp` edits `mState`
directly. Apply calls `TryToChangeAudio()`. Cancel copies `mTempState` back and
attempts to reopen it. `InitAudio()` closes the current stream before attempting
the new stream; failure returns `false` after the old stream has already been
closed. `TryToChangeAudio()` silently substitutes a default device when a
stored name is missing and writes that substitution immediately. These
behaviors are insufficient for Milestone 6 rollback and missing-device policy.

`InitAudio()` configures the plug-in sample rate and calls `OnReset()` before
opening the RtAudio stream. Amphibia's Milestone 2 reset path increments the
processing generation, marks incompatible model/IR stages bypassed, and queues
asynchronous preparation. A failed open must therefore restore both a working
stream and the prior plug-in processing configuration.

The audio callback uses fixed numeric buffers and calls `IPlugAPP::AppProcess()`.
Device enumeration, dialog work, settings writes, and stream opening are outside
that callback. The inherited callback already fades stream start/stop over a
buffer but has no setup test generator, input activity snapshot, underrun
counter, or callback-active diagnostic.

## Existing preference and settings storage

The generic host uses `<AppSupport>/Amphibia/settings.ini`. On Windows this is
normally `%LOCALAPPDATA%\Amphibia\settings.ini`; on macOS it is beneath the
user Application Support directory. It is the frozen Amphibia settings
namespace.

`InitState()` reads audio and MIDI values through profile APIs and then calls
`UpdateINI()`, which rewrites individual INI keys in place. There is no settings
schema, atomic whole-file replacement, backup, corrupt-value report, or
multiple-process merge policy. The file contains only generic standalone
audio/MIDI state. It contains no plug-in state, credentials, or provider data.

Milestone 6 will preserve readable legacy INI audio values as migration input,
but new Amphibia-owned configuration will use a separate versioned JSON
document with temp-file, flush, atomic replace, and backup behavior. This avoids
changing the frozen namespace while allowing unsupported-future-schema rejection
and recovery.

## Existing model-folder and IR-folder preferences

There is no durable application-level NAM-folder or IR-folder preference.
`NAMFileBrowserControl` is an `IDirBrowseControlBase` with non-recursive scan.
Choosing or activating a file rebuilds its menu from that file's containing
directory; `OnUIOpen()` reconstructs the directory from the active path.
Referenced active paths can enter DAW project state, but a preferred browsing
folder is not stored independently.

Milestone 6 will add shared local `nam_folder` and `ir_folder` preferences. A
folder preference seeds the existing browser only when it does not override an
active/project-restored file. It does not import, recurse, hash, copy, rename,
delete, or enter provider requests. Same-directory preview counts use the same
extension scope as the inherited browser and run outside the UI/audio threads.

## Existing managed-library path and status

Each Amphibia instance constructs `ManagedLibrary(DefaultLibraryPath())`, calls
`Initialize()`, and owns a joined `ImportWorker`. The default root is
`<AppSupport>/Amphibia/Library`; relocation is deliberately unsupported.

The existing Settings page already exposes:

- root path, schema, object/NAM/IR/pack counts, provider-association count,
  approximate bytes, and recent names;
- Open folder, Verify, Remove unused, Clear staging, Import files, Import
  folder, and Import ZIP actions; and
- copied import progress through message-thread polling.

The library service already supplies containment, schema, atomic-index,
verification, staging, and recovery contracts. Milestone 6 adds a bounded
readiness/status projection (write access, staging status, verification state,
fatal versus warning condition, and readable disk-space status). It does not
add relocation or automatic deletion. A failed library leaves referenced-file
mode usable.

## Existing TONE3000 registration and connection state

`ConfiguredTone3000()` is intentionally unconfigured. The Settings page shows
registration, connection/API, and secure-store diagnostics. Hosted Search/Find
entry points and the exact-selection panel exist but stay inert when
unconfigured. No approved account-only authorization or production callback
receiver exists.

The wizard page will read copied registration/connection diagnostics only. It
will not open a browser on page entry. In the repository build it displays the
required unconfigured explanation and allows setup to continue. If a future
registered build supplies an approved explicit connection action, it must use
the existing OAuth, secure credential, hosted-selection, and cancellation
boundaries. Setup state never contains tokens, callback material, client IDs,
or signed URLs.

`docs/TONE3000_BRANDING_COMPLIANCE.md` was referenced by the Milestone 5 map
but is absent from the baseline. Milestone 6 will create the record without
inventing or packaging provider assets.

## Existing Settings, About, and diagnostics surfaces

The in-editor gear opens `NAMSettingsPageControl`. It contains calibration and
output-mode controls, model information, About text, provider diagnostics, and
managed-library actions. It is shared by standalone and plug-in builds. It has
no standalone audio controls and no shared folder-preference controls.

The standalone menu separately opens the generic modal Preferences dialog for
audio/MIDI. Help/About route through the plug-in callbacks. Diagnostics are
currently formatted ad hoc and do not expose a redacted copy action.

Milestone 6 will:

- add `Run Setup Wizard Again` and `Audio Settings` to the standalone menu;
- keep the first-run wizard standalone-only;
- extend the shared Settings panel with NAM/IR folder configuration, setup
  status/diagnostics, and the explicit statement that DAW hosts own audio
  device and buffer configuration; and
- never launch the wizard on plug-in construction, host scan, state restore,
  or editor reopen.

## First-launch detection and command-line options

The setup store schema is independent of DAW project state:

```text
schema_version: 1
completion: never_started | in_progress | completed | skipped
audio_configured: bool
nam_folder_configured: bool
ir_folder_configured: bool
library_validated: bool
tone3000_configured: bool
last_completed_app_version?: string
completed_at_unix_seconds?: int64
```

Missing setup state is `NeverStarted` for a genuinely new settings directory.
For an existing legacy `settings.ini`, migration preserves audio values and
shows a non-blocking `Review Amphibia setup` invitation unless critical audio
output is unavailable; it does not erase existing state or force an upgrade
wizard merely because setup state is absent.

Exact standalone options are `--first-run`, `--setup`, `--audio-settings`,
`--safe-mode`, `--help`, and `--version`. Unknown options produce bounded help
and a nonzero command-line result rather than becoming paths or shell input.
`--help` and `--version` are terminal and do not initialize audio. `--setup`
and `--first-run` force the wizard; `--audio-settings` starts on its audio page.
Safe mode suppresses automatic previous model/IR restoration only; it does not
alter DAW state or delete any setting.

The first-run order is:

```text
parse bounded arguments
  -> load/migrate ordinary standalone settings
  -> load setup state
  -> decide initial wizard/page/safe-mode policy
  -> construct plug-in and safely attempt the last audio configuration
  -> create the main editor
  -> show the native wizard only when policy requires it
```

Missing optional folders, TONE3000 configuration, offline status, provider
token expiry, missing previous content, and app-version changes do not
automatically force setup.

## Setup schema and migration policy

The Amphibia settings document schema and setup-state sub-schema are both
versioned at 1. Unsupported future versions are rejected without replacement.
A corrupt primary can fall back to a validated `.bak`; otherwise safe defaults
are used and the corrupt file is preserved for diagnostics. Legacy INI values
are imported field by field with bounds. Existing folder/library/provider/
project data is never reset because setup state is absent or invalid.

Writes serialize a merged snapshot to a random sibling temporary file, flush,
replace the primary, and retain the previous validated primary as a backup.
Standalone audio commits update standalone fields only. VST3 shared-folder
writes update shared fields only and cannot overwrite standalone audio/setup
fields. Cross-process writes use an Amphibia-specific bounded file lock and
reload-before-merge. Shared non-audio values use last-successful-write wins.

Cancel from Welcome records `Skipped` only after an explicit confirmation;
otherwise window close/Cancel leaves `InProgress`. Successfully previewed
audio remains active for the current session only when clearly accepted, while
durable settings remain the last committed valid configuration. Finish alone
sets `Completed`, the app version, and completion timestamp.

## Standalone versus plug-in ownership

| Setting | Standalone | VST3 | Shared |
| --- | --- | --- | --- |
| audio backend/device/channels | owns | DAW owns; never configurable | no |
| sample-rate/buffer preference | owns | DAW owns | no |
| setup completion/window state | owns | never reads for launch policy | no |
| local NAM browse folder | configures | configures | yes |
| local IR browse folder | configures | configures | yes |
| managed root/index/provenance | displays/actions | displays/actions | yes |
| TONE3000 credential namespace/status | optional actions | optional actions | yes, secure store remains authoritative |
| active model/IR and DSP parameters | session owns through plug-in state | project/instance owns | no |

The standalone setup store is not serialized through `SerializeState()` and
does not change state layout 1.1. Plugin instances never write standalone audio
or completion fields.

## Wizard page and draft model

The native wizard has Welcome, Audio device, Input/output test, Local NAM
folder, Local IR folder, Managed library, TONE3000, Summary, and Finish pages.
Navigation uses Back, Next, Skip, Cancel, and Finish according to page policy.
Page controls bind to a copied draft. Optional pages may be skipped. Summary
reads only that draft plus copied service status. Finish validates and commits
the allowed settings in order, then commits setup completion last.

The wizard is a standalone top-level native dialog owned by the main window,
not a webview and not a DSP/control overlay. Windows uses a resizable/scaled
Win32 dialog with scrollable long-page content and normal dialog keyboard
navigation. macOS will use the corresponding SWELL/native dialog seam; this
environment can source-review but cannot build or runtime-validate it.

## Device-change transaction and lifecycle

An Amphibia-owned audio service exposes copied backend/device/capability
descriptors with stable backend/device IDs plus display names. Duplicate names
are not identities. Enumeration and Refresh occur on the message/setup worker,
never the audio callback.

Audio apply is transactional:

1. validate backend, exact device identity, channels, rate, and buffer;
2. retain the committed working configuration;
3. stop the test signal;
4. attempt the requested stream and require start/callback/output readiness;
5. on success commit the active configuration and let existing `OnReset()`
   generation logic asynchronously reprepare model/IR;
6. on failure reopen the retained working configuration; and
7. if restoration also fails, enter a responsive no-audio state without
   overwriting the retained preference.

The stream service records bounded last-error text and callback-active state.
It never loops indefinitely. Active-device disappearance stops audio and shows
a single recovery state with Retry, Choose Another Device, or Continue Without
Audio. It never silently starts monitoring an unrelated input. The pinned
RtAudio integration has no reliable cross-platform hotplug callback, so
Milestone 6 provides explicit Refresh and documents that limitation.

## Input/output test

Input peak/activity and output peak are published as lock-free numeric
snapshots from the existing callback. Samples are not stored, logged, recorded,
or uploaded.

The output test is an audio-callback-only bounded oscillator controlled by
atomics prepared outside the callback. It starts only after the explicit test
button and volume warning, uses a conservative peak bound, fades in/out, has a
maximum duration, cannot stack, and stops on page exit, Cancel, device change,
or shutdown. Test control/state is standalone-host state and never plug-in or
DAW project state.

## UI-thread and audio-thread constraints

UI/message thread may update controls, take copied service snapshots, submit
enumeration/scan/library/provider work, and commit settings. A joined setup
worker performs same-directory folder counts and settings I/O where needed.
Library verification remains on `ImportWorker`; provider work remains on its
existing workers.

None of these is reachable from `ProcessBlock()`:

- wizard rendering or navigation;
- command-line parsing;
- device enumeration/open/close;
- folder validation/counting;
- library verification;
- provider connection;
- settings parsing/serialization;
- diagnostic formatting; or
- test-tone control.

Only bounded signal generation, meter atomics, and fixed stream diagnostics
are added to the standalone audio callback. No allocation, lock, settings, UI,
filesystem, provider, or formatting operation is added there.

## Failure and rollback paths

- Unsupported/corrupt future settings: reject, preserve, try backup, then safe
  defaults with a bounded diagnostic.
- Invalid/missing device: preserve stored preference, continue without audio,
  and offer explicit recovery.
- Unsupported rate/buffer/channel: reject before commit and retain/reopen the
  working stream.
- Folder missing or removed: mark the draft unavailable; do not clear the last
  committed preference unless the user explicitly replaces/skips it.
- Library unavailable/read-only/future/corrupt: warn; disable imports/downloads;
  leave referenced mode and setup completion available.
- OAuth/provider failure: report and allow Skip/Finish; secure systems own any
  credential mutation.
- Cancel/window close/shutdown: stop test tone, cancel wizard-owned scans,
  invalidate callbacks, join owned worker, preserve last valid audio, and do
  not mark Completed.

## Platform differences

- Windows: DirectSound and ASIO from the pinned RtAudio build; prefer ASIO in
  recommendations when an actual ASIO device is present, but never imply a
  quality guarantee. Use UTF-16 Win32 paths/arguments at the platform edge and
  bounded UTF-8 settings values internally.
- macOS: CoreAudio remains the available backend. The setup core and schema are
  platform-neutral, but native dialog, callback/hotplug behavior, Keychain,
  provider HTTP, and full product integration cannot be validated here.
- No Linux target is enabled by the inherited standalone host.

## Exact files planned for modification

Create before replacing generic host integration:

- `NeuralAmpModeler/Standalone/SetupTypes.h`
- `NeuralAmpModeler/Standalone/CommandLine.*`
- `NeuralAmpModeler/Standalone/SettingsStore.*`
- `NeuralAmpModeler/Standalone/AudioConfiguration.*`
- `NeuralAmpModeler/Standalone/SetupController.*`
- `NeuralAmpModeler/Standalone/SetupDiagnostics.*`
- `NeuralAmpModeler/Standalone/Windows/AmphibiaAPPHost.*`
- `NeuralAmpModeler/Standalone/Windows/SetupWizardWin.*`
- `NeuralAmpModeler/SharedSettings.*`
- `tests/milestone6/*`

Integrate narrowly in:

- `NeuralAmpModeler/projects/NeuralAmpModeler-app.vcxproj` and filters: compile
  the Amphibia-owned standalone host/wizard instead of the generic iPlug host,
  dialog, and main sources while retaining `IPlugAPP` processing APIs;
- `NeuralAmpModeler/projects/NeuralAmpModeler-vst3.vcxproj` and filters: compile
  only the shared settings/configuration-panel sources, never standalone audio;
- `NeuralAmpModeler/resources/resource.h`, `main.rc`, `main.rc_mac_menu`, and
  `main.rc_mac_dlg`: setup/audio menu commands and native resources;
- `NeuralAmpModeler/NeuralAmpModeler.h/.cpp`: shared folder preferences,
  setup-panel actions, safe-mode restore hook, and copied diagnostics only;
- `NeuralAmpModeler/NeuralAmpModelerControls.h`: explicit configuration panel
  content and DAW-owned-audio wording without changing local navigation;
- `NeuralAmpModeler/Library/*` only if a read-only readiness snapshot cannot be
  assembled from existing public APIs; no relocation/schema change is planned;
- required Milestone 6 documentation, privacy/security/threat/state/threading/
  architecture/upstream/milestone records, README, and changelog.

The iPlug2, NAM Core, AudioDSPTools, ZIP/hash, provider transport, credentials,
and DSP submodules/implementations are not edited. Installer files are not
modified.

## Acceptance evidence plan

Pure C++ tests use fake devices, clock, filesystem roots, and settings files to
cover setup states/migration, exact command options, drafts/navigation,
transaction commit/rollback, disappearance/no-device paths, channels/rates/
buffers, conservative test-tone envelopes, folder counts/privacy, library and
provider projections, secret canaries, and project-state separation. Static
contracts verify no setup/device/settings/provider operations enter
`ProcessBlock()` and no standalone wizard enters the VST3 project.

Windows Debug/Release standalone and VST3 builds plus Milestone 1-5 tests are
required. Interactive hardware, output loudness, input meter, hot-unplug, and
DAW-host validation are reported separately and only when actually performed.
macOS is source-only in this environment. Installer, signing, notarization,
release publication, system-wide VST3 installation, and automatic rescanning
remain Milestone 7 or later.
