# Milestone 4 TONE3000 foundation map

Status: pre-implementation design and contract gate

Evidence/access date: 2026-07-26

Baseline: completed Amphibia Milestones 0-3 on `main`, derived from
NeuralAmpModelerPlugin `v0.7.15`, NAM Core `v0.5.3`, AudioDSPTools `v0.1.1`,
and iPlug2 `66f9060`.

This map was created before Milestone 4 product-code changes. It records the
current provider contract and the existing Amphibia seams the implementation
must preserve. TONE3000 is an independent third-party service; nothing here
claims endorsement, partnership, registration approval, or production access.

## Authoritative provider review

Reviewed official material:

- API documentation and embedded design requirements:
  <https://www.tone3000.com/api>
- API Terms of Service, effective **2026-07-07**:
  <https://www.tone3000.com/api/terms>
- general Terms of Service, effective 2025-03-12:
  <https://www.tone3000.com/terms>
- Privacy Policy, effective 2025-03-12:
  <https://www.tone3000.com/privacy>
- Tone Sharing Policy: <https://www.tone3000.com/policy>
- provider changelog: <https://www.tone3000.com/changelog>
- official example repository at commit
  `56a528a163f7a16362983e94174c5e86ce5c4496` (2026-07-14):
  <https://github.com/tone-3000/api/tree/56a528a163f7a16362983e94174c5e86ce5c4496>

The legally applicable API Terms control where the broader API landing page
or full-API example advertises capabilities outside Amphibia's free,
non-commercial integration tier.

## Current free-tier boundary

The 2026-07-07 API Terms permit a free published integration only when the
product is free/open and non-commercial with no paid product or upsell. They
limit that tier to:

- hosted OAuth prompt flows named `select_tone` and, in the Terms,
  `get_tone`;
- bounded tone lists: favorited, downloaded, created, trending, and latest;
- user-requested, per-user authenticated content delivery.

The terms prohibit bulk download, scraping, mirroring or catalog caching,
preloading/bundling content, redistribution, white-labeling, account pooling
or proxying, rate-limit circumvention, collecting unrelated user data, and
stripping creator, tone, or license metadata. Full CRUD, training/capture,
native catalog mirroring, and unrestricted native search are not Milestone 4
features. The docs describe `/tones/search` as heavily rate-limited and advise
production callers to contact support; Amphibia does not implement it.

Creator content remains the creator's. Current documented license identifiers
include `t3k`, `cc-by`, `cc-by-sa`, `cc-by-nc`, `cc-by-nc-sa`, `cc-by-nd`,
`cc-by-nc-nd`, and `cco`; unknown future values must be preserved rather than
made more permissive. The Tone Sharing Policy allows analog captures and
permissioned/original commercial-software capture cases and removes prohibited
content. Amphibia must preserve provider creator/license/source provenance.

On revocation or request, the API Terms require deletion of the user's data
from the integration. Disconnect therefore removes credentials and cached
account identity. User-requested downloaded model bytes may remain as local
content only with retained creator/license provenance and subject to the
current content terms; they are never redistributed. Removal of downloaded
content is a distinct, explicit destructive action.

## Prompt-flow discrepancy and production blocker

The current API page, its Authentication/Load Tone sections, and official
example commit all use `prompt=load_tone`. The current API Terms name the
free-tier known-tone prompt `get_tone`. No reviewed changelog entry resolves
the mismatch.

Milestone 4 therefore:

- implements `select_tone` request construction and callback parsing for
  tests and the Milestone 5 handoff;
- permits prompt omission for explicit Connect Account authorization if the
  registered provider application supports it;
- keeps the known-tone prompt value configurable but **disabled in production**;
- does not send either `load_tone` or `get_tone` for a production known-tone
  flow until TONE3000 confirms the legally permitted canonical value;
- does not claim known-tone prompt support.

The API page also advertises Full API Access and a full API example, while the
terms constrain free integrations. Amphibia follows the narrower terms.

## Documented OAuth and redirect contract

- Authorization: `GET https://www.tone3000.com/api/v1/oauth/authorize`.
- Token exchange and refresh:
  `POST https://www.tone3000.com/api/v1/oauth/token`, form encoded.
- Public-client identifier: the publishable key used as `client_id`; the docs
  say it is safe in client binaries. A `t3k_cs...` key is a server-only Bearer
  secret and is forbidden from source, binaries, state, CI, and logs.
- Authorization Code with PKCE `S256`; no plain fallback.
- Required authorization inputs: `client_id`, exact `redirect_uri`,
  `response_type=code`, challenge, `code_challenge_method=S256`, and state.
- Documented prompts: omitted standard authorization, `select_tone`, and API
  docs/example `load_tone` (subject to the blocker above).
- Callback fields: `code`, `state`, optional `tone_id` for hosted tone flows,
  `error`, and documented cancellation information.
- Token response: `access_token`, `refresh_token`, `expires_in`, and
  `token_type=bearer`; the authentication table additionally documents
  `scope`. Refresh uses the same endpoint with `grant_type=refresh_token`,
  refresh token, and client ID. Current examples replace the refresh token on
  every response, so Amphibia treats refresh as rotating. A 400
  `error=invalid_grant` is terminal and clears the invalid session.
- OAuth errors are JSON objects with at least an `error` identifier in the
  documented/example handling. No broader stable error schema is promised.

The API page requires an allowed redirect URI and shows HTTPS web callbacks,
deep links for native mobile applications, development `localhost`, and a
LAN-relay example for headless hardware. It does **not** document production
desktop acceptance of a fixed `127.0.0.1` URI, random loopback port, IPv6
loopback, an Amphibia custom scheme, claimed HTTPS callback, or separate
Windows/macOS callback registrations. Development localhost auto-allowance is
not production approval. The LAN relay exposes a device listener to its LAN
and is explicitly unsuitable for an ordinary desktop/VST3 integration.

No production listener or URI scheme is enabled without a supplied registered
redirect and provider confirmation. Milestone 4 may implement strict callback
parsers/listener abstractions and loopback-only mocks. Production remains
`Unconfigured` when these values are absent.

## Current endpoint and model-download boundary

The API page documents many endpoints; Milestone 4 implements typed methods
only for the foundation needed after a hosted selection and for explicitly
permitted bounded lists:

| Category | Documented endpoint | Milestone 4 boundary |
| --- | --- | --- |
| authorization | `GET /api/v1/oauth/authorize` | URL construction only; explicit system-browser launch |
| tokens | `POST /api/v1/oauth/token` | code exchange and refresh |
| current account | `GET /api/v1/user` | verification after auth/credential load |
| selected tone | `GET /api/v1/tones/{id}` | typed exact-ID metadata resolution |
| selected models | `GET /api/v1/models?tone_id=...` | typed bounded pagination, max documented page size 300 |
| exact model | `GET /api/v1/models/{id}` | typed exact-ID resolution when needed |
| free lists | created/favorited/downloaded/trending/latest | documented permission boundary only; no Milestone 4 client method or UI |
| individual bytes | returned `model_url` | explicit selected-file streaming with Bearer auth as currently documented |

The individual `model_url` is pre-built and requires the user's Bearer token
according to the current model docs. It is transport-only: full URLs and query
signatures are neither persisted nor logged. Redirects are independently
validated and authorization is stripped on a cross-origin hop unless an exact
documented approved host policy requires otherwise.

The partner-only `GET /api/v1/tones/{id}/download` whole-tone ZIP endpoint is
not implemented. Search, users search, favorites mutation, CRUD, upload,
training, tone download ZIPs, and arbitrary endpoint strings are not exposed.

Default rate limit is **100 requests/minute**. Provider policy remains
configurable, honors valid response limits and `Retry-After`, bounds queues and
attempts, and never uses parallel instances/accounts/keys to evade limits.

## Attribution and design requirements

The Terms require “Powered by TONE3000”, an appropriate link to TONE3000,
unaltered approved brand assets, no implied endorsement, and preserved creator
and license information. The API design section requires TONE3000 identity on
entry, list, loaded-tone, and detail experiences and specifies title, gear,
format, creator, imagery, and exact model selection where those surfaces exist.

Milestone 4 adds no final catalog/list/card/attribution experience and vendors
no unapproved logo. Diagnostics use text-only provider identification. The
final compliant hosted selection and attribution surfaces belong to Milestone
5 after approved assets and product wording are available. The published
wireframe's “partnered” wording must not be used without actual approval.

## Existing Amphibia boundaries

- Milestone 2 loading gateway: `_RequestModel()` and `_RequestIR()` are the
  only activation submissions. Provider code cannot call DSP activation.
- Milestone 2 inspection: `InspectRegularFile()` caps NAM at 512 MiB;
  `InspectWaveFile()` caps IR at 64 MiB and validates bounded mono WAV input.
- Milestone 3 promotion: `ManagedLibrary::ImportFiles()` routes ordinary
  validated local paths through a randomized staging copy, inspection,
  platform SHA-256, second-copy hash, content-addressed publication, and
  atomic library transaction. Provider promotion must extend/reuse this seam,
  not duplicate it.
- Milestone 3 associations: `LibraryObject`, `PackRecord`, and
  `PackEntryRecord` retain object and source association data. Schema 1 lacks
  provider provenance, so Milestone 4 requires an explicit schema-2 migration
  preserving object paths, hashes, packs, entries, and classifications.
- State: schema 1.1 stores only optional managed SHA-256 references. It stores
  no provider credentials or signed URLs and need not change for Milestone 4.
- Workers: model, IR, and local import workers are per-instance, joined, and
  off audio. Provider/auth/HTTP/download ownership must be independent and
  joined before library/DSP destruction.

## Existing platform and dependency facilities

- Browser launch: iPlug2 `IGraphics::OpenURL()` delegates to `ShellExecuteW`
  on Windows and `NSWorkspace openURL:` on macOS. It is UI-bound. Milestone 4
  centralizes strict HTTPS URL validation and explicit-action launch behind a
  small browser service so provider flow state outlives an editor.
- Loopback listener: no suitable product listener exists. WDL jnetlib and an
  optional CivetWeb WebSocket extra are present but not product inputs and are
  general server surfaces without the required one-request policy. A minimal
  bounded callback interface is required; actual production binding remains
  disabled pending redirect approval.
- Networking/TLS: product projects link `wininet.lib` through inherited build
  settings but contain no provider transport. No macOS HTTP transport is
  compiled. WDL jnetlib is not an approved HTTPS client boundary.
- JSON: pinned nlohmann/json is already used for NAM/state/library metadata and
  is suitable behind strict byte/depth/count/string bounds.
- Crypto: Windows CNG and macOS CommonCrypto/OS RNG already back Milestone 3
  SHA-256 and random library identifiers.
- Credentials: no product credential service exists. Add Windows Credential
  Manager and macOS Keychain behind one interface; never fall back to plaintext.
- Archive: minizip-ng/zlib remain local ZIP import dependencies and are not
  used for provider whole-tone downloads.

## Transport dependency decision direction

Candidates to evaluate in `HTTP_TRANSPORT.md` are existing WDL/iPlug
facilities, platform-native WinHTTP plus `NSURLSession`, and a maintained
cross-platform library such as libcurl. The preferred Milestone 4 direction is
native WinHTTP/`NSURLSession` behind `IHttpTransport`: maintained OS TLS and
proxy stacks, no new vendored TLS, controllable redirects/cancellation,
streaming, and reproducible SDK-only builds. WDL/WinINet lack the desired
cross-platform policy boundary; libcurl would add a pinned binary/dependency
and TLS-backend build matrix not yet justified. Final selection is documented
before transport product code.

## Desktop/VST3, shutdown, and multiple-instance constraints

- Plugin scans, construction, state restoration, and `ProcessBlock()` cannot
  open browsers, start listeners, load credentials, perform DNS/TLS/HTTP,
  parse provider JSON, wait for rate limits, download, hash, or promote.
- Authorization begins only after an explicit visible user/developer action.
  Closing an editor must not invalidate the operation; destruction must cancel
  it, stop callbacks, cancel HTTP/download work, join owned threads, then
  release provider/library ownership without late UI callbacks.
- Standalone and several DAW processes may share one credential. Refresh must
  use a bounded Amphibia/TONE3000 credential lock and reload/compare the
  credential around rotation; it must not use the library lock or an audio
  lock. Crash recovery treats the OS credential as authoritative.
- Simultaneous identical downloads converge through Milestone 3 content
  hashing and library writer locking. No background synchronization starts.

## Required owner/provider configuration and blockers

The repository does not contain and Milestone 4 will not invent:

- a publishable TONE3000 OAuth client ID/application registration;
- an approved exact desktop redirect URI or loopback/custom-scheme policy;
- approved TONE3000 brand assets or partnership wording;
- final application/repository/privacy/support URLs and contact identity;
- a commercial agreement or broader endpoint permission.

Build-time configuration separates publishable production values from
test-only endpoints and contains no client secret. Builds without production
configuration retain all local Amphibia functionality and show
`provider registration is not configured`; no fake authorization is launched.

## Milestone 4 implementation boundary

Add provider-neutral types and adapter, PKCE/callback foundation, system
browser service, bounded typed transport/API parsing, rate/retry policy,
platform credential stores, refresh/session handling, individual streaming
download, schema-2 provider provenance, and deterministic injected mocks plus
developer/account diagnostics.

Do not add Search TONE3000 behavior, Find IR, a catalog browser, tone cards,
recommendations, final variant/attribution UI, auto-download/activation,
upload/training, proxy/accounts, telemetry, analytics, installer, updater, or
new DSP behavior. Milestone 5 consumes the stable foundation to deliver the
final hosted `select_tone` experience.
