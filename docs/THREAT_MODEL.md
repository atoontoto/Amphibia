# Amphibia threat model

Status: Milestone 0 baseline, reviewed for Milestone 2 local loading

Review trigger: any new provider, network/archive library, state-schema change, installer change, or credential flow change

## Milestone 2 security status

Milestone 2 adds no network, provider, OAuth, archive, cache, or credential
surface. Local NAM input is capped at 512 MiB and local IR input at 64 MiB
before parser construction. RIFF/WAVE chunk arithmetic, format, channel count,
sample rate, and non-empty data are preflighted before the inherited decoder.
NAM JSON parsing, NAM Core construction, WAV decoding/resampling, and stale or
retired object destruction run only on per-instance workers.

The stale-completion, malformed local NAM/WAV, active-state preservation, and
audio-thread retirement controls below are implemented and covered by the
Milestone 2 deterministic harness. A single indivisible NAM Core operation is
not preemptible, and the complete inherited signal path has not been checked
with a global allocation trap; these remain explicit limitations rather than
weakened invariants for the loading bridge.

## Scope and assets

Protected assets are:

- OAuth access and refresh tokens, authorization codes, and PKCE verifiers.
- User model/IR files and the integrity of managed downloads.
- Host and standalone stability, especially the real-time audio thread.
- User filesystem outside Amphibia's managed directories.
- Plugin state, library metadata, creator attribution, and license metadata.
- Product/update channel integrity and side-by-side isolation from NeuralAmpModeler.

Trust boundaries exist between the audio thread, UI thread, worker services, local filesystem, OS credential service, loopback listener, TONE3000 API/browser, CI runners, and installer/updater.

## Security invariants

1. The audio callback never performs networking, filesystem I/O, parsing, blocking synchronization, unbounded allocation, or object destruction.
2. Network or parser failure never replaces a known-good active model/IR.
3. A downloaded file becomes loadable only after bounds checks, complete transfer, format validation, and final-path atomic rename.
4. Tokens never appear in plugin/standalone state, logs, crash annotations, URLs opened by Amphibia, filenames, analytics, or source control.
5. OAuth callbacks are accepted only for the active request and exact loopback endpoint; `state` and PKCE are mandatory.
6. Archive entries cannot escape a newly created per-request extraction root and cannot overwrite existing files.
7. Install/uninstall operations do not remove NeuralAmpModeler or user content and do not reuse its identifiers.
8. Hosted content is downloaded only after explicit user selection and is not republished, mirrored, or bundled.

## Threats and mitigations

| Threat | Impact | Required controls | Verification |
|---|---|---|---|
| Forged OAuth callback / login CSRF | Account confusion or token theft | Cryptographic random `state`; constant-time comparison; single-use request; short deadline; exact path/method; reject extra concurrent callbacks. | Unit and loopback integration tests for wrong/missing/replayed state. |
| Authorization-code interception | Account compromise | Authorization Code + PKCE S256; random high-entropy verifier; system browser; exchange only over TLS; erase verifier after terminal result. | Known-vector test and provider sandbox flow. |
| Refresh-token theft | Persistent account access | OS credential store; least-readable service/account ACL; redacted logs; explicit disconnect deletes credential; never serialize token. | Platform tests plus repository/log scans. |
| Malicious local process sends loopback response | Login confusion | Bind loopback only, exact active state, one outstanding flow, PKCE, prompt close after terminal response. | Adversarial local HTTP tests. |
| Listener exposed beyond local host | Remote callback injection | Bind `127.0.0.1` (and optionally `::1` only when separately registered/tested), never wildcard; verify bound address; minimal HTTP parser. | Socket inspection integration test. |
| Redirect URI mismatch or undocumented dynamic ports | Auth unavailable | Use only provider-registered exact URI; do not assume wildcard/random-port support; feature remains disabled until registration is confirmed. | Production configuration gate. |
| TLS interception / redirect downgrade | Tampered API or payload | Platform trust store, HTTPS only, strict hostname verification, bounded redirect count, reject HTTPS-to-HTTP redirect, allow-list API/download hosts or revalidate provider-signed locations. | HTTP-client tests with invalid cert and redirect chain. |
| Oversized response / decompression bomb | Memory/disk exhaustion, UI/audio disruption | Header and streaming byte caps, timeouts, cancellation, minimum free-space check, archive entry/count/ratio/uncompressed-size caps. | Generated oversized fixtures and fuzz/property tests. |
| ZIP Slip (`../`, absolute, drive/UNC path) | Arbitrary file overwrite | Normalize separators; reject absolute/rooted/drive/UNC paths, `.`/`..`, empty/control/NUL components; enforce canonical descendant; no symlink/hardlink/device extraction. | Cross-platform malicious archive corpus. |
| Partial/corrupt download activated | Crash or wrong sound | Download to random `.partial`, cap size, hash while streaming, validate full format, `fsync` where practical, atomic rename, then update index. | Kill/restart and corrupt-payload tests. |
| Filename collision / hostile filename | Overwrite or spoofing | Content-addressed managed filenames; display name stored separately; sanitize UI; never trust server filename as path. | Collision and Unicode/control-character tests. |
| Malformed `.nam` JSON or weights | CPU/memory exhaustion or exception | Preflight file-size/depth/count bounds; strict schema/version checks; worker-only parse; catch exceptions; verify input/output topology; construction deadline/cancellation checkpoints. | Malformed corpus, fuzz parser boundary, no-active-swap assertion. |
| Malformed WAV | Overflow, huge allocation, invalid DSP state | Bound RIFF/chunk sizes before allocations, validate integer arithmetic, mono/format/sample-rate/sample-count limits, worker-only decode. Upstream loader requires hardening or guarded preflight. | Crafted RIFF tests and sanitizers. |
| Stale async completion | User selects B, late A becomes active | Monotonic generations and cancellation; publish only current generation; bind UI state to request ID. | Deterministic reordered-completion tests. |
| Destruction/allocation on audio thread | Dropouts or deadlocks | Bounded pointer exchange; non-RT reclamation; preallocate queues; audio-thread instrumentation. | Stress test under rapid swaps and host reset. |
| Cache index corruption or concurrent writers | Lost library or invalid references | Single writer, schema version, temp+atomic replace, checksum/backup, process-level lock with Amphibia-specific name, rebuild from managed files. | Crash-consistency and concurrent-instance tests. |
| Cache path or mutex collision with NAM | Cross-product corruption or singleton conflict | New reverse-DNS identifiers, folders, credential service, mutex, and installer GUID; automated residual-name scan. | Side-by-side install/run/uninstall test. |
| Provider catalog scraping or unintended bulk download | Terms violation/account revocation | Hosted Select for discovery; explicit single asset selection; no background crawl, bulk sync, or catalog persistence; rate limit/backoff. | Mock API call-count/purpose tests and release review. |
| Missing attribution/license metadata | Creator harm or noncompliance | Persist provider, creator, source URL, asset ID, and supplied license metadata; show attribution; never redistribute assets in binaries/tests without permission. | Metadata schema tests and release inventory review. |
| Log leakage | Token, path, or identity disclosure | Structured allow-list logging; redact query/body/auth; path privacy mode; bounded rotating logs; disabled verbose HTTP in release. | Secret canary test and log scan. |
| Compromised CI dependency/action | Malicious release | Pin actions by immutable SHA, least-privilege token, dependency hashes, SBOM, artifact checksums/signatures, protected environments, no fork secret exposure. | Workflow lints and release provenance checks. |
| Untrusted installer/update | System compromise | Platform signing/notarization, published checksums, deterministic artifact inventory, no auto-update until signed design exists. | Clean-VM install and signature verification. |

## OAuth desktop flow

The intended flow is external-browser Authorization Code + PKCE S256:

1. Create 32+ bytes of CSPRNG state and a 43–128 character PKCE verifier; derive base64url SHA-256 challenge without padding.
2. Start a loopback listener only on the exact registered callback address/port. If the provider does not approve a dynamic port, use the registered fixed port or a separately approved custom scheme.
3. Open the provider authorization URL in the system browser. Never embed a client secret; the public client ID is configuration, not a secret.
4. Accept one bounded HTTP request to the exact callback path. Validate method, state, mutually exclusive success/error fields, and presence of code.
5. Return a minimal success/cancel/error browser page, stop the listener, and exchange the code with the original verifier over TLS.
6. Store tokens in the OS credential service. On refresh, atomically replace a rotated refresh token. On `invalid_grant`, delete stored credentials and require explicit login again.
7. Cancellation closes the listener and invalidates state/verifier. Timeouts do the same.

## Download and archive pipeline

TONE3000's documented Select example provides an individual `model_url`, so archives are not required for its initial adapter. A future provider or import feature may supply ZIP data; it must use this pipeline:

```text
network stream
  -> bounded temporary file in managed downloads
  -> transport/hash validation
  -> bounded archive inventory (no extraction yet)
  -> safe-entry validation for every entry
  -> extraction to new private request directory
  -> exact selected .nam/.wav validation and hash
  -> atomic promotion to content-addressed library path
  -> atomic metadata-index update
```

On any failure, the active DSP is unchanged. Temporary data is removed by a best-effort worker cleanup and an age-based startup sweep constrained to Amphibia's verified download root.

## Privacy and data minimization

- No analytics or telemetry in the initial release.
- No catalog or user-library upload.
- API requests contain only provider-required fields.
- Store only selected-item metadata needed for display, attribution, exact restore, and cache repair.
- Provide disconnect, remove-download, clear-cache, and open-cache-folder actions with clear scope.
- Logs omit tokens, request bodies, query secrets, full callback URLs, and—by default—full personal filesystem paths.

## Incident-safe behavior

- Provider outage/auth failure: Online UI reports the condition; local files and current DSP continue.
- Corrupt state/index: quarantine the corrupt metadata file, rebuild what can be proven from managed files, never guess account tokens.
- Missing managed asset: offer explicit re-download if provider metadata exists; never silently substitute a different model variant.
- Queue exhaustion: defer/decline the newest publication with a recoverable status; never block audio.
- Credential backend unavailable: disable login for that session rather than falling back to plaintext.

## Release blockers from this review

- Production OAuth client registration and exact redirect rules are unknown.
- HTTP/TLS and optional ZIP implementations are not selected or audited.
- Hosted downloads still require streaming byte caps and promotion rules before they may feed the local bounded preflight.
- The inherited DSP path outside the Milestone 2 loading bridge still needs allocator instrumentation across the supported host matrix before a product-wide real-time-safety claim.
- Platform signing/notarization and CI provenance are not yet implemented.
