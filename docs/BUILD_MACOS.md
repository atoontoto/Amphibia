# Build on macOS

The inherited iPlug2/Xcode targets remain the starting point for macOS developer
builds; see [../BUILDING.md](../BUILDING.md). Milestone 7 did not create or
validate a macOS installer, signing, notarization, provider HTTP adapter, or
native setup-wizard integration.

Do not present the inherited `makedist-mac.sh` output as an Amphibia release.
Before macOS packaging, reconcile the frozen Amphibia bundle/AU identifiers,
replace or prove every packaged visual/font asset, integrate the platform
services documented in the modification map, implement hardened runtime signing
and notarization, and run the equivalent package-content and uninstall-retention
tests.

This explicit limitation prevents the Windows packaging evidence from being
misread as cross-platform release readiness.
