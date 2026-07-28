# Package Amphibia for Windows

The only supported Windows release entry point is:

```powershell
.\scripts\package-windows.ps1
```

Run it from a clean `main` worktree after recursively initializing the pinned
submodules and installing the iPlug dependency payloads. Required tools are:

- Visual Studio Build Tools with Desktop C++, MSBuild, `dumpbin`, and a Windows
  10/11 SDK;
- Python 3;
- Inno Setup **6.7.3** (`ISCC.exe`);
- `signtool.exe` when signing is requested.

The script reads `PLUG_VERSION_STR` from `NeuralAmpModeler/config.h`; it does not
mutate tracked files. It rebuilds Release x64 standalone and VST3 targets with
post-build copying disabled, runs the identity/characterization/Milestone 2–7
tests, validates PE architecture/version/dependencies, reconstructs an
allowlisted VST3 bundle, compiles the installer, creates portable ZIPs, rejects
forbidden archive entries, and verifies generated SHA-256 checksums.

Outputs are isolated under `artifacts/release/<version>/`:

- `Amphibia-<version>-Windows-Setup.exe`
- standalone, VST3, and combined portable ZIPs
- `SOURCE-COMMIT.txt`
- `DEPENDENCY-VERSIONS.txt`
- `THIRD_PARTY_NOTICES.txt`
- `ARTIFACT-MANIFEST.txt`
- `SHA256SUMS.txt`

`-Development` permits a dirty worktree. `-SkipBuild` reuses binaries but still
validates them. `-SkipTests` is accepted only with `-Development` and marks the
output unverified. Neither mode is suitable for a release.

The Release projects use the static MSVC runtime (`/MT`); the script confirms no
dynamic MSVC runtime dependency before stating that no Visual C++ Redistributable
is bundled. The compiler itself is not redistributed. Inno Setup licensing and
the generated artifact notices must be reviewed for the intended distribution.

Asset provenance remains a public-release gate. The allowlist deliberately
excludes inherited `Desktop.ini`, `PlugIn.ico`, installer background art, models,
IRs, settings, credentials, PDBs, caches, and logs.
