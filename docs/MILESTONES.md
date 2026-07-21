# Amphibia milestone checklist

Status legend: `[x]` evidenced in the current repository; `[ ]` not implemented or not yet verified. A milestone closes only when all required evidence is committed.

## Milestone 0 — upstream investigation and architecture

- [x] Clone official repository with recursive submodules.
- [x] Pin and record upstream plugin/Core/framework/DSP commits.
- [x] Inspect history, releases, remotes, and existing tags.
- [x] Trace local model/IR selection and previous/next behavior.
- [x] Trace DSP load/prewarm/staging and real-time hazards.
- [x] Trace plugin state compatibility and standalone persistence.
- [x] Trace Windows/macOS projects, packaging, installer, and CI.
- [x] Verify current A1/A2 implementation path and tests.
- [x] Review TONE3000 OAuth, Select, model-resolution, terms, branding, and rate-limit documentation.
- [x] Produce architecture plan, upstream modification map, threat model, dependency/license table, proposed tree, identifier inventory, Git plan, A1/A2 plan, and provider limitations.
- [x] Commit the reviewed Milestone 0 document set. A public tag waits for an Amphibia `origin` and maintainer review.

Exit evidence: this documentation set, clean submodules, validation report, and a single scoped documentation commit. No product-code or build claim is part of this milestone.

## Milestone 1 — independent identity and renamed baseline

- [ ] Confirm repository owner and reverse-DNS namespace.
- [ ] Generate/freeze new VST3 processor/controller IDs, AU codes, IDE project GUIDs, installer `AppId`, bundle IDs, mutex, credentials, and data namespaces.
- [ ] Rename product/class/targets/resources/scripts/installers to Amphibia.
- [ ] Replace/quarantine inherited brand assets and establish asset licenses.
- [ ] Build Windows app + VST3 and macOS app + VST3 from a clean checkout.
- [ ] Validate plugin discovery plus app launch and side-by-side NAM coexistence.
- [ ] Run source and binary residual-identity audits.

## Milestone 2 — DSP safety and A1/A2 validation

- [ ] Add characterization tests for parameters, DSP ordering, resampling, navigation, and legacy state.
- [ ] Implement bounded `NamInspector`/`IrInspector` and async preparation.
- [ ] Implement generation cancellation, real-time exchange, and non-RT reclamation.
- [ ] Eliminate model/IR parsing, preparation, allocation-heavy reset, and destruction from the audio callback.
- [ ] Validate redistributable real A1 and released A2 fixtures over sample-rate/block-size matrix.
- [ ] Harden hosted-facing NAM/WAV bounds and malformed-file behavior.

## Milestone 3 — local provider, library, and state

- [ ] Introduce provider-neutral domain types and `IContentProvider`.
- [ ] Implement Local provider with characterized non-recursive directory navigation and wraparound.
- [ ] Add managed content-addressed library, atomic index, cleanup, and collision policy.
- [ ] Add Amphibia state schema and tested legacy NAM import.
- [ ] Add atomic standalone session persistence beside iPlug device settings.
- [ ] Add missing/corrupt content recovery UI.

## Milestone 4 — content browser UI

- [ ] Preserve compact local controls and all current DSP controls.
- [ ] Add Local/Online content surface, responsive layout, accessibility, keyboard focus, and progress/error states.
- [ ] Add exact variant selection and architecture/source/creator metadata.
- [ ] Validate common host window sizes and HiDPI/Retina scaling.

## Milestone 5 — secure TONE3000 provider

- [ ] Re-verify current API/terms and resolve all documented ambiguities.
- [ ] Obtain publishable client registration and exact desktop redirect.
- [ ] Implement external-browser PKCE, loopback/custom callback, cancellation, refresh rotation, and OS credential storage.
- [ ] Implement hosted `select_tone` flows for A1/Custom, A2, and IR per confirmed contract.
- [ ] Implement post-selection detail pagination and explicit individual model download.
- [ ] Add provider brand/creator attribution and independent-integration disclosure.
- [ ] Pass fake-server and provider sandbox integration tests without secrets in artifacts/logs.

## Milestone 6 — download/import hardening

- [ ] Enforce TLS/redirect/host policies, timeouts, concurrency, rate limits, byte caps, hashes, and atomic promotion.
- [ ] Add secure ZIP inventory/extraction only if a scoped feature requires it.
- [ ] Add malicious archive/NAM/WAV corpus, fuzz/property tests, disk-full/interruption/restart coverage.
- [ ] Add clear cache/remove item/disconnect behavior without destructive uninstall defaults.

## Milestone 7 — standalone reliability

- [ ] Persist/restore plugin state, audio device, sample rate, buffer, channels, and MIDI independently.
- [ ] Recover from missing/changed devices and invalid saved formats.
- [ ] Verify cold start offline, no credentials, corrupt state, and cache migration.
- [ ] Run long-duration audio, repeated device-reset, and rapid content-switch stress tests.

## Milestone 8 — installers and side-by-side safety

- [ ] Build signed/notarized Windows and macOS packages with app and VST3.
- [ ] Install to platform-standard product/plugin locations with unique identities.
- [ ] Include license/notices/SBOM and exact artifact manifest.
- [ ] Test install, upgrade, repair, uninstall, and user-data retention in clean VMs.
- [ ] Prove NeuralAmpModeler remains installed, discoverable, and unaffected.

## Milestone 9 — CI and release security

- [ ] Pin maintained CI actions and dependencies by immutable version/hash.
- [ ] Build/test Windows and macOS, run plugin validation, sanitizers where available, secret scan, dependency/license audit, and artifact inventory.
- [ ] Publish checksums/provenance and gate signing secrets behind protected environments.
- [ ] Create draft prereleases only after all required jobs pass.

## Milestone 10 — documentation and usability

- [ ] Complete README, BUILDING, INSTALL, USER_GUIDE, PRIVACY, SECURITY, THIRD_PARTY_NOTICES, TONE3000 integration, architecture notes, and upstream-sync guide.
- [ ] Verify every command/link/screenshot against release candidates.
- [ ] Document limitations, exact supported formats/architectures, cache paths, data deletion, and provider-offline behavior.

## Milestone 11 — release candidate and handoff

- [ ] Complete cross-host/plugin validation and standalone matrix on clean supported OS versions.
- [ ] Resolve or explicitly waive every security/license/provider/realtime release blocker with named owner and rationale.
- [ ] Tag a signed release candidate from a clean, reproducible commit.
- [ ] Publish artifact hashes, provenance, notices, known limitations, and upstream attribution.
- [ ] Perform post-release installation/download/auth smoke tests from public artifacts.

## Global non-claims

Until the relevant boxes are evidenced, do not claim that Amphibia is buildable, release-ready, fully real-time safe, production-authorized for TONE3000, signed/notarized, or fully A1/A2 validated. Milestone completion is recorded by commit hash and verification output, not by prose alone.
