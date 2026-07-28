# Upgrade Amphibia on Windows

Close Amphibia and all DAWs, verify the new package checksum and signature, then
run the newer installer. Amphibia uses the frozen Inno Setup application ID
`802D7F30-6B3A-44C3-B809-93DA217821CA`, so a newer package upgrades the existing
Windows installation instead of creating a second uninstall entry.

The installer preserves the previous setup type and task choices where Inno
Setup can do so. Files for deselected components are removed from Amphibia's
fixed install locations. Downgrades are blocked: uninstall the newer version
first if an older version is required.

Settings, the managed library, cache, logs, stored TONE3000 credential, external
NAM/IR folders, and original ZIP archives are not modified by upgrade. Project
and preset compatibility is governed by the frozen plug-in identity and state
contracts documented in [IDENTIFIERS.md](IDENTIFIERS.md) and
[STATE_SCHEMA.md](STATE_SCHEMA.md).

After upgrading, restart the DAW. A rescan is normally unnecessary because the
VST3 path and identity do not change, but a host cache may require one. An
upgrade does not auto-run the first-run wizard.
