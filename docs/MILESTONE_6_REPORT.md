# Milestone 6 report: first-run setup and configuration

Evidence date: 2026-07-26

Baseline: completed Amphibia Milestones 0-5 on `main`, product version `0.1.0`,
derived from NeuralAmpModelerPlugin `v0.7.15` and iPlug2 `66f9060`.

## Outcome

Milestone 6 adds a product-owned Windows standalone startup path and a native
nine-page first-run wizard for welcome, audio, NAM folder, IR folder, managed
library, optional TONE3000 status, review, and finish. The same local-content
and provider configuration is exposed in a VST3 Configuration panel without
giving the plug-in control over DAW audio devices.

The implementation preserves the inherited DSP and state contracts. Setup,
device management, folder checks, library work, provider status, persistence,
and diagnostics remain outside `ProcessBlock()`. No installer, system-wide
installation, signing, AAX work, or Milestone 7 packaging was performed.

Milestone 6 engineering acceptance is satisfied by code review, Windows builds,
and automated tests, subject to the manual, hardware, host, and macOS validation
gaps stated below.

## First-run behavior

- **Detection:** the standalone opens setup when schema-versioned state is
  absent or incomplete, an upgrade/repair condition is detected, or an explicit
  setup command-line override is present. A completed normal setup does not
  reappear on ordinary launch.
- **Setup-state schema:** `%LOCALAPPDATA%\\Amphibia\\settings-v1.json` is the
  canonical version-1 setup/shared-settings projection. The inherited
  `settings.ini` remains the iPlug standalone audio-compatibility store and can
  seed missing JSON audio fields during migration.
- **Command-line overrides:** the exact supported options are `--first-run`,
  `--setup`, `--audio-settings`, `--safe-mode`, `--help`, and `--version`.
  Unknown options produce a terminal error instead of being ignored.
- **Cancel:** the setup controller keeps edits provisional. Cancelling does not
  mark setup complete or replace the last committed configuration.
- **Completion:** Finish commits validated state and records completion only
  after the persistence transaction succeeds. The final page offers Start Using
  Amphibia, Open Audio Settings, Open NAM Folder, and Open Documentation.
- **Rerun:** Setup can be opened from the standalone menu or with `--setup`;
  `--audio-settings` enters configuration focused on audio.
- **Safe mode:** `--safe-mode` suppresses risky automatic startup behavior,
  avoids opening the normal audio stream, and leaves repair/configuration paths
  available.

## Audio configuration

- **Backends:** Windows exposes the pinned host's DirectSound and ASIO backends.
  Amphibia does not bundle, install, or download an ASIO driver.
- **Device enumeration:** input and output choices are enumerated on the UI
  thread. Persistent identifiers combine backend and enumeration identity;
  duplicate display names remain distinguishable.
- **Channel selection:** input and output channel selectors follow the selected
  devices. Output-only operation is supported by supplying silent input.
- **Sample rates:** choices are derived from the selected device pair's reported
  capabilities.
- **Buffer sizes:** the inherited supported choices from 32 through 8192 samples
  are presented.
- **Transaction and rollback:** Apply validates a candidate, attempts to open it,
  and commits only after success. Failure attempts to reopen the last known-good
  configuration; if that also fails, Amphibia enters an explicit no-audio state.
- **Test output:** output testing is a user-triggered, bounded oscillator. It is
  never started merely by opening a page.
- **Input meter:** the meter displays current levels only. It does not record,
  persist, or upload audio.
- **Missing-device recovery:** a missing saved device is reported and setup
  remains available. Amphibia does not silently commit a substituted device.

## Local content setup

- **NAM folder:** the wizard and VST3 panel can choose and validate a referenced
  local NAM folder; Open Folder is available after configuration.
- **IR folder:** the same referenced-folder behavior is available for impulse
  responses.
- **Managed library:** the page reports schema, item counts, size, task state,
  last verification, and verification result. It exposes Scan, Import NAM,
  Import IR, Verify, Repair, and Open Library Folder actions.
- **Folder privacy:** folder paths remain local. Validation enumerates only what
  is needed for compatibility/health reporting; it does not upload content.
- **Import separation:** choosing a referenced folder does not copy, rename,
  rewrite, or import source files. Managed imports remain explicit actions.
  Managed-library failure does not disable referenced local mode.

## TONE3000 setup

- **Registration state:** no production Amphibia OAuth/client registration or
  callback is configured, so setup reports TONE3000 as unconfigured.
- **Connection behavior:** the page can report connection capability/status but
  does not invent a working sign-in flow. Provider work stays outside the audio
  callback.
- **Unconfigured behavior:** the limitation and branding/compliance boundary are
  explained directly in the UI and documentation.
- **Optional completion:** TONE3000 is optional; setup can always finish without
  connecting it.
- **Secure-store behavior:** provider access tokens and signed URLs are excluded
  from setup/settings JSON and plug-in state. Existing provider authentication
  material remains confined to the platform secure-store boundary.

## Standalone versus VST3

| Setting | Standalone | VST3 | Shared |
| --- | --- | --- | --- |
| Audio backend/device/channels/rate/buffer | Owns and applies | DAW controls; read-only explanation | No |
| Setup completion/resume/safe mode | Owns | Does not launch wizard | No |
| Referenced NAM folder | Configurable | Configurable | Yes |
| Referenced IR folder | Configurable | Configurable | Yes |
| Managed-library location/status/actions | Configurable | Configurable | Yes |
| TONE3000 status/actions | Optional | Optional | Yes |
| Provider tokens/signed URLs | Never stores | Never stores | No |

Shared-setting writes merge only the `shared` object. They do not overwrite
standalone audio or setup-completion state.

## Settings schema

- **Version:** JSON schema version 1, with explicit setup, standalone-audio, and
  shared sections.
- **Migration:** missing legacy JSON can be seeded from active inherited INI
  audio settings. Versioned migrations are explicit; an unknown future version
  is refused rather than rewritten.
- **Atomic writes:** settings are written to a sibling temporary file, flushed,
  and atomically replaced while a cross-process lock serializes writers.
- **Recovery:** the previous valid file is retained as a backup. A malformed or
  interrupted primary write is recovered from backup when possible; otherwise
  defaults/repair are surfaced without silently claiming completion.
- **Sensitive-data exclusions:** tokens, authorization headers, passwords,
  cookies, signed URLs, and equivalent provider secrets are rejected by
  defensive persistence checks and never belong in this schema.

## Real-time safety

| Operation | Reachable from `ProcessBlock()`? |
| --- | --- |
| Wizard | No |
| Device enumeration | No |
| Device opening | No |
| Folder validation | No |
| Library verification | No |
| Provider connection | No |
| Settings writes | No |
| Diagnostics formatting | No |

The only setup-related audio-callback activity is bounded sample generation for
an output test that the user explicitly starts, plus lock-free level observation
for the input meter. The callback performs no setup I/O or allocation-heavy
orchestration.

## Files changed

### Wizard

- `NeuralAmpModeler/Standalone/Windows/AmphibiaAPPMain.cpp`
- `NeuralAmpModeler/Standalone/Windows/AmphibiaAPPDialog.cpp`
- `NeuralAmpModeler/Standalone/Windows/SetupWizardWin.{h,cpp}`
- `NeuralAmpModeler/resources/{resource.h,main.rc}`

### Audio setup

- `NeuralAmpModeler/Standalone/AudioConfiguration.{h,cpp}`
- `NeuralAmpModeler/Standalone/Windows/AmphibiaAPPHost.{h,cpp}`
- `NeuralAmpModeler/Standalone/Windows/AmphibiaAPP.cpp`

### Settings

- `NeuralAmpModeler/Standalone/SetupTypes.{h,cpp}`
- `NeuralAmpModeler/Standalone/CommandLine.{h,cpp}`
- `NeuralAmpModeler/Standalone/SettingsStore.{h,cpp}`
- `NeuralAmpModeler/Standalone/SetupController.{h,cpp}`
- `NeuralAmpModeler/SharedSettings.{h,cpp}`

### Local folders

- `NeuralAmpModeler/Standalone/FolderValidation.{h,cpp}`

### Library

- `NeuralAmpModeler/ConfigurationServices.h`
- `NeuralAmpModeler/Library/ImportWorker.cpp`

### Provider setup

- Existing provider services are surfaced through
  `NeuralAmpModeler/ConfigurationServices.h`; no production registration or
  protocol change was made.

### VST3 panel

- `NeuralAmpModeler/NeuralAmpModeler.{h,cpp}`
- `NeuralAmpModeler/NeuralAmpModelerControls.h`

### Tests

- `tests/milestone6/SetupTests.cpp`
- `tests/milestone6/Milestone6Tests.vcxproj`
- `tests/characterization/test_milestone6_contract.py`

### Documentation

- `README.md`, `PRIVACY.md`, `SECURITY.md`, `CHANGELOG.md`
- `docs/FIRST_RUN_SETUP.md`, `docs/AUDIO_CONFIGURATION.md`
- `docs/SETTINGS_OWNERSHIP.md`, `docs/SETTINGS_SCHEMA.md`
- `docs/TROUBLESHOOTING.md`, `docs/TONE3000_BRANDING_COMPLIANCE.md`
- architecture, local-library, milestone, state, threading, threat-model,
  provider, upstream-map, setup-map, and this report

### Build system

- `NeuralAmpModeler/projects/NeuralAmpModeler-app.vcxproj{,.filters}`
- `NeuralAmpModeler/projects/NeuralAmpModeler-vst3.vcxproj{,.filters}`
- `.gitignore`

The Windows APP project now compiles Amphibia-owned APP integration sources.
The VST3 project includes shared configuration code but excludes standalone
wizard/device code.

## Commands run

The following validation commands were run from `D:\\amphibia`. MSBuild used
Visual Studio 2026 Build Tools' `v145` toolset and `PostBuildEventUseInBuild=false`
for product targets so validation did not install anything system-wide.

```powershell
python tests\identity\validate_identity.py
python tests\characterization\test_inherited_contract.py
python tests\characterization\test_state_contract.py
python tests\characterization\test_milestone6_contract.py

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone2\Milestone2Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:1 /nologo
& .\tests\milestone2\build\x64\Debug\Milestone2Tests.exe

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone3\Milestone3Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:1 /nologo
& .\tests\milestone3\build\x64\Debug\Milestone3Tests.exe

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone4\Milestone4Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:1 /nologo
& .\tests\milestone4\build\x64\Debug\Milestone4Tests.exe

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone5\Milestone5Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:1 /nologo
& .\tests\milestone5\build\x64\Debug\Milestone5Tests.exe

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone6\Milestone6Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:1 /nologo
& .\tests\milestone6\build\x64\Debug\Milestone6Tests.exe

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-app /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-app /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-vst3 /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-vst3 /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:PostBuildEventUseInBuild=false /m:1 /nologo

git status
git diff
git diff --cached
git diff --check
git branch --show-current
git remote -v
git log --oneline -n 30
git submodule status --recursive
Get-FileHash -Algorithm SHA256 <artifact>
```

The first sandboxed MSBuild attempt was blocked by MSBuild FileTracker access
(`E_ACCESSDENIED`). The same builds passed when rerun with the required build
permission; this was an execution-environment restriction, not a product test
failure.

## Builds

| Configuration | Output | Warnings | SHA-256 |
| --- | --- | ---: | --- |
| Windows APP Debug x64 | `NeuralAmpModeler/build-win/app/x64/Debug/Amphibia.exe` (13,482,496 bytes) | 23 inherited conversion warnings on full compile; 0 on final incremental build | `69F02F1EEB269759574C5DD6D58D29E7B9E424B067392092848008605A808353` |
| Windows APP Release x64 | `NeuralAmpModeler/build-win/app/x64/Release/Amphibia.exe` (3,742,208 bytes) | 23 inherited conversion warnings on full compile; 0 on final incremental build | `F0D2D19ECCB4E6B8AC22D728765DD6CB72368B4992DAE470B69533FED2E75F28` |
| Windows VST3 Debug x64 | `NeuralAmpModeler/build-win/vst3/x64/Debug/Amphibia.vst3` (13,640,192 bytes) | 12 inherited conversion warnings | `2F3A4F9568DADC930F1B6880C59A9648EB85A055AD65BB8AE8286497C0159BBA` |
| Windows VST3 Release x64 | `NeuralAmpModeler/build-win/vst3/x64/Release/Amphibia.vst3` (3,639,296 bytes) | 12 inherited conversion warnings | `0E29A56947E5A0A17EA35259F3954F7A9F7583067577BFA030FCD1251239CF06` |

No installer or system-wide copy step ran, and AAX remained disabled.

## Tests

### Passed

- Identity validation: 35 identity checks.
- Inherited-contract characterization.
- State-contract characterization.
- Milestone 6 static setup/configuration contract.
- Milestone 2 native tests.
- Milestone 3 native tests, including 32 MiB hash throughput and library/ZIP
  performance characterization.
- Milestone 4 native tests: 20 passed.
- Milestone 5 native tests: 9 passed.
- Milestone 6 native tests: 7 passed, covering CLI parsing, migration and future
  schema refusal, recovery, sensitive-field rejection, transactional audio
  rollback/no-audio behavior, folder validation, setup completion, and shared
  merge preservation.
- Audio-configuration, settings-migration, and VST3-panel static assertions are
  included in the Milestone 6 native/static suites.

### Failed

- No failures remained in the final isolated test matrix.
- An exploratory concurrent closure invocation produced a Milestone 3 managed
  import failure and a Milestone 4 download-promotion failure. Milestone 3 also
  failed once immediately afterward. Both projects were rebuilt and rerun in
  isolation, where Milestone 3 passed and Milestone 4 passed all 20 groups. The
  transient failures were not reproducible in the acceptance runs.
- The initial sandboxed builds were permission-blocked before compilation and
  passed on the required rerun; they are not counted as product failures.

### Skipped

- Interactive GUI walkthrough.
- Physical audio-device testing.
- DAW/pluginval VST3-host testing.
- Live TONE3000 authentication.
- macOS build and behavior validation.

### Mock-only

- Audio transaction, rollback, missing-device, no-audio, and meter/control
  boundaries use deterministic fake adapters in the native setup tests.
- Settings failure/recovery tests use isolated temporary files and injected
  failure paths.
- Provider configuration is validated as an unconfigured/optional boundary;
  no live provider session is asserted.

### Hardware-dependent

- Actual ASIO/DirectSound enumeration, channel capabilities, stream switching,
  audible test output, input-meter response, and unplug/replug recovery remain
  unvalidated on physical hardware.

### Host-dependent

- The VST3 Configuration panel and preserved state behavior were compiled and
  statically checked, but not exercised in a DAW or pluginval host.

### Platform-dependent

- Windows x64 was built. macOS setup UI, CoreAudio behavior, and macOS packaging
  were not implemented or validated in this Windows environment.

## Manual validation

| Check | Result |
| --- | --- |
| Standalone interactive launch/wizard walkthrough | Not run |
| Physical audio interface selection/recovery | Not run |
| Audible output test | Not run |
| Live input meter | Not run |
| VST3 in a host | Not run |
| Safe-mode interactive launch | Not run |
| Setup rerun/repair interactive flow | Not run |

An attempted command-line smoke check was stopped because this Windows
GUI-subsystem build presents `--help`/`--version` through UI dialogs and is not a
noninteractive console command. Command-line behavior is therefore claimed only
from native and static tests, not from that attempt.

## Git

Required repository inspection was run before each commit and again at closure.

- Branch: `main`
- `origin`: `https://github.com/atoontoto/Amphibia.git`
- `upstream`: `https://github.com/sdatkinson/NeuralAmpModelerPlugin.git`
- Submodules remained pinned at their pre-milestone revisions, including iPlug2
  `66f9060b5287afdb2ddb83aab5a2a2a8e66a4d5e`, NeuralAmpModelerCore
  `9c7b185de346fe0725dea537bcee4bc38b5bb6d6`, and AudioDSPTools
  `0827c6c2fc0deced568536142ea86f189e0b98a1`.

Milestone 6 product/evidence commits:

- `3f49767` — docs: map Amphibia first-run configuration flow
- `9ef8002` — feat: add standalone setup state and command-line core
- `b06c1ae` — feat: add Windows first-run setup wizard
- `714c5ef` — feat: share configuration with the VST3 panel
- `8015416` — feat: complete managed library setup controls
- `51de3e6` — test: cover setup migration rollback and panel safety
- `00ab129` — feat: complete setup finish actions
- Documentation closure — the commit containing this report; use the final
  handoff `git log` for its hash.

## Known limitations

- The Milestone 6 native wizard is Windows-only. The macOS APP project still
  follows the inherited generic host path and was not built or validated.
- No physical interface, speaker, microphone, device-unplug, or DAW-host manual
  validation was performed.
- TONE3000 remains honestly unconfigured until production registration and a
  callback contract exist.
- Windows persistent device identities contain backend/enumeration identity and
  distinguish duplicate labels, but an identity can change if a backend
  reorders its inventory.
- Managed-library write-access status reflects successful library
  initialization; setup does not repeatedly perform destructive ACL probes.
- The inherited `settings.ini` remains for iPlug APP audio compatibility while
  versioned JSON owns setup completion and shared projection.
- Inherited numeric-conversion warnings remain in product full builds.
- No installer, component selector, packaging, signing, system-wide deployment,
  AAX, or release publication was started.

## Milestone 7 handoff

Milestone 7 is installer, packaging, and end-user deployment. This milestone did
not implement it.

Stable interfaces now available to Milestone 7 are:

- Windows installer entry to the existing standalone and isolated VST3 outputs;
- explicit VST3 and standalone component boundaries;
- post-install `--first-run`, and maintenance `--setup`/`--audio-settings`
  entry points;
- versioned settings detection, migration, backup recovery, and repair paths;
- a safe-mode launch path;
- documented ownership boundaries for preserving managed content and referenced
  user folders during upgrade, repair, and uninstall;
- stable Amphibia product/version/resource naming for release artifact naming;
- unsigned build outputs ready for a later signing stage; and
- documented macOS gap and settings/content contracts for future packaging.

Milestone 7 must decide installer technology, component selection, upgrade and
repair registration, uninstall preservation policy, artifact naming, signing,
and macOS packaging without moving any of those concerns into the real-time DSP
path.
