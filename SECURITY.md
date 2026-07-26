# Security policy

## Supported versions

Amphibia has no supported release yet. Only the current development branch receives security fixes.

## Reporting

Do not include credentials, private models, personal data, or exploit details in a public issue. Once the canonical Amphibia GitHub repository exists, use its private Security Advisory form. A separate security contact has not yet been established; this is a release blocker, not an invitation to publish sensitive details.

For non-sensitive hardening reports, open an issue with affected commit, platform/format, reproduction steps, impact, and any proposed mitigation.

## Scope

Important areas include malformed `.nam`/WAV handling, real-time safety, state deserialization, installer paths, dependency provenance, OAuth callback/state handling, credential storage, HTTP/TLS/redirect policy, archive handling, and downloaded-content validation. See `docs/THREAT_MODEL.md` for the current design review.

Never attach an authorization code, access/refresh token, complete signed model URL, credential-store export, or real provider user metadata to a report. Milestone 4 diagnostics are deliberately redacted and persistent request/body logging is disabled. If the platform credential backend is unavailable, provider authorization must fail closed; plaintext settings, DAW state, environment files, and library metadata are not fallback stores.

The TONE3000 production flow is intentionally unconfigured in this repository. A publishable client ID and approved redirect are required before live testing. Reports that bypass HTTPS, certificate validation, redirect checks, response bounds, or the Milestone 3 promotion boundary are security regressions, not configuration workarounds.

Milestone 5 adds a cross-process hosted-selection lease, exact callback/tone/model identity checks, bounded pagination, short-lived download URLs, and stale task suppression. The shipped callback receiver fails closed because no approved production desktop callback exists. Do not enable the buttons by substituting an arbitrary port, wildcard redirect, LAN listener, custom URI, client secret, scraped brand asset, or undocumented prompt/filter.

Every exact variant still follows provider download to randomized staging, local inspection, SHA-256, managed promotion, provenance, and a separate explicit load request. Download success must never be treated as DSP activation success. Exact cached-copy removal must protect active and shared objects.
