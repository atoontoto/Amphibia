# Amphibia release process

Milestone 8 provides an unsigned Windows validation candidate and an explicit
PASS/BLOCKED checklist; it does not declare a releasable RC or stable public
release. A release operator must close every item in
`RELEASE_CHECKLIST.md` with actual evidence.

1. Confirm `main` is clean, reviewed, and synchronized with the exact `origin`.
2. Recursively verify pinned submodules, `release/dependency-pins.json`, the
   CycloneDX SBOM, compiled-asset record, notices, and every downloaded-input
   checksum.
3. Resolve every remaining visual/provider-brand/ASIO authority item in
   [DEPENDENCIES.md](DEPENDENCIES.md) and
   [LICENSE_REVIEW_WINDOWS.md](LICENSE_REVIEW_WINDOWS.md).
4. Complete real-hardware, supported-DAW, side-by-side NAM, clean-VM install,
   upgrade, repair, uninstall, and reboot/pending-file tests.
5. Package on the approved Windows toolchain with an approved signing identity.
6. Verify signatures, installer metadata, fixed destinations, archive
   allowlists, source commit, SBOM schema/content, dependency manifest, secret
   scans, vulnerability results, and all checksums independently.
7. Test default-preserve and explicitly confirmed cleanup behavior under both
   same-user elevation and credentialed elevation.
8. Review license/notices, confirm the MSVC/SDK build license, and make the
   owner policy decision for Inno Setup if distribution becomes commercial.
9. Publish immutable artifacts and checksums through an approved release channel.
10. Create a signed tag only after the evidence is archived and all blockers are
    closed.

The GitHub packaging workflow deliberately uploads only a seven-day artifact
labelled `unsigned-not-for-public-release` under read-only repository
permissions. It is a build/review aid, not a release channel. CI success cannot
close environment-dependent gates. There is no auto-updater.

Do not tag, upload, publish, sign, or shorten the candidate label while the
checklist decision is `BLOCKED FROM PUBLIC RELEASE`.
