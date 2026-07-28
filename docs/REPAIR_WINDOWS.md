# Repair Amphibia on Windows

There are two different repair operations:

1. **Application setup repair:** run `Amphibia.exe --setup` or choose **Amphibia
   Setup** from the Start Menu. This repairs audio/content configuration and
   keeps the installed binaries unchanged.
2. **Installation repair:** close Amphibia and all DAWs, then rerun the same or
   newer installer and select the required components. This replaces the
   allowlisted application/VST3/documentation files.

If a newer version is installed, the older package intentionally refuses to
overwrite it. Uninstall first only when a deliberate downgrade is necessary.

For a missing VST3, confirm
`%CommonProgramFiles%\VST3\Amphibia.vst3\Contents\x86_64-win\Amphibia.vst3`
exists, restart the DAW, and perform a full VST3 rescan. For a damaged managed
library, use the setup/library health actions; do not reinstall over external
model folders.
