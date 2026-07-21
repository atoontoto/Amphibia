# Product identifier migration inventory

Status: Milestone 0 exact change list

Allocation status: intentionally pending Milestone 1

Amphibia must install and run side by side with NeuralAmpModeler. Merely changing the visible product name is insufficient. This is the exhaustive identifier class list known from the baseline; Milestone 1 must allocate values, apply them, and run a residual scan.

## Source configuration identifiers

In `NeuralAmpModeler/config.h`, replace or redefine for the Amphibia product:

- `PLUG_NAME`
- `PLUG_MFR`
- `PLUG_VERSION_HEX` and `PLUG_VERSION_STR`
- `PLUG_UNIQUE_ID` (four-character plugin/AU subtype input)
- `PLUG_MFR_ID` (four-character manufacturer/AU manufacturer input)
- `PLUG_URL_STR` and `PLUG_EMAIL_STR`
- `PLUG_COPYRIGHT_STR`
- `PLUG_CLASS_NAME`
- `BUNDLE_NAME`, `BUNDLE_MFR`, and `BUNDLE_DOMAIN`
- `SHARED_RESOURCES_SUBPATH`
- `AUV2_ENTRY`, `AUV2_ENTRY_STR`, `AUV2_FACTORY`, `AUV2_VIEW_CLASS`, and `AUV2_VIEW_CLASS_STR`
- inherited AAX manufacturer/product/type IDs, names, and bundle values, even if AAX is disabled
- resource filename macros for new Amphibia art/fonts/icons

The existing four-character values are `PLUG_UNIQUE_ID='1YEo'` and `PLUG_MFR_ID='SDAa'`; they must not survive in an Amphibia binary.

## VST3 identifiers

iPlug currently derives defaults in `iPlug2/IPlug/IPlug_include_in_plug_src.h`:

```text
processor  = F2AEE70D-00DE4F4E-<PLUG_MFR_ID>-<PLUG_UNIQUE_ID>
controller = F2AEE70E-00DE4F4F-<PLUG_MFR_ID>-<PLUG_UNIQUE_ID>
```

Milestone 1 will generate and freeze explicit, unrelated 128-bit `VST3_PROCESSOR_UID` and `VST3_CONTROLLER_UID` values, plus a new VST3 bundle identifier. The non-distributed build registers the processor class; distributed builds register both. Both IDs must be audited even if only the combined product is released.

Do not reuse the NAM VST3 class ID to preserve host replacement. Amphibia is a separate plugin, and deliberate legacy-state import happens inside Amphibia rather than through identity collision.

## Apple identifiers and symbols

Replace every occurrence in the macOS Xcode project, xcconfig, property lists, xibs/storyboards, Objective-C sources, schemes, and scripts:

- `com.StevenAtkinson.app.*`, `.vst3.*`, `.audiounit.*`, `.aauv3.*`, `.framework.*`, and `.aax.*` bundle identifiers.
- Application, VST3, AU/AUv3/framework/AAX product and executable names.
- AU component manufacturer/subtype codes and AudioComponent names.
- AU factory/entry/view-controller C/Objective-C symbols and XIB class/resource names.
- `CFBundleIdentifier`, `CFBundleName`, `CFBundleDisplayName`, `CFBundleExecutable`, `CFBundleSignature`, icon names, nib names, and document/type declarations.
- Xcode project/target/product names, shared schemes, archive names, and package identifiers.
- macOS entitlement application identifiers/keychain groups if introduced.
- Keychain service/account prefix and OAuth callback scheme registration.
- `pkgbuild`/`productbuild` package identifiers and receipt IDs.

The final reverse-DNS root cannot be chosen responsibly until the Amphibia repository/release owner is known. Use a verified owner-controlled namespace, not `com.StevenAtkinson` and not an invented organization.

## Windows identifiers

Replace/generate:

- Visual Studio solution/project filenames, target names, root namespaces, output binaries, resource metadata, icons, and project GUIDs.
- Current VST3 project GUID `{079FC65A-F0E5-4E97-B318-A16D1D0B89DF}`.
- Current app project GUID `{41785AE4-5B70-4A75-880B-4B418B4E13C6}`.
- Any remaining format project GUIDs even when their targets are disabled.
- Inno Setup `AppId` as a new stable GUID. The upstream script currently has no explicit `AppId`; add one so identity is not an accidental derived default.
- Installer `AppName`, publisher/contact URLs, version, default install/group paths, source/destination bundle names, shortcut names, uninstaller display values, output filename, and registry keys.
- Executable/VST3 filenames and Windows version-resource `CompanyName`, `FileDescription`, `InternalName`, `OriginalFilename`, `ProductName`, copyright, and version.
- Standalone singleton mutex/window-class namespace currently derived from `BUNDLE_NAME`.
- Credential Manager target name, cache/state root, download/cache lock names, named pipes/events if added, and URL/custom-protocol registration if added.

IDE project GUIDs, VST3 class IDs, and Inno `AppId` are three different identity domains and require different fresh values.

## Cross-platform data/network identifiers

Allocate and freeze:

- Application-support/cache/log subpaths.
- State magic (`###Amphibia###`) and schema IDs.
- Library database/index schema namespace and lock name.
- Managed content MIME/UTI/file associations only if needed; Amphibia must not take ownership of `.nam` globally without explicit UX approval.
- Credential service plus per-provider account key.
- OAuth public client ID and exact registered redirect URI. The client ID is provider-issued; the redirect is not chosen until registration.
- User-Agent/product token for API requests, without leaking host/session identity.
- CI artifact names, installer/package IDs, update-channel/feed identifiers if an updater is later approved.

## Names that may remain as attribution or compatibility evidence

- `NeuralAmpModelerCore`, its namespace/includes, and upstream submodule names.
- Root/upstream copyright and license notices.
- `###NeuralAmpModeler###` inside the legacy state reader and its tests.
- Historical names in `docs/UPSTREAM_SYNC.md`, modification maps, changelog attribution, patch provenance, or test fixtures.
- User-facing accurate attribution such as “Based on the open-source Neural Amp Modeler project.”

They must not remain as Amphibia product/bundle/installer identity.

## Milestone 1 audit commands

After rename, CI will maintain an allow-listed residual scan similar to:

```powershell
rg -n -i "NeuralAmpModeler|StevenAtkinson|com\.StevenAtkinson|1YEo|SDAa" Amphibia tests .github
rg -n "079FC65A-F0E5-4E97-B318-A16D1D0B89DF|41785AE4-5B70-4A75-880B-4B418B4E13C6" .
```

Every match must be either removed or explicitly allow-listed as compatibility/attribution documentation. Built binary metadata and installed paths must be inspected as well; a source scan alone is insufficient.
