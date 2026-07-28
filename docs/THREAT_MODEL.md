# Amphibia threat model

Status: Milestone 6 setup/configuration review (2026-07-26)

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
- Production desktop redirect approval and a publishable client registration remain unavailable.
- WinHTTP is implemented and tested on Windows; the macOS HTTP adapter remains a platform-validation gap.
- The inherited DSP path outside the Milestone 2 loading bridge still needs allocator instrumentation across the supported host matrix before a product-wide real-time-safety claim.
- Platform signing/notarization and CI provenance are not yet implemented.
# Milestone 3 local import additions

Local files, recursive folders, archives, metadata, and the managed index are untrusted. Added controls include bounded traversal without symlink/junction following; ZIP path normalization and duplicate/case collision rejection; special/link/encryption/method rejection; entry/count/depth/type/total/ratio limits; streaming CRC/size checks; pre-extraction free-space checks; create-new no-follow staging writes; content-addressed final paths; reinspection and second hash; root/path containment checks; OS writer locks; atomic JSON replacement; and conservative verification/recovery.

Source files and archives are opened read-only and never cleaned up by Amphibia. Randomized staging is confined to the managed root and removed on cancellation/failure. Unknown data, corrupt metadata, and orphan objects are reported rather than automatically repaired or uploaded. Remaining native-parser and filesystem time-of-check/time-of-use risk is reduced by generated destination names, secure create flags, root reparse rejection, same-filesystem rename, and revalidation, but sandboxed archive parsing is not present.

No network, provider, OAuth, TLS, telemetry, scripting, executable loading, or trusted HTML surface was added.

## Milestone 4 provider threat register

| Threat | Asset | Boundary / attack | Mitigation and test | Remaining risk |
| --- | --- | --- | --- | --- |
| OAuth CSRF | account authorization | browser callback supplies attacker state | 32-byte CSPRNG state, constant-time compare, single use; wrong/replayed-state tests | compromised host process can observe its own memory |
| code interception | authorization code | another process receives callback | PKCE S256 and exact callback; RFC vector/callback-route tests | production redirect strategy awaits approval |
| verifier theft | PKCE verifier | memory/log/state disclosure | move-only pending value, secure clear, never serialize/log; repository/state canary checks | crash dumps may capture live process memory |
| callback hijack | authorization result | wrong route/duplicate/malformed query | exact bounded parser, mutually exclusive result, one pending flow; adversarial parser tests | no production listener is enabled yet |
| custom URI hijack | authorization result | another application registers scheme | custom schemes disabled without approval | future platform registration requires new review |
| loopback abuse | authorization result | non-loopback or multiple requests | production listener disabled; test bypass is explicit loopback-only | approved listener implementation remains work |
| token leakage/replay | access/refresh tokens | logs, state, URLs, metadata | platform credential store, central redaction, no plaintext fallback; canary scans/tests | malicious local debugger can inspect memory |
| refresh race/rotation | refresh token | two host processes refresh old token | five-second named/file lock, reload after lock, generation and atomic replace; contention test | crash between provider rotation and local replace can require login |
| malicious redirect | bearer token | redirect exfiltrates Authorization | automatic redirects off, every hop validated, cross-origin secret stripping; redirect tests | native resolver/connect DNS-rebinding visibility is limited |
| SSRF/private host | host filesystem/network | provider supplies local/model URL | public HTTPS parser plus resolved-address rejection; private/metadata/loopback tests | DNS answer can change after pre-resolution |
| signed URL leakage | download capability | query enters log/library/state | URL redaction and source-page-only provenance; serialized canary test | OS networking diagnostics are outside Amphibia |
| malicious/oversized JSON | memory and metadata | deep/large/missing identities | byte/depth/node/string/count limits and required IDs; parser adversarial tests | nlohmann allocates within the bounded response before tree walk |
| API downgrade/TLS interception | token/content | HTTP or invalid certificate | HTTPS production gate, Schannel trust/hostname checks, no ignore flags; URL policy tests | enterprise trust roots can intentionally intercept TLS |
| rate abuse | provider account | burst/retry storm | 100/min sliding window, bounded queue/attempts, Retry-After; deterministic policy tests | provider limits may change |
| malicious download | model/library | invalid or mislabeled bytes | exclusive `.part`, actual-byte cap, Milestone 2 inspection, Milestone 3 hash/revalidation; valid/invalid tests | NAM parser remains native in-process |
| disk exhaustion | local storage | huge or false Content-Length | free-space reserve, declared and actual caps, cleanup; size tests | space may be consumed concurrently |
| partial confusion | model integrity | truncated file treated complete | Content-Length agreement, flush/close, validation, no resume; truncation test | abrupt power loss after object publication can leave reported orphan |
| provider ID collision | provenance | ID used as physical identity | SHA-256 physical identity, provider/tone/model association tuple; dedup tests | malicious provider metadata can still be misleading UI text |
| creator/license stripping | attribution | dedup/disconnect loses terms | schema-2 independent associations preserve unknown values; migration/provenance tests | provider may omit license metadata |
| stale OAuth/HTTP/download | UI/state | late completion overwrites newer task | generations, weak lifetime, single-use flow and cancellation; supersede/lifecycle tests | native call returns only at timeout in worst case |
| UI/host shutdown race | process stability | callback targets destroyed editor/plugin | UI polling only, weak shared state, suppress callbacks, joined workers; immediate-shutdown tests | abrupt host termination prevents cleanup guarantees |
| concurrent hosted selector | callback/account state | two instances own browser flows | named semaphore/flock hosted-selection lease plus per-task OAuth state; contention test | OS crash releases lease but loses the unfinished task |
| substituted exact variant | downloaded content/provenance | list URL or model identity changes before click | refresh exact model ID and recheck tone/format/architecture before download; deterministic substitution checks | provider may legitimately remove a variant |
| automatic activation confusion | active DSP/project state | callback or download silently replaces sound | callback only opens exact panel; explicit download/audition; managed activation and Active confirmation remain separate | live host/editor validation remains open |
| destructive cache removal | managed bytes/attribution | active or shared object is deleted | exact association tuple, protected active hashes, shared/pinned liveness, metadata-first removal; native test | crash after metadata commit can leave a reportable orphan |

## Milestone 6 setup threat register

| Threat | Asset | Attack | Mitigation | Test | Remaining risk |
| --- | --- | --- | --- | --- | --- |
| Malicious command line | startup integrity | oversized/unknown value becomes a path or shell fragment | exact allow-list, 256-byte bounded diagnostic, terminal help/error before host/audio creation | parser and static startup-order contract | Windows GUI help is interactive, not console output |
| Unsafe settings path | user configuration/filesystem | redirected or broad path escapes Amphibia data root | path is derived from app-support + fixed Amphibia filename; no CLI override | settings temporary-root tests and source review | compromised OS profile/app-support API is trusted |
| Corrupt/future settings | valid configuration | malformed data crashes startup or is overwritten | 1 MiB cap, typed bounds, future-schema refusal, validated backup, safe defaults | corrupt/backup/future-schema tests | backup may also be corrupt; user repair is then required |
| Device-name injection | UI/diagnostics/settings | hostile driver name injects control/log text | bounded strings, native controls, no shell composition, local-only diagnostics | Unicode/duplicate/fake-device tests | third-party driver UI behavior is outside Amphibia |
| Folder-path injection | local files/privacy | crafted path causes recursion, provider leak, or overwrite | filesystem dialog, directory check, no shell input, same-directory validation, local-only JSON | Unicode/missing/file/count/static privacy tests | path may disclose identity if user shares settings/diagnostics |
| Symlink/reparse selection | filesystem boundary | selected folder redirects after validation | reject selected reparse/symlink roots and do not recurse for preference setup | platform policy/source contract; manual redirected-path pass pending | target can change after check; loads still revalidate files |
| Library path redirection | managed objects | root is relocated to unsafe/untrusted location | fixed application-data root, containment checks, relocation absent | Milestone 3 library path/schema tests | compromised root ACL/profile remains an OS risk |
| Output-test loudness | hearing/equipment | automatic, stacked, unbounded test signal | explicit warning/action, 0.05 peak, fade, <=5 s, one atomic instance, stop on transitions | deterministic peak/fade/stack/stop tests | physical gain/speaker sensitivity cannot be inferred |
| Unexpected input monitoring | user privacy/hearing | guitar input is recorded, retained, or routed unexpectedly | meter computes peak only; output-only supported; no input capture storage/upload added | meter and static serialization/privacy contract | driver/DAW routing outside Amphibia may monitor input |
| Provider launch from wizard | account/network privacy | page entry opens browser or starts OAuth | page is informational; unconfigured gate; only explicit existing actions can connect | static page-entry/VST3 contract and provider mocks | future registered UI requires live manual review |
| Setup-state rollback | configuration integrity | Cancel or failed audio overwrites known-good state | draft model; Finish-only completion; device rollback; atomic settings backup | controller cancel/finish and fake transaction tests | previewed active stream is session state until Finish/restart |
| Standalone/VST3 settings race | audio/setup/shared state | plug-in overwrites standalone fields or processes interleave writes | bounded cross-process lock, reload-before-merge, field-specific updates, future-schema refusal | round-trip/update/static ownership contract | last successful shared-folder write wins by design |
| Diagnostics leakage | paths, device identity, secrets | copied status exposes private/local/provider material | bounded local formatting; sensitive fields never stored; no tokens/signed URLs; user review guidance | canary and static serialization checks | current audio diagnostics include local device display names |
| Wizard callback after destruction | process stability | async validation/library completion touches dead window | future ownership checked by message timer, joined worker, no worker UI callback; timer killed on destroy | deterministic worker shutdown from prior suites; manual rapid-close pending | native file dialog/driver calls return on OS timing |

## Milestone 7 installer and artifact review

| Threat | Boundary | Control | Remaining validation |
| --- | --- | --- | --- |
| Elevated arbitrary-path write | installer to machine | fixed Program Files and Common Files VST3 roots; no directory page, service, driver, association, or environment change | clean-VM ACL inspection |
| Package contamination | build tree to artifact | clean source gate, exact binary paths, reconstructed VST3 allowlist, forbidden archive entries, manifest/checksum tests | independent release review/SBOM |
| Downgrade or component residue | installed version/components | stable AppId, version comparison, downgrade refusal, explicit deselected-component deletion | multi-version VM matrix |
| User/external data loss | uninstaller to per-user filesystem | preserve default; silent preserve; separate default-No warning; exact LocalAppData root; reparse refusal; external sources excluded | same-user and credentialed-elevation VM cases |
| Credential overreach | uninstaller to Credential Manager | exact generic target only after explicit cleanup confirmation | multi-account elevation case |
| Unsigned package substitution | publication to user | optional binary/installer Authenticode verification and SHA-256 inventory | approved certificate/protected release channel |
| Pending host file replacement | DAW-loaded VST3 | close-app request, restart replacement semantics, no forced reboot | supported-host/reboot matrix |

There is no updater. Public release remains blocked on signed clean-VM evidence,
NAM coexistence, asset/license closure, and protected publication.
