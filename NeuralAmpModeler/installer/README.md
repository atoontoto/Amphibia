# Amphibia installer

`Amphibia.iss` is the Windows x64 installer source. Compile it only through
`scripts/package-windows.ps1` from the repository root. The script supplies the
version and isolated staging/output directories, validates exact inputs, and
pins Inno Setup 6.7.3. Direct compilation intentionally fails without those
defines.

The installer uses the frozen Amphibia `AppId`, fixed application/VST3 paths,
component selection, default-off desktop shortcut, upgrade/downgrade policy,
fresh-install `--first-run` launch, and default-preserve uninstall behavior.
See `docs/PACKAGING_WINDOWS.md`.

Inherited RTF/background/icon inputs in this directory are not release inputs.
The package allowlist uses root project documentation and a reconstructed VST3
bundle. Do not reintroduce inherited artwork by broad wildcard copying.

The macOS installer package identifiers currently use the temporary technical namespace `org.amphibiaaudio.*`. This does not assert ownership of a matching domain. Replace `INSTALLER_PKG_ID_PREFIX` before release if the project adopts a verified namespace.

`ThirdPartyNotices.txt` is installed with the standalone application and inside the VST3/AU bundle resources. Keep it current when adding, removing, or replacing dependencies. Update source-availability language when a canonical Amphibia repository URL is established.

The inherited macOS scripts also accept:

- `NOTARIZE_BUNDLE_ID`
- `NOTARIZE_BUNDLE_ID_DEMO`
- `APP_SPECIFIC_ID`
- `APP_SPECIFIC_PWD`

Milestone 7 does not validate those macOS scripts as release packaging. Plug-in
identity, DAW compatibility, and saved-session compatibility are controlled
elsewhere, including `config.h`, Xcode bundle identifiers, and plug-in metadata.
