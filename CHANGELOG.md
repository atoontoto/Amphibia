# Changelog

All notable Amphibia changes are recorded here. The project has no public release yet.

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
