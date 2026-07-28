# Amphibia release process

Milestone 7 provides a Windows packaging candidate; it does not declare a stable
public release. A release operator must complete every remaining gate.

1. Confirm `main` is clean, reviewed, and synchronized with the exact `origin`.
2. Recursively verify pinned submodules and dependency/license inventory.
3. Resolve every visual/font/provider-brand provenance item in
   [DEPENDENCIES.md](DEPENDENCIES.md).
4. Complete real-hardware, supported-DAW, side-by-side NAM, clean-VM install,
   upgrade, repair, uninstall, and reboot/pending-file tests.
5. Package on the approved Windows toolchain with an approved signing identity.
6. Verify signatures, installer metadata, fixed destinations, archive allowlists,
   source commit, dependency manifest, and all checksums independently.
7. Test default-preserve and explicitly confirmed cleanup behavior under both
   same-user elevation and credentialed elevation.
8. Review license/notices and obtain any required Inno Setup commercial license.
9. Publish immutable artifacts and checksums through an approved release channel.
10. Create a signed tag only after the evidence is archived and all blockers are
    closed.

The GitHub packaging workflow deliberately uploads a short-retention artifact
named `unsigned-development`. It is a build/review aid, not a release channel.
There is no auto-updater.
