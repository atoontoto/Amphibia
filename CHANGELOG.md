# Changelog

All notable Amphibia changes are recorded here. The project has no public release yet.

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
