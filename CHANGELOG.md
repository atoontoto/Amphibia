# Changelog

All notable Amphibia changes are recorded here. The project has no public release yet.

## Unreleased - Milestone 8

- Added deterministic CycloneDX 1.5 and exact compiled-asset evidence,
  repository/archive validation, independent schema validation, immutable
  dependency/tool/action pins, and complementary credential/path scans.
- Hardened Windows Release binaries with Control Flow Guard and private-path
  resistant PDB records; added PE import/export/version/hardening/signature
  evidence and an explicit installer license allowlist.
- Rebuilt the standalone/VST3 from clean source, passed Milestones 0-8 static
  and Milestones 2-6 native suites, compiled the unsigned installer, and
  verified the final archives/checksums without installing them.
- Cleared exact Roboto Apache-2.0 and Michroma OFL-1.1 evidence and VST3 SDK
  3.8.0 MIT treatment. Inherited UI artwork and legacy ASIO authority remain
  blocked.
- Recorded the complete PASS/BLOCKED matrix and RC notes. No public release,
  tag, push, upload, signature, clean-VM/DAW/audio/coexistence pass, live
  TONE3000 validation, complete vulnerability result, or macOS pass is claimed.

## Unreleased - Milestone 7

- Replaced the Windows installer placeholder with a versioned x64 Inno Setup
  component installer for standalone and VST3, fixed destinations, optional
  shortcuts, fresh-install setup launch, downgrade refusal, and deterministic
  upgrade behavior.
- Added default-preserve uninstall behavior plus a separately confirmed,
  fixed-root/reparse-safe removal of Amphibia user data and its exact TONE3000
  credential; external NAM/IR/ZIP sources are never deleted.
- Added a clean-worktree packaging entry point with pinned Inno Setup 6.7.3,
  Release/x64/metadata/runtime checks, explicit VST3 reconstruction, provenance
  and dependency manifests, portable archives, checksums, and optional verified
  Authenticode signing.
- Added Milestone 7 installer/package tests, an unsigned short-retention
  development-artifact CI workflow, and install/upgrade/repair/uninstall,
  packaging/signing/releasing/build documentation.
- No stable release, signing certificate, macOS package, clean-VM/DAW/hardware
  pass, or clearance of inherited visual/font assets is claimed.

## Unreleased - Milestone 6

- Added a native Windows nine-page first-run wizard with bounded command-line overrides, completion/cancel/rerun/repair policy, safe mode, and an honest optional-provider page.
- Added DirectSound/ASIO enumeration, input/output/channel/rate/buffer selection, transactional device rollback, output-only mode, a bounded faded test tone, and non-recording peak meters.
- Added shared NAM/IR folder preferences, same-directory validation, managed-library health/counts and six explicit maintenance/import actions, plus legacy INI audio seeding.
- Added schema-1 atomic JSON settings with backup recovery, future-schema rejection, sensitive-field guards, and bounded cross-process reload/merge locking.
- Added an explicit shared standalone/VST3 Configuration panel; the VST3 states that its DAW controls audio and contains no standalone wizard startup path.
- Added Milestone 6 native/fake/static contract tests and setup/security/user documentation. No installer, signing, release publication, macOS wizard validation, real-hardware pass, or DAW-host pass is claimed.

## Unreleased - Milestone 5

- Added fail-closed Unconfigured, DevelopmentConfigured, and ProductionConfigured registration validation without embedding a client secret or inventing production registration values.
- Added hosted `select_tone` request coordination for NAM A1/custom, NAM A2, and cabinet IR entry points using only documented filters.
- Added exact-tone resolution, bounded all-page exact-model listing, compatibility classification, short-lived exact-model refresh, and explicit one-variant download.
- Added a native bounded waiting/variant panel with creator/license/source attribution, pack-local filtering, downloaded-only navigation, and separate Download Only and Download and Audition actions.
- Added a cross-process hosted-selection lease, stale callback suppression, explicit cancellation, safe shutdown, cached exact-association recognition, active-object removal protection, and SHA-256/provenance preservation.
- Added deterministic Milestone 5 tests. Production callback, branding, macOS transport, and live-provider validation remain blocked and are not claimed.

## Unreleased - Milestone 4

- Added provider-neutral remote tone/model/user/download types and a TONE3000 adapter boundary without adding a native catalog browser.
- Added external-system-browser OAuth Authorization Code with PKCE S256, strict single-use callback state, bounded token exchange/refresh, cancellation, and shutdown handling.
- Added Windows Credential Manager and macOS Keychain source implementations with no plaintext fallback, plus bounded cross-process refresh coordination.
- Added WinHTTP HTTPS transport with explicit timeouts, response limits, redirect revalidation, authorization stripping, cancellation, rate limiting, and bounded retries.
- Added typed current-user, exact-tone, exact-model/list parsing and streamed individual-model downloads.
- Extended the managed-library index to schema 2 for provider/creator/license provenance while preserving schema-1 libraries and state 1.1 compatibility.
- Added deterministic mock/loopback OAuth, credential, HTTP, parser, download, promotion, lifecycle, and migration coverage. Live-provider and macOS transport validation remain unclaimed.

## Unreleased - Milestone 3

- Added optional referenced-versus-managed local content modes and platform application-data library paths.
- Added background multi-file/folder scanning, drag-and-drop intent, review confirmation, cancellation, and maintenance tasks.
- Added reader-only ZIP64 inventory and selective extraction with traversal, link, collision, bomb, size, CRC, and disk-space defenses.
- Added streaming SHA-256, content-addressed NAM/IR objects, byte deduplication, pack associations, schema-versioned transactional JSON metadata, verification, staging recovery, and unused cleanup.
- Added optional state 1.1 managed hash references while preserving the Amphibia header, the complete 1.0 prefix, parameter order, and legacy NAM reader.
- Added native generated-fixture coverage and local-library/archive/state/security documentation. No network, OAuth, telemetry, installer, provider, or DSP feature was added.

## Unreleased - Milestone 2

- Added per-instance asynchronous NAM and IR inspection, preparation, and newest-request handling.
- Replaced audio-thread `unique_ptr` staging with bounded atomic activation and worker-thread reclamation.
- Added processing-generation re-preparation so resets do not reconstruct model or IR DSP in the audio callback.
- Added bounded local NAM/WAV preflight and failure behavior that preserves the active stage.
- Hardened current and legacy state parsing while restoring file paths asynchronously.
- Added characterization, state-contract, deterministic concurrency, malformed-WAV, lifetime, and stress tests.
- Documented DSP ownership, threading, state compatibility, validation evidence, and remaining platform/fixture gaps.

## Unreleased — Milestone 1

- Established independent Amphibia product metadata and version `0.1.0`.
- Allocated new plug-in, VST3, AU, Visual Studio, bundle, and installer identifiers.
- Renamed standalone and plug-in outputs while retaining selected upstream technical filenames.
- Added an Amphibia state header and preserved legacy NeuralAmpModeler header reading.
- Quarantined inherited branding from release claims and documented unresolved asset provenance.
- Added repository policies, contribution/build guidance, identity validation, and non-publishing CI foundations.
