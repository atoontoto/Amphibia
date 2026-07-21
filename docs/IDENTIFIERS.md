# Amphibia identifier registry

Status: Milestone 1 frozen development identifiers

Rule: these values are compatibility contracts. Change them only through an explicit migration. Amphibia must coexist with, not replace, the official NeuralAmpModeler plug-in.

## Product metadata

| Domain | Amphibia value | Authoritative source |
|---|---|---|
| Product / standalone / binary | `Amphibia` | `NeuralAmpModeler/config.h`, platform configs |
| Manufacturer display name | `Amphibia Project` | `NeuralAmpModeler/config.h` |
| Version | `0.1.0` / `0x00000100` | `NeuralAmpModeler/config.h` |
| C++ plug-in class | `Amphibia` | `NeuralAmpModeler/config.h` |
| Plug-in unique fourcc | `AmPh` | `NeuralAmpModeler/config.h` |
| Manufacturer/AU fourcc | `AmBi` | `NeuralAmpModeler/config.h` |
| State writer magic | `###Amphibia###` | `NeuralAmpModeler/NeuralAmpModeler.cpp` |
| State layout version | `1.0.0` | `AMPHIBIA_STATE_VERSION` in `config.h` |
| Legacy reader magic | `###NeuralAmpModeler###` | compatibility reader only |

## VST3 and native project IDs

| Identifier | Frozen value |
|---|---|
| VST3 processor FUID | `7DBF8585-2FC5-4817-AE21-F7910D0330C1` |
| VST3 controller FUID | `893C1354-A5D5-416D-B84C-CA8AE0C27034` |
| Windows app project GUID | `0C7F68EC-0584-439E-BBDF-89E8BB5032D2` |
| Windows VST3 project GUID | `1FED5C76-68A8-4624-A8D9-716E5EFC7AB0` |
| Windows AAX project GUID | `D3F5A13E-3A81-4BBE-A4E9-AE9B8257C8E6` |
| Visual Studio solution GUID | `283D4B11-8B4F-45F9-B50F-04A1514FF186` |
| Inno Setup `AppId` | `802D7F30-6B3A-44C3-B809-93DA217821CA` |
| AAX native / AudioSuite type IDs | `AmP1` / `AmA1` |

The VST3 FUIDs are represented in iPlug's four-word macro form in `config.h`. They are deliberately unrelated to the official NAM class ID.

## Apple and data namespaces

Milestone 1 uses the temporary technical root `org.amphibiaaudio` for bundle/package identifiers, with format-specific suffixes in the Xcode projects and plists. This is a collision-resistant development namespace, not a claim that the project owns a corresponding domain. It must be reviewed when the repository owner establishes a verified namespace; changing released bundle IDs later would be a migration.

Current shared resource, settings, library, and cache name: `Amphibia`. The iPlug standalone mutex/window namespace derives from `BUNDLE_NAME=Amphibia`, separating it from NeuralAmpModeler.

## Intentionally unresolved

- Canonical Amphibia repository, website, support email, and owner are unknown; URL/email fields remain empty rather than fictitious.
- `amphibia-project` is a documented GitHub-owner placeholder only and is not emitted as a working product link.
- OAuth client ID, redirect URI, credential service/account, User-Agent, provider cache locks, and network identifiers are not allocated because provider integration is outside Milestone 1.
- No global `.nam` file association is claimed.

## Allowed inherited names

`NeuralAmpModelerCore`, source directory/project/resource filenames, upstream license/attribution, historical REAPER fixtures, and the legacy state header may remain. They are technical provenance or compatibility evidence, not installed product identity. See `MILESTONE_1_RENAME_MAP.md` for the allocation record and retained-name rationale.
