# Security policy

## Supported versions

Amphibia has no supported release yet. Only the current development branch receives security fixes.

## Reporting

Do not include credentials, private models, personal data, or exploit details in a public issue. Once the canonical Amphibia GitHub repository exists, use its private Security Advisory form. A separate security contact has not yet been established; this is a release blocker, not an invitation to publish sensitive details.

For non-sensitive hardening reports, open an issue with affected commit, platform/format, reproduction steps, impact, and any proposed mitigation.

## Scope

Important areas include malformed `.nam`/WAV handling, real-time safety, state deserialization, installer paths, dependency provenance, OAuth callback/state handling, credential storage, HTTP/TLS/redirect policy, archive handling, and downloaded-content validation. See `docs/THREAT_MODEL.md` for the current design review.

Never attach an authorization code, access/refresh token, complete signed model URL, credential-store export, or real provider user metadata to a report. Milestone 4 diagnostics are deliberately redacted and persistent request/body logging is disabled. If the platform credential backend is unavailable, provider authorization must fail closed; plaintext settings, DAW state, environment files, and library metadata are not fallback stores.

The TONE3000 development flow uses a centralized publishable public client,
Authorization Code with PKCE S256, and the exact registered redirect
`http://127.0.0.1:43821/oauth/callback`. It uses no client secret. Reports that
bypass HTTPS, certificate validation, redirect checks, response bounds, or the
Milestone 3 promotion boundary are security regressions, not configuration
workarounds.

Milestone 5 adds a cross-process hosted-selection lease, exact
callback/tone/model identity checks, bounded pagination, short-lived download
URLs, and stale task suppression. The post-Milestone-8 receiver binds an
exclusive retained socket only to `127.0.0.1:43821` before browser launch and
releases it on every terminal path. Do not substitute another/random port,
wildcard/LAN/IPv6 address, custom URI, client secret, scraped brand asset, or
undocumented prompt/filter.

Every exact variant still follows provider download to randomized staging, local inspection, SHA-256, managed promotion, provenance, and a separate explicit load request. Download success must never be treated as DSP activation success. Exact cached-copy removal must protect active and shared objects.

Milestone 6 adds a bounded command-line allow-list, versioned settings with
future-schema rejection, backup recovery, atomic replacement, and a bounded
cross-process merge lock. The standalone wizard performs device operations,
folder validation, library maintenance, settings writes, and diagnostics only
off the audio thread. Its test oscillator is explicitly armed, amplitude/time
bounded, and preconfigured before the callback consumes it. Folder preferences
reject redirected selections and never authorize automatic recursive import.

Do not work around a settings error by inserting provider secrets, changing the
managed-library root, or granting broad permissions. Preserve corrupt settings
for private review and redact local paths/device names.

Milestone 7 constrains Windows package inputs, fixed elevated destinations,
component removal, version downgrade, archive contents, provenance, checksums,
and optional signing. User-data cleanup is default-off, fixed-root validated,
and reparse-safe; silent uninstall cannot request it. Public release still
requires an approved signing identity, clean-VM install/upgrade/uninstall and
NAM-coexistence evidence, asset/license closure, and protected publication.
There is no updater or unattended credential-cleanup mechanism.

Milestone 8 adds immutable release-input/action pins, deterministic CycloneDX
and compiled-asset evidence, archive and import allowlists, CFG-enabled product
binaries, private-path-resistant PDB records, and complementary source/history/
artifact secret scans. These are defense-in-depth controls, not release
authorization. The independent SBOM schema passed, but current C++ vulnerability
review remains blocked by offline database coverage. Public release also
remains blocked on inherited UI-art and legacy ASIO authority, the isolated
Windows/manual matrices, signing, live provider approval, macOS, and a canonical
private security contact.
