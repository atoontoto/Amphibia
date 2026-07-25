# Amphibia milestone checklist

Status legend: `[x]` evidenced in the current repository; `[ ]` not implemented or not yet verified. A milestone closes only when its required evidence is committed. Open release-gate items from an earlier milestone remain visible without invalidating the implemented engineering slice.

## Milestone 0 — upstream investigation and architecture

- [x] Pin and inspect the official plugin, Core, framework, DSP, history, releases, remotes, and submodules.
- [x] Trace local model/IR behavior, loading, state, packaging, CI, A1/A2, and provider constraints.
- [x] Produce the architecture, threat, dependency, identifier, Git, upstream-sync, DSP, and provider plans.

## Milestone 1 — independent identity and renamed baseline

- [x] Freeze independent product, VST3, IDE, bundle, installer, mutex, and data identifiers at version `0.1.0`.
- [x] Rename selected targets/outputs and preserve legacy state reading.
- [x] Quarantine inherited brand assets and add identity validation and non-publishing CI.
- [x] Build Windows standalone and VST3 with the available v145 override.
- [ ] Confirm the final reverse-DNS owner namespace and clear all packaged visual/font provenance.
- [ ] Validate macOS, host discovery, side-by-side installation, and complete binary metadata.

## Milestone 2 — DSP safety and A1/A2 validation

- [x] Characterize parameters, DSP order, resampling, navigation, and legacy state.
- [x] Add bounded NAM/IR inspection, asynchronous preparation, generations, audio-boundary activation, and off-audio reclamation.
- [x] Preserve active DSP on failed, stale, or cancelled work and harden malformed input/state behavior.
- [ ] Validate redistributable released third-party A1/A2 fixtures over the full platform/sample-rate/block-size matrix.

## Milestone 3 — local import, managed library, and state

- [x] Preserve characterized referenced-file navigation and add reviewed individual/folder/multi-file import.
- [x] Add reader-only secure ZIP import, streaming SHA-256, content-addressed deduplication, atomic metadata, verification, recovery, and cleanup.
- [x] Add backward-compatible state 1.1 managed SHA-256 references and retain all legacy readers.
- [x] Add compact Settings statistics/actions and native security/migration tests.
- [ ] Add standalone plugin-session persistence beside iPlug device settings.
- [ ] Add the final missing/corrupt-content recovery UI.

## Milestone 4 — TONE3000 authentication, transport, and provider foundation

- [x] Re-review current API documentation/terms and record the `get_tone`/`load_tone` contradiction without guessing.
- [x] Add provider-neutral remote types and isolate TONE3000 wire/OAuth behavior.
- [x] Add explicit external-browser Authorization Code with PKCE S256 and bounded single-use callback handling.
- [x] Add Windows Credential Manager and macOS Keychain source storage with no plaintext fallback.
- [x] Add Windows WinHTTP transport, typed exact API boundaries, bounded rate/retry policy, and streamed individual download promotion.
- [x] Extend the library to schema 2 provider/creator/license associations while preserving schema 1 and state 1.1.
- [x] Add deterministic mock/loopback tests and provider-disabled CI gates; no live credentials or API are required.
- [x] Keep provider controls inert when unconfigured and preserve all local/DSP behavior.
- [ ] Implement and validate the macOS `NSURLSession` transport and Xcode product integration.
- [ ] Obtain a production client registration, exact approved desktop redirect, support/privacy URLs, and approved brand assets.

## Milestone 5 — hosted TONE3000 selection experience

- [ ] Re-verify terms and resolve the prompt/redirect registration blockers.
- [ ] Complete the provider-approved loopback or custom-scheme callback receiver and live authorization validation.
- [ ] Add explicit hosted `select_tone` entry points for NAM and IR without native unrestricted search.
- [ ] Resolve the returned tone ID, page exact variants, and require an exact user selection.
- [ ] Add final provider brand, creator, license, source-link, and independent-integration presentation.
- [ ] Download/promote only after user action and submit the chosen local object through `_RequestModel()` or `_RequestIR()`.

## Milestone 6 — download/import hardening

- [x] Enforce Windows transport HTTPS/redirect/host policy, timeouts, rate limits, byte caps, streamed validation, hashes, and atomic promotion for the Milestone 4 foundation.
- [ ] Complete equivalent macOS transport coverage and broader malicious HTTP/download corpus, disk-full, interruption, restart, and sanitizer coverage.
- [ ] Add clear-cache/remove-item UX with explicit scope and current provider-term handling.

## Milestone 7 — standalone reliability

- [ ] Persist/restore plugin state and device configuration independently.
- [ ] Recover from missing devices/formats and validate cold-start/offline/cache migration.
- [ ] Run long-duration audio, device-reset, and rapid-switch stress tests.

## Milestone 8 — installers and side-by-side safety

- [ ] Build signed/notarized Windows and macOS packages with exact manifests/notices.
- [ ] Test install, upgrade, repair, uninstall, user-data retention, and NAM coexistence in clean VMs.

## Milestone 9 — CI and release security

- [ ] Pin CI actions/dependencies immutably and add macOS, plugin validation, sanitizers, secret scans, SBOM, and artifact integrity.
- [ ] Gate signing/release publication behind protected environments.

## Milestone 10 — documentation and usability

- [ ] Complete and verify release-user documentation, screenshots, commands, limitations, data deletion, and offline behavior.

## Milestone 11 — release candidate and handoff

- [ ] Complete cross-host/platform validation and resolve or explicitly waive every release blocker.
- [ ] Sign/tag a reproducible release candidate and publish checksums/provenance/notices.

## Milestone 4 evidence record

Milestone 4 implements the provider foundation only: external-browser PKCE/state handling, secure credential storage, bounded WinHTTP transport, typed exact API calls, refresh/session behavior, streamed explicit individual downloads, and schema-2 creator/license provenance. It adds no final catalog/search UI, automatic download/activation, whole-tone ZIP, upload, telemetry, proxy, installer, or DSP behavior.

Windows native test and standalone/VST3 evidence is recorded in `MILESTONE_4_REPORT.md`. Provider tests are mock/loopback-only; no live-provider pass is claimed. macOS Keychain source exists, but macOS HTTP transport/product integration and platform validation remain open. The next milestone is Milestone 5, the user-facing hosted selection experience.

## Global non-claims

Amphibia is not release-ready, production-authorized for TONE3000, signed/notarized, fully macOS-validated, or fully A1/A2 fixture-validated. Completion is evidenced by committed code and test output, not by prose alone.
