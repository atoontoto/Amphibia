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

- [x] Re-verify current terms/API behavior and retain unresolved redirect/registration blockers without guessing.
- [ ] Complete the provider-approved loopback or custom-scheme callback receiver and live authorization validation.
- [x] Add explicit hosted `select_tone` entry points for NAM and IR without native unrestricted search; shipped controls remain disabled while unconfigured.
- [x] Resolve the returned tone ID, page exact variants, and require an exact user selection in the provider-neutral coordinator.
- [x] Add bounded creator, license, source-link, compatibility, progress, and independent-integration presentation; approved production brand assets remain blocked.
- [x] Download/promote only after user action and expose a separate activation request that the product must route through `_RequestModel()` or `_RequestIR()` after configuration becomes available.
- [x] Add deterministic development/mock validation for cancellation, stale callbacks, constraints, exact download, cached audition, and multiple-instance ownership.

## Milestone 6 — first-run setup and configuration

- [x] Add Windows first-run detection, exact command-line overrides, completion/cancel/rerun/repair policy, and safe mode.
- [x] Add DirectSound/ASIO device/channel/rate/buffer selection with transactional rollback, explicit no-audio continuation, bounded output test, and non-recording meters.
- [x] Add referenced NAM/IR folder setup, live managed-library health/counts/actions, and an optional honest TONE3000 page.
- [x] Add schema-1 atomic/backup settings, legacy active-audio seeding, sensitive-field exclusion, and bounded cross-process shared-field merges.
- [x] Add the explicit VST3 Configuration panel with DAW-owned audio and no standalone wizard path.
- [x] Add native/fake/static tests and the first-run/audio/settings/privacy/security documentation set.
- [ ] Validate the native setup flow with real Windows audio hardware and a DAW host.
- [ ] Implement and validate the equivalent macOS standalone setup integration.

## Milestone 7 — installer, packaging, and end-user deployment

- [x] Implement a Windows x64 installer with explicit standalone/VST3
  components and fresh-install `--first-run` support.
- [x] Add deterministic Windows artifact naming, allowlisted staging/archives,
  dependency/source manifests, notices, checksums, and signing hooks.
- [x] Add installer/package static tests, generated-artifact validation, and an
  unsigned development packaging workflow.
- [x] Document install, upgrade, repair, uninstall, cleanup retention, signing,
  packaging, and release operations.
- [ ] Validate clean install, upgrade, repair, uninstall, managed-content
  preservation, and NeuralAmpModeler coexistence in clean Windows VMs/DAWs.
- [ ] Supply and validate an approved Authenticode identity.
- [ ] Implement/validate the equivalent macOS package, signing, and notarization.

## Later release gates

- [ ] Complete macOS provider transport, broad host/plugin validation, real-device/reset stress, sanitizers, SBOM, and artifact integrity.
- [ ] Resolve visual/font provenance, final owner/support identifiers, live provider registration/redirect approval, and all documented release blockers.
- [ ] Sign/tag a reproducible release candidate only after Milestone 7 and the remaining gates pass.

## Milestone 7 evidence record

The Windows installer, packaging pipeline, tests, CI, manifests, checksums, and
documentation are recorded in `MILESTONE_7_REPORT.md`. Fresh Release x64 app and
VST3 builds and all automated Milestone 1–7 tests passed. The evidence artifact
is unsigned. No clean-VM install/upgrade/uninstall, DAW, NAM-coexistence,
certificate-signing, real-hardware, or macOS-package pass is claimed.

## Milestone 4 evidence record

Milestone 4 implements the provider foundation only: external-browser PKCE/state handling, secure credential storage, bounded WinHTTP transport, typed exact API calls, refresh/session behavior, streamed explicit individual downloads, and schema-2 creator/license provenance. It adds no final catalog/search UI, automatic download/activation, whole-tone ZIP, upload, telemetry, proxy, installer, or DSP behavior.

Windows native test and standalone/VST3 evidence is recorded in `MILESTONE_4_REPORT.md`. Provider tests are mock/loopback-only; no live-provider pass is claimed. macOS Keychain source exists, but macOS HTTP transport/product integration and platform validation remain open.

## Milestone 5 evidence record

Milestone 5 adds the visible controls and native bounded selection model, plus a provider-neutral hosted-flow coordinator. It deliberately does not enable a live repository build: the exact approved production callback, publishable client registration, brand assets, and final privacy/repository/support values do not exist. The unconfigured controls do not open a browser, start a receiver, read credentials, or perform network work.

Deterministic evidence is recorded in `MILESTONE_5_REPORT.md`. The exact-variant pipeline is mock-tested through promotion/activation separation, while Release VST3 compilation proves the additive UI and provider sources integrate with the product target. macOS and live-provider validation remain open.

## Milestone 6 evidence record

The Windows standalone wizard, shared settings, VST3 configuration panel, and
managed-library setup projection are recorded in `MILESTONE_6_REPORT.md`.
Device behavior is covered with deterministic fakes and Windows targets compile,
but no real hardware, DAW-host, or macOS manual validation is claimed. Installer,
packaging, signing, and system-wide changes were not started.

## Global non-claims

Amphibia is not release-ready, production-authorized for TONE3000, signed/notarized, fully macOS-validated, or fully A1/A2 fixture-validated. Completion is evidenced by committed code and test output, not by prose alone.
