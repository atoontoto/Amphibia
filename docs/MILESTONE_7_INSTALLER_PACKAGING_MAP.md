# Milestone 7 Windows installer and packaging map

Status: pre-implementation design and ownership gate

Evidence date: 2026-07-28

Baseline: completed Amphibia Milestones 0-6 on `main`, product version
`0.1.0`, upstream plug-in base `v0.7.15` / `96337e9`, NAM Core `v0.5.3`,
AudioDSPTools `v0.1.1`, and iPlug2 `66f9060`.

This map was created before changing installer, packaging, release, or
deployment behavior. It records the inherited outputs and every product-owned
or user-owned path that Milestone 7 must preserve.

## Frozen identity and version sources

`NeuralAmpModeler/config.h` is authoritative for the product version:
`PLUG_VERSION_STR` is `0.1.0` and `PLUG_VERSION_HEX` is `0x00000100`.
`NeuralAmpModeler/resources/main.rc` currently repeats that version in Windows
resource syntax and records product/file description `Amphibia`, company
`Amphibia Project`, internal name `Amphibia`, and original filename
`Amphibia.exe` for APP or `Amphibia.vst3` for VST3.

The following compatibility identities are frozen and will be validated, not
regenerated:

- product `Amphibia`, version `0.1.0`;
- plug-in/manufacturer codes `AmPh` / `AmBi`;
- VST3 processor/controller FUIDs in `docs/IDENTIFIERS.md`;
- Windows app/VST3 project GUIDs in `docs/IDENTIFIERS.md`;
- Inno Setup `AppId` `802D7F30-6B3A-44C3-B809-93DA217821CA`;
- bundle, settings, managed-library, state, and credential namespaces.

The packaging script will parse `config.h`, pass the version into Inno through
preprocessor definitions, and fail if the installer, Windows resources,
documentation, or artifact names disagree. No independently edited release
version file will be introduced.

## Existing build outputs

| Output | Existing verified path | Packaging treatment |
| --- | --- | --- |
| Standalone link output | `NeuralAmpModeler/build-win/app/x64/Release/Amphibia.exe` | Copy to installer staging and install as `Amphibia.exe`; the current build already uses the public name, so no source/output rename is needed. |
| VST3 link output | `NeuralAmpModeler/build-win/vst3/x64/Release/Amphibia.vst3` | Validate as the PE x64 plug-in binary, then place inside the staged bundle. |
| iPlug VST3 bundle | `NeuralAmpModeler/build-win/Amphibia.vst3/Contents/x86_64-win/Amphibia.vst3` | Reconstruct an allowlisted bundle containing the x64 binary and approved notice resources. Do not flatten it. |
| Inherited copied notice | `NeuralAmpModeler/build-win/Amphibia.vst3/Contents/Resources/ThirdPartyNotices.txt` | Replace from the repository-controlled release notice during clean staging. |

The inherited bundle builder also creates `Desktop.ini` and `PlugIn.ico`.
`PlugIn.ico` comes from the inherited iPlug/Steinberg shell-art path and is not
cleared as Amphibia branding. It is not required by the VST3 bundle loading
contract and will not enter the release allowlist. No PDB, import library,
incremental-link file, intermediate object, test executable, model, IR,
credential, setting, or downloaded content will be staged.

## Inherited installer audit

The repository contains one Inno Setup script:
`NeuralAmpModeler/installer/Amphibia.iss`. It is an identity placeholder, not a
working release installer.

Useful inherited facts:

- it already contains the frozen escaped-brace `AppId`;
- it selects `app`, `vst3_64`, and fixed `manual` components;
- it intends to normalize an older `Amphibia_x64.exe` output to
  `Amphibia.exe`;
- it intends to install a VST3 bundle under `{cf64}\VST3`;
- it creates a Start Menu group and uninstall shortcut; and
- it enables normal Inno Setup logging.

Unsafe or stale placeholder behavior to replace:

- source paths point at obsolete `build-win/Amphibia_x64.exe` and manual PDF
  outputs that do not exist;
- it permits unsupported/custom program paths while the VST3 destination is
  hard-coded separately;
- commented VST2/AAX/32-bit rules enlarge the audit surface;
- documentation and notices are incomplete and the inherited RTF/license
  inputs are not a controlled Amphibia release set;
- the VST3 copy includes inherited shell art and relies on broad wildcards;
- component rules do not enforce at least one executable product;
- shortcuts are not component-safe and do not expose setup/audio commands;
- it persists an installation log inside Program Files;
- it has no post-install `--first-run`, downgrade policy, explicit repair
  documentation, safe optional user-data removal, or packaging manifest; and
- its output filename is `Amphibia Setup`, not the release artifact convention.

`NeuralAmpModeler/scripts/update_installer-win.py` edits the placeholder in
place. Milestone 7 packaging will not mutate tracked installer source. The
script will either be retired from the Windows release path or changed to
validation-only behavior; compile-time definitions from the packaging script
will carry staged paths and the authoritative version.

## Installer technology decision

Use Inno Setup, preserving the inherited technology and `AppId`.

| Property | Decision |
| --- | --- |
| Tool/version | Pin Inno Setup 6.7.3 for Milestone 7 validation. It is the current stable Inno 6 release reviewed on 2026-07-28; the locally installed compiler was absent at map time. |
| License | Copyrighted Inno Setup license; free/non-commercial project use is supported, while commercial users are requested to purchase a commercial license. Preserve the tool license outside Amphibia's installed application notices because the compiler itself is not redistributed. |
| Compiler | `ISCC.exe` from the pinned Inno Setup installation. |
| Build command | `ISCC.exe /DMyAppVersion=0.1.0 /DStageRoot=<clean-stage> /DOutputDir=<release-dir> NeuralAmpModeler\installer\Amphibia.iss` (final definition names are fixed by the implementation). |
| Privilege model | `PrivilegesRequired=admin`; per-machine installation. |
| Component selection | Native `[Types]`, `[Components]`, `[Tasks]`, and Pascal validation. |
| Upgrade | Stable `AppId`, previous component/task selection, same install roots, version/downgrade check, and Inno replacement/rollback behavior. |
| Repair | Same-version reinstall is the documented repair mechanism; Inno is not represented as an MSI-style maintenance service. |
| Signing | `SignTool`/`SignedUninstaller` hooks and an external packaging sign/verify phase; no certificate or signing claim in this milestone. |
| Silent/logging | Native `/SILENT` or `/VERYSILENT`, `/LOG=<safe path>`, and uninstall logging are documented. Destructive user-data removal remains interactive/explicit and is not implied by silent uninstall. |
| Uninstall | Native uninstaller removes installer-owned files/shortcuts/registration. Default preserves user data; a separate default-No warning controls fixed-root cleanup. |

WiX, NSIS, MSIX, InstallShield, and a second competing installer format are not
introduced. Inno already meets the component, elevation, upgrade, signing-hook,
logging, rollback, and uninstall requirements with substantially lower
migration and maintenance risk.

## Ownership and installation mode

Milestone 7 uses one x64 per-machine installer. Elevation is required because
both supported destinations are machine-wide:

```text
Standalone: C:\Program Files\Amphibia\
VST3:       C:\Program Files\Common Files\VST3\Amphibia.vst3\
```

The standalone directory is fixed rather than user-selected. The VST3 path is
the platform-standard Common Files path and is never selectable. The installer
rejects unsupported 32-bit Windows and does not claim ARM64 support. It creates
no writable executable search path and relies on standard Program Files ACLs.

Inno has no MSI product/upgrade-code pair. Its stable `AppId` owns one
uninstall registration and is the upgrade identity for every future Amphibia
version. The release version changes; the `AppId` does not. The installer must
not inspect, replace, register over, or remove the official Neural Amp Modeler
application, VST3, settings, or uninstall keys.

## Component and shortcut model

The executable components are:

- `standalone` (default selected);
- `vst3` (default selected);
- `documentation` (default selected, available with either executable
  component).

At least one of `standalone` or `vst3` is mandatory. Documentation cannot be
the only selected component. Optional tasks are:

- Start Menu shortcuts (default selected only with standalone);
- desktop shortcut (default unselected, standalone only).

VST3-only installation creates no application, setup, audio, desktop, or
broken documentation shortcut that depends on the standalone. The Start Menu
group contains the applicable subset of:

```text
Amphibia
Amphibia Setup              -> Amphibia.exe --setup
Amphibia Audio Settings     -> Amphibia.exe --audio-settings
Amphibia Documentation
Uninstall Amphibia
```

At completion, `Launch Amphibia setup` is offered only when the standalone was
installed and executes `Amphibia.exe --first-run` through Inno's normal
post-install non-elevated launch behavior. It does not launch a DAW, provider
authorization, browser, model, or IR.

## Product-owned and user-owned paths

### Installer-owned machine paths

| Path/entry | Owner | Default uninstall |
| --- | --- | --- |
| `%ProgramFiles%\Amphibia\` | Installer | Remove |
| `%CommonProgramFiles%\VST3\Amphibia.vst3\` | Installer | Remove |
| Common Start Menu `Amphibia` group | Installer | Remove |
| Optional common desktop shortcut | Installer | Remove |
| Inno uninstall registration for the frozen `AppId` | Installer | Remove |

The installer creates no `.nam`/`.wav` association, App Paths entry, URI
scheme, browser handler, context menu, startup entry, service, scheduled task,
driver, firewall rule, DAW preference, or VST rescan request.

### Amphibia-owned per-user data

Windows `AppSupportPath(..., false)` resolves to Local AppData. Current or
reserved Amphibia-owned data lies beneath the fixed root:

```text
%LOCALAPPDATA%\Amphibia\
  settings.ini
  settings-v1.json
  settings-v1.json.bak
  Library\
    objects\
    packs\
    metadata\
    staging\
    quarantine\
    logs\
  Cache\       (reserved/packaging cleanup policy; not a Program Files path)
  Logs\        (reserved top-level diagnostic location)
```

Provider download parts and ZIP/import staging are created only beneath the
managed `Library\staging` tree. The current managed library is fixed at
`%LOCALAPPDATA%\Amphibia\Library`; it has no relocation setting.

The default uninstaller preserves the entire Local AppData root. Upgrade and
same-version repair do not read, write, migrate, copy, export, or delete it.

An explicit destructive uninstall choice, default No/unchecked, may remove
only the fixed `%LOCALAPPDATA%\Amphibia` root after canonical containment,
root, and reparse checks. It never derives deletion targets from
`settings-v1.json`, `settings.ini`, library metadata, provider metadata, or a
command-line value and never follows junctions/reparse points.

### Credential ownership

TONE3000 OAuth credentials live outside the filesystem in Windows Credential
Manager as the exact Generic Credential target:

```text
org.amphibiaaudio.Amphibia.TONE3000/oauth-v1
```

Default uninstall preserves it. The destructive option may remove only that
exact credential target. It does not enumerate, export, log, or remove any
other credential.

### User-owned references

The `shared.nam_folder` and `shared.ir_folder` settings are references to
external user directories. Original ZIP archives, individual source files,
source folder trees, official NAM data, and any other configured path remain
user-owned. Neither installer nor uninstaller may traverse or delete them,
even when the destructive option is chosen.

## Upgrade, downgrade, repair, and rollback

- Fresh install creates one uninstall entry and only selected components.
- Upgrade with the same `AppId` replaces installer-owned files and preserves
  prior component/task choices where Inno supports them.
- Changing component selection adds/removes only installer-owned component
  files and shortcuts.
- Downgrade to an older package version is blocked with a clear message unless
  a future separately documented development override is used.
- Same-version reinstall is supported and documented as repair. It restores
  selected binaries, documentation, and shortcuts without resetting setup or
  user data.
- Inno's normal transactional file replacement/uninstall log provides
  best-effort rollback. It is not represented as MSI transactional repair.
  Failure cleanup never deletes the Local AppData root or credentials and does
  not launch Amphibia.

## Runtime and operating-system baseline

Release APP and VST3 builds inherit `<RuntimeLibrary>MultiThreaded</RuntimeLibrary>`
from `common-win.props`, i.e. `/MT`. Debug uses `/MTd` but Debug artifacts are
forbidden from packages. The release package therefore does not need to bundle
or bootstrap the Microsoft Visual C++ Redistributable. Dependency inspection
will still verify the final PE import tables.

Expected runtime inputs are Windows system APIs already linked by the product:
Win32 UI/shell, DirectSound/winmm, WinHTTP/Schannel, CNG, Credential Manager,
Winsock, and VST3 host interfaces. Amphibia bundles no ASIO driver, service,
network daemon, updater, or unrelated private DLL. Imported NAM/IR/library
directories never enter `PATH` or DLL search configuration.

The supported packaging target is Windows 10/11 x64. Milestone 7 does not
claim Windows 7/8, Windows ARM, or 32-bit support. The installer will use an
explicit Windows 10 minimum and x64 architecture gate.

## Documentation and release artifacts

Installed documentation will be generated/copied from controlled plain-text
Markdown sources and include getting started, setup/audio, local NAM/IR and
ZIP workflows, managed library, VST3/Reaper discovery, optional TONE3000
status, privacy, troubleshooting, repair/upgrade/uninstall, known limitations,
license, attribution, changelog, and third-party notices. Developer-only
threat and internal milestone maps are not Start Menu entries.

The release staging root is ignored:

```text
artifacts\release\0.1.0\
```

Only actually generated artifacts use these names:

```text
Amphibia-0.1.0-Windows-Setup.exe
Amphibia-0.1.0-Windows-Portable.zip
Amphibia-0.1.0-Windows-VST3.zip
Amphibia-0.1.0-Windows-Standalone.zip
SHA256SUMS.txt
SOURCE-COMMIT.txt
DEPENDENCY-VERSIONS.txt
THIRD_PARTY_NOTICES.txt
```

The portable archive is labelled a manual-install development/test package.
It makes no portability claim beyond its statically linked CRT and listed
Windows system dependencies. SHA-256 is generated after all optional signing
steps and then read back and verified.

`SOURCE-COMMIT.txt` records full repository/submodule pins, branch,
dirty/clean status, upstream base, UTC build time, toolchain, SDK, and Inno
version without machine/user/workspace identity. `DEPENDENCY-VERSIONS.txt`
records only actual compiled/packaged dependencies and notice references.

## Signing readiness

Artifacts are unsigned unless real credentials are supplied and signature
verification succeeds. Packaging hooks accept external `signtool`/certificate
selection and timestamp configuration through environment/CI secrets. No PFX,
password, token, key, certificate, or timestamp credential is committed.

Future order is binary signing, binary signature verification, installer
build, installer signing, installer verification, then final checksums. CI
without credentials produces clearly documented unsigned test artifacts and
does not claim verified publisher or SmartScreen reputation.

## CI and clean-environment strategy

The packaging workflow will be manual and version-tag triggered, with
least-privilege read-only repository permissions. It will:

1. check out recursive pins;
2. build Release x64 APP and VST3 with system-copy post-build behavior
   disabled;
3. run identity, characterization, state, Milestone 2-7, and packaging tests;
4. install/use a pinned Inno Setup 6.7.3 compiler from an authenticated,
   checksum-pinned source or a pinned runner package;
5. create clean allowlisted staging, installer, archives, manifests, and
   checksums;
6. verify contents and checksums; and
7. upload unsigned workflow artifacts without publishing a GitHub Release.

CI runs no live TONE3000 test and receives no provider or signing secret on
pull requests. Runner/tool availability, GUI/DAW/audio testing, Windows
Sandbox nesting, signature reputation, and destructive uninstall validation
cannot be inferred from a packaging job.

Clean installation testing will use Windows Sandbox or a disposable VM, never
the primary development profile. Test fixtures use isolated Local AppData and
external-content roots. The manual matrix covers fresh standalone-only,
VST3-only, combined, component changes, lower-version upgrade, same-version
repair, default uninstall preservation, explicit cleanup boundaries, official
NAM coexistence, and Reaper/plugin validation where available. Every unrun
item remains an explicit gap in the final report.

## macOS packaging gap

Expected future outputs are `Amphibia.app`, `Amphibia.vst3`, and (only if
supported and approved) `Amphibia.component`, installed under standard
Applications and audio plug-in paths. A public package/DMG requires final
bundle namespace ownership, cleared assets, macOS Milestone 6 integration,
Keychain/provider transport parity, CoreAudio validation, Developer ID signing,
Hardened Runtime, notarization, stapling, and a macOS CI runner.

**macOS packaging not validated.**

Milestone 7 will document this gap but will not create an untested macOS
installer.

## Exact implementation file plan

Create:

- `scripts/package-windows.ps1`;
- `tests/milestone7/test_installer.py`;
- `tests/milestone7/test_packaging.py`;
- `.github/workflows/package-windows.yml`;
- `docs/INSTALL_WINDOWS.md`;
- `docs/UPGRADE_WINDOWS.md`;
- `docs/UNINSTALL_WINDOWS.md`;
- `docs/REPAIR_WINDOWS.md`;
- `docs/PACKAGING_WINDOWS.md`;
- `docs/SIGNING_WINDOWS.md`;
- `docs/RELEASING.md`;
- `docs/BUILD_WINDOWS.md`;
- `docs/BUILD_MACOS.md`; and
- `docs/MILESTONE_7_REPORT.md`.

Modify:

- `NeuralAmpModeler/installer/Amphibia.iss`;
- `NeuralAmpModeler/installer/README.md`;
- `NeuralAmpModeler/scripts/update_installer-win.py` if needed to remove
  tracked-file mutation from release packaging;
- `.gitignore`;
- `.github/workflows/build-windows.yml` only if packaging-safe build output
  isolation requires it;
- `NeuralAmpModeler/resources/ASSET_PROVENANCE.md`;
- `README.md`, `PRIVACY.md`, `SECURITY.md`, `CHANGELOG.md`;
- `docs/ARCHITECTURE_PLAN.md`, `docs/DEPENDENCIES.md`,
  `docs/LOCAL_LIBRARY.md`, `docs/MILESTONES.md`,
  `docs/SETTINGS_OWNERSHIP.md`, `docs/THREAT_MODEL.md`,
  `docs/TONE3000_INTEGRATION.md`, `docs/TROUBLESHOOTING.md`, and
  `docs/UPSTREAM_MODIFICATION_MAP.md`.

Product DSP, parameter, state, provider transport, archive, hashing,
managed-library schema, model/IR loading, and audio-thread sources are outside
the Milestone 7 implementation boundary.
