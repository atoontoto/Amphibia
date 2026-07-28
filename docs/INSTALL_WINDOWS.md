# Install Amphibia on Windows

Amphibia 0.1.0 supports 64-bit Windows 10 and 11. The installer requires
administrator approval because the application is installed below `Program
Files` and the VST3 is installed for all users.

## Before installing

Close Amphibia and any DAW that may have loaded Amphibia. Download the installer
and `SHA256SUMS.txt` from the same release. Verify the SHA-256 value before
running it. A production release must also have a valid Amphibia Authenticode
signature; development CI artifacts are explicitly unsigned and are not stable
public releases.

## Components and destinations

- **Standalone application:** `%ProgramFiles%\Amphibia\Amphibia.exe`
- **VST3 plug-in:** `%CommonProgramFiles%\VST3\Amphibia.vst3`
- **Documentation:** `%ProgramFiles%\Amphibia\Documentation`
- **Start Menu shortcuts:** optional and enabled on the first install
- **Desktop shortcut:** optional and disabled by default

The component page supports standalone-only, VST3-only, both, or a custom
selection. At least one executable component is required. The destination paths
are fixed so upgrades and uninstall remain deterministic. The installer does not
add a file association, alter `PATH`, install a service, install a driver, or
configure a DAW.

After a fresh interactive install, the installer can launch
`Amphibia.exe --first-run` as the original unelevated user. See
[FIRST_RUN_SETUP.md](FIRST_RUN_SETUP.md). Silent installs never launch it.

## VST3 discovery

Restart the DAW and request a VST3 rescan if Amphibia does not appear. The plug-in
is installed in the standard all-users VST3 directory; the installer does not
edit host-specific scan paths. Check the DAW architecture is 64-bit.

No model, impulse response, TONE3000 account, settings, or user library content is
included. Those remain user-owned or are created by Amphibia under
`%LOCALAPPDATA%\Amphibia`.

## Silent install

Inno Setup command-line switches are supported for managed testing. For example:

```powershell
.\Amphibia-0.1.0-Windows-Setup.exe /SILENT /LOG=install.log
```

The normal component defaults apply unless an administrator supplies supported
Inno Setup component/task switches. Test managed deployment in an isolated
machine first.
