# Milestone 1 report — independent identity and repository foundation

Date: 2026-07-21

Outcome: Amphibia now has a distinct, buildable Windows development identity and a repository foundation. It is **not a finished or release-ready end-user product**. macOS builds, DAW discovery, side-by-side installation, complete asset clearance, signed installers, and later product milestones remain open.

## Implemented

- Central product metadata defines Amphibia `0.1.0`, class/output names, unique four-character IDs, explicit VST3 processor/controller FUIDs, AU/AAX symbols, build provenance, data/settings name, and upstream pins.
- Windows solution/project GUIDs, resource metadata, output names, installer placeholder, scripts, and workspaces use Amphibia identity.
- Apple product outputs, bundle identifiers, AU codes/symbols, plists, interface metadata, and shared-scheme build products use Amphibia identity under the documented temporary technical namespace.
- State writing uses `###Amphibia###` with layout version `1.0.0`; reading also accepts `###NeuralAmpModeler###`. The inherited payload layout and DSP behavior were not redesigned.
- Existing About UI identifies Amphibia as independent, open source, derived from upstream, and not the official NAM app.
- Repository README/build/contribution/privacy/security/conduct/attribution/notices/changelog, issue forms, PR checklist, Dependabot configuration, and build-only CI foundations were added. The unsafe/incomplete upstream release workflow was removed.
- A static identity contract checks 35 build-bearing files.

## Frozen identifiers

The authoritative registry is `docs/IDENTIFIERS.md`; the pre-edit allocation record is `docs/MILESTONE_1_RENAME_MAP.md`.

- Plug-in / manufacturer fourcc: `AmPh` / `AmBi`.
- VST3 processor: `7DBF8585-2FC5-4817-AE21-F7910D0330C1`.
- VST3 controller: `893C1354-A5D5-416D-B84C-CA8AE0C27034`.
- Windows app/VST3/AAX projects: `0C7F68EC-0584-439E-BBDF-89E8BB5032D2`, `1FED5C76-68A8-4624-A8D9-716E5EFC7AB0`, `D3F5A13E-3A81-4BBE-A4E9-AE9B8257C8E6`.
- Inno `AppId`: `802D7F30-6B3A-44C3-B809-93DA217821CA`.
- Development bundle root: `org.amphibiaaudio` (temporary technical namespace; no domain-ownership claim).

Canonical owner/repository/website/support values are unknown and remain empty or explicitly marked placeholders. OAuth, credentials, network, provider, and updater identifiers are deferred to their owning milestones.

## Verification performed

Environment: Windows x64; Visual Studio Build Tools 2026 (`18`), MSVC `14.51.36231` / local `v145` override, Windows SDK `10.0.26100.0`. The projects retain their `v143` default for VS 2022/hosted compatibility.

```text
python tests/identity/validate_identity.py
```

Result: passed; 35 identity-bearing files checked.

```text
msbuild Amphibia.sln /t:Amphibia-app;Amphibia-vst3 /p:Configuration=Release;Platform=x64;PlatformToolset=v145;VST3_64_PATH=D:\amphibia\.no-system-install /m /nologo
```

Result: standalone and VST3 succeeded with zero errors. The full inherited build emits existing numeric-conversion warnings. `VST3_64_PATH` pointed to a nonexistent workspace path, so no system plug-in installation occurred.

Final development artifact metadata reports product/description `Amphibia`, company `Amphibia Project`, version `0.1.0`, and internal/original name `Amphibia`. Binary scans found Amphibia and no inherited `1YEo`/`SDAa`; the legacy NAM state header is intentionally embedded.

| Artifact | SHA-256 |
|---|---|
| `NeuralAmpModeler/build-win/app/x64/Release/Amphibia.exe` | `830D6CB94043F1EC33A8BBED8281B05EFEBB1A332AEAA8740BB7BF805BDBCEDC` |
| `NeuralAmpModeler/build-win/Amphibia.vst3/Contents/x86_64-win/Amphibia.vst3` | `E2ED6DD7CBC787FF6813DA452EAE5FDFA5227D336C19C44FBD189F3977E6AE8B` |

A five-second hidden standalone smoke test confirmed the process launched and remained alive, then stopped only the test process. Generated artifacts are ignored and are not release deliverables.

## Asset and license disposition

The inherited Windows icon is no longer compiled and the macOS plist no longer selects the inherited `.icns`. The remaining inherited UI raster/SVG files and Roboto/Michroma files are usable only for developer baseline validation: incomplete provenance makes them a public-release blocker. `ATTRIBUTION.md`, `THIRD_PARTY_NOTICES.md`, `docs/DEPENDENCIES.md`, and `resources/ASSET_PROVENANCE.md` record the gate. No TONE3000 asset is vendored.

## Deliberately retained upstream seams

The `NeuralAmpModeler/` directory, core source filenames, project filenames, metadata filenames, `NeuralAmpModelerCore`, historical REAPER fixtures, attribution links, and legacy state header remain. The upstream `TemplateProject/` scaffold also retains its generic template GUIDs because it is not an Amphibia build target. Installed Amphibia outputs, bundle IDs, class IDs, project GUIDs, and visible product metadata are independent. This balance reduces upstream merge churn without identity collision.

## Not done / blockers

- No macOS machine was available, so app/VST3 compilation and bundle inspection are unverified.
- No DAW or plug-in test host validation was run; VST3 discovery/session restore and coexistence with an installed official NAM plug-in remain unproven.
- No supported installer, signing, notarization, update/repair/uninstall test, or public package exists.
- Asset provenance/replacements, canonical owner/reverse-DNS namespace, repository/support URLs, and private security contact remain unresolved.
- No Milestone 2 asynchronous/real-time DSP work or characterization suite was implemented.
- No content browser, local managed library, TONE3000 UI/API/networking/OAuth, download, ZIP, or credential behavior was implemented.

## Commits

- `64f7290` — product identity.
- `b24c018` — repository documentation/policy.
- `4945d43` — CI/templates/static identity checks.
- This report and final Milestone 1 record are committed separately so the evidence remains reviewable.

## Milestone 2 handoff

Start with characterization tests for parameters, DSP order, resampling, navigation, state import, A1/A2 fixtures, and failure behavior. Then design bounded model/IR inspection and asynchronous preparation without changing audio output unintentionally. Do not start provider/network work as part of Milestone 2.
