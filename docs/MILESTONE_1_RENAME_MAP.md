# Milestone 1 rename and identifier map

Status: approved implementation map, recorded before source changes

## Identity policy

Amphibia is the permanent product/display name. `amphibia-project` is a clearly non-final GitHub owner placeholder and is not asserted to be an existing account or organization. `org.amphibiaaudio.*` is a temporary technical namespace reserved only inside this source tree; it does not claim ownership of `amphibiaaudio.org` or any matching internet domain. Both values remain centralized and must be reviewed before the first public signed release.

## Authoritative metadata

`NeuralAmpModeler/config.h` remains the authoritative iPlug-compatible metadata file. It will define Amphibia-prefixed constants first and map iPlug's required `PLUG_*`, `BUNDLE_*`, AU, AAX, and VST3 macros to them. This avoids a second conflicting metadata system.

| Metadata | Milestone 1 value | Status |
|---|---|---|
| Product/plugin/standalone name | `Amphibia` | Final name |
| Manufacturer display | `Amphibia Project` | Temporary until a release owner is confirmed |
| Version | `0.1.0` / `0x00000100` | Initial development version |
| GitHub owner | `amphibia-project` | Explicit placeholder; no URL is emitted from it |
| GitHub repository URL | empty | Unconfigured; UI link hidden/disabled |
| Website | empty | Unconfigured |
| Build commit | `unknown` unless injected as `AMPHIBIA_BUILD_COMMIT` | Build-time value |
| Build type | `unknown` unless injected as `AMPHIBIA_BUILD_TYPE` | Build-time value |
| Upstream plugin base | `0.7.15` / `96337e9` | Fixed provenance |
| NAM Core base | `0.5.3` / `9c7b185` | Fixed provenance |
| iPlug2 base | `66f9060` | Fixed provenance |

Future OAuth client configuration is intentionally absent until Milestone 5. The installer UUID is centralized in the installer source and cross-referenced from `IDENTIFIERS.md`; it is not an OAuth secret.

## Identifier allocation

UUIDs were generated with .NET `System.Guid.NewGuid()` and are independent random version-4 UUIDs.

| Identifier | Upstream | Amphibia | Representation / location |
|---|---|---|---|
| Plugin/AU subtype code | `1YEo` | `AmPh` | printable four-character literal; `config.h` |
| Manufacturer/AU code | `SDAa` | `AmBi` | printable four-character literal; `config.h` |
| VST3 processor FUID | `F2AEE70D-00DE4F4E-53444161-3159456F` (derived) | `7DBF8585-2FC5-4817-AE21-F7910D0330C1` | four 32-bit words; `config.h` |
| VST3 controller FUID | `F2AEE70E-00DE4F4F-53444161-3159456F` (derived) | `893C1354-A5D5-416D-B84C-CA8AE0C27034` | four 32-bit words; `config.h` |
| Windows app project GUID | `41785AE4-5B70-4A75-880B-4B418B4E13C6` | `0C7F68EC-0584-439E-BBDF-89E8BB5032D2` | braced UUID; app `.vcxproj` |
| Windows VST3 project GUID | `079FC65A-F0E5-4E97-B318-A16D1D0B89DF` | `1FED5C76-68A8-4624-A8D9-716E5EFC7AB0` | braced UUID; VST3 `.vcxproj` |
| Windows AAX project GUID | `DC4B5920-933D-4C82-B842-F34431D55A93` | `D3F5A13E-3A81-4BBE-A4E9-AE9B8257C8E6` | braced UUID; disabled-format `.vcxproj` neutralization |
| Inno Setup application ID | implicit/derived | `802D7F30-6B3A-44C3-B809-93DA217821CA` | escaped braced UUID; installer source |

## Namespace and output map

| Purpose | Upstream | Amphibia | Status |
|---|---|---|---|
| iPlug bundle root | `com.StevenAtkinson.NeuralAmpModeler` variants | `org.amphibiaaudio.<api>.Amphibia` family | Temporary namespace following pinned iPlug's derived `BUNDLE_ID` convention |
| Standalone bundle | `com.StevenAtkinson.app.NeuralAmpModeler` | `org.amphibiaaudio.app.Amphibia` | Temporary |
| VST3 bundle | `com.StevenAtkinson.vst3.NeuralAmpModeler` | `org.amphibiaaudio.vst3.Amphibia` | Temporary |
| AUv2 bundle | `com.StevenAtkinson.audiounit.NeuralAmpModeler` | `org.amphibiaaudio.audiounit.Amphibia` | Temporary; not an initial release format |
| AUv3 bundle | inherited NAM app-extension/framework IDs | `org.amphibiaaudio.app.Amphibia.AUv3` family | Temporary; neutralized but not an initial release format |
| AAX bundle | `com.StevenAtkinson.aax.NeuralAmpModeler` | `org.amphibiaaudio.aax.Amphibia` | Temporary; neutralized but not an initial release format |
| Settings/application-support directory | `NeuralAmpModeler` | `Amphibia` | Final product namespace |
| Windows singleton/window lookup | `NeuralAmpModeler` | `Amphibia` | Final product namespace |
| Standalone executable | `NeuralAmpModeler_x64.exe` | `Amphibia_x64.exe` (distribution normalized later to `Amphibia.exe`) | Milestone 1 upstream build convention |
| VST3 bundle | `NeuralAmpModeler.vst3` | `Amphibia.vst3` | Final output name |
| macOS app | `NeuralAmpModeler.app` | `Amphibia.app` | Final output name |
| AU component | `NeuralAmpModeler.component` | `Amphibia.component` | Neutralized output name |
| Installer display/artifact | `NeuralAmpModeler Installer` | `Amphibia Setup` | Placeholder only; installer release remains Milestone 8 |

The Windows `_x64` executable suffix is produced by the pinned iPlug post-build convention. Milestone 1 keeps that internal development artifact layout to avoid modifying framework scripts; the future installer may install/rename it as `Amphibia.exe` after installer work is authorized.

## Source and build rename boundary

Change:

- iPlug product metadata, C++ plugin class, AU symbols, binary names, project display/root namespaces, project GUIDs, bundle IDs, resource metadata, window/menu/title strings, and installer placeholders.
- Existing About surface text and build/provenance display.
- Repository workspace/project labels where they are user/developer-facing.

Intentionally retain:

- Root source directory `NeuralAmpModeler/`, source filenames `NeuralAmpModeler.cpp`, `NeuralAmpModeler.h`, `NeuralAmpModelerControls.h`, and `Unserialization.cpp` to reduce upstream merge noise.
- Submodule/directory names `NeuralAmpModelerCore`, `AudioDSPTools`, and `iPlug2`.
- `.nam`, NAM, A1/A2, model metadata, and Core technical terminology.
- Legacy state magic `###NeuralAmpModeler###` and version readers so Amphibia can read compatible upstream state without sharing host identity.
- Upstream names in attribution, history, license, sync documentation, and source provenance.

No DSP methods, parameter definitions, signal flow, file navigation, model/IR loading, resampling, or state layout will be changed in this milestone.
