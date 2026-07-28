# TONE3000 integration plan and current limitations

Status: Milestone 5 hosted-selection workflow implemented; live production integration blocked by registration

Reviewed: 2026-07-26; API Terms effective 2026-07-07

Sources: [API documentation](https://www.tone3000.com/api), [API terms](https://www.tone3000.com/api/terms), [official example](https://github.com/tone-3000/api)

This is an independent integration plan. It does not assert endorsement or a partnership with TONE3000.

## Implemented Milestone 5 boundary

The repository now contains provider-neutral interfaces, a TONE3000 adapter,
external-browser PKCE/state handling, bounded token refresh, platform secure
credentials, WinHTTP transport, typed current-user/exact-tone/model endpoints,
streamed individual `model_url` download, schema-2 creator/license provenance,
and deterministic mocks. The default API policy is 100 requests/minute with a
bounded queue/retries and `Retry-After` support.

The Settings page reports the integration and secure-storage status. Because
no publishable client ID or approved desktop redirect is present, it reports
Not configured and keeps Connect/Test/Disconnect/Cancel disabled. It performs
no automatic login, browser launch, request, download, or activation.

Milestone 5 adds visible hosted entry points, architecture pre-selection,
waiting/cancel/reopen states, exact tone resolution, bounded all-page model
listing, compatibility classification, a native exact-variant and attribution
surface, pack-local filtering, explicit individual download, cached-variant
recognition, downloaded-only navigation, removal protection, and an activation
request boundary. Download success and DSP activation remain separate states.

Still not implemented: `/tones/search`, native catalog browsing, whole-tone
ZIP, bulk/background download, automatic model choice, upload/training,
account proxying, local file upload, automatic DSP activation, or unapproved
production branding/callback behavior.

## Permitted initial experience

The initial Online tab will use the provider-hosted `select_tone` OAuth prompt for discovery. Amphibia will not implement unrestricted native text search, scrape public pages, crawl the catalog, bulk-sync, or mirror metadata.

After a successful selection, Amphibia may use the documented authenticated detail flow to:

1. Resolve the selected tone with `GET /api/v1/tones/{tone_id}`.
2. Page through `GET /api/v1/models?tone_id={tone_id}` (documented maximum page size 300).
3. Show the returned exact model/IR variants and their available metadata.
4. Download only the user-selected variant from its bearer-authorized `model_url`.

Whole-tone ZIP download is not required and is described as an approved-partner capability. It will not be used.

## OAuth contract

- Authorization Code with PKCE S256 is mandatory.
- Authorization endpoint: `GET /api/v1/oauth/authorize`.
- Token/refresh endpoint: `POST /api/v1/oauth/token` with form encoding.
- Required authorization values include the public `client_id`, exact `redirect_uri`, `response_type=code`, code challenge/method, `state`, and prompt.
- A publishable `t3k_pub...` client ID may be present in a native binary. A `t3k_cs...` client secret is server-only and must never be added to Amphibia source, binaries, CI variables used by the client, state, or logs.
- A successful hosted selection returns `code`, `state`, and `tone_id`; cancel/error paths must be handled separately.
- Token refresh may rotate the refresh token. Store the replacement atomically. On `invalid_grant`, clear the credential and require an explicit new login.
- Use the external system browser. No embedded credential webview is planned.

Only one authorization request may be active per process. A fresh random state and verifier are required for every request; neither is persisted after completion, cancellation, or timeout.

## Redirect URI decision

The product brief prefers a random loopback port “if allowed.” The current provider documentation says a redirect URI must match the registered list, but it does not promise production wildcard loopback ports or RFC 8252 dynamic-port matching for this service. The official example's automatic localhost allowance is described for development, not a production desktop registration guarantee.

Therefore:

- Do not assume a random production port is allowed.
- Register an exact loopback URI and fixed high port if TONE3000 approves that desktop pattern, binding only `127.0.0.1`.
- If TONE3000 explicitly confirms dynamic loopback ports, prefer a random OS-assigned port and register/construct it per their instructions.
- If loopback is not supported, evaluate a provider-approved private-use custom URI scheme with platform registration and equivalent state/PKCE checks.
- Keep the Online login action disabled with an actionable configuration message until a real publishable client ID and registered redirect are provided.

## Format and architecture filters

The hosted selector documents:

- `format=nam` or `format=ir`.
- Gear filters including `amp`, `amp-cab`, `cab`, `pedal`, `outboard`, `space`, and `experimental`.
- A singular architecture filter of `1`, `2`, or `custom`.

A critical limitation is that omitting `architecture` returns A1 plus Custom and excludes A2. The documentation does not state that the parameter can contain multiple architectures. Amphibia must not invent a comma-separated/multi-value form.

Initial safe UX:

- “TONE3000 A1 / Custom” launches `select_tone` with `format=nam` and no architecture filter, using the documented default behavior.
- “TONE3000 A2” launches a separate selector with `format=nam&architecture=2`.
- “TONE3000 IR” launches a separate selector with `format=ir`.

If the provider confirms a supported multi-architecture selector, the UX may combine the first two. Regardless of provider metadata, the downloaded `.nam` is locally inspected before its architecture label or activation is accepted.

## Exact model selection

A tone may expose multiple `models`. The documented model record includes an ID, name, URL, size enum, tone ID, and `architecture_version` (`1`, `2`, `custom`, or null for non-NAM content). Amphibia will:

- Never auto-pick a model when multiple compatible variants exist.
- Persist `provider=tone3000`, tone ID, exact model ID, local SHA-256, and display/attribution metadata.
- Treat a short-lived model URL as transport data only; never persist it in state.
- Verify that the local file type and inspected architecture agree with the requested content type before activation.
- Keep the prior active model/IR if detail lookup, pagination, download, validation, or loading fails.

## Attribution and presentation

At the Online entry point, show the full approved TONE3000 brand treatment before using a compact `T3K` label in constrained UI. Item/detail surfaces should show the provider mark plus title, gear/format, creator, and exact model selector. Include “Powered by TONE3000” and a link back where the provider terms/guidelines require them.

Use neutral language such as “Independent open-source integration; not endorsed by TONE3000” unless written approval permits different wording. Preserve creator attribution and provider-supplied license/source metadata in the local library.

## Rate limits and resilience

The API documentation currently states a default 100 requests/minute limit, but a separate search endpoint is more heavily limited and production search access requires contact. Amphibia will honor response headers where present, cap concurrency, use bounded exponential backoff with jitter for retryable failures, and not retry authentication or validation failures blindly.

Offline/provider-unavailable behavior:

- Local provider and cached valid assets remain available.
- Online lists show a clear unavailable/offline status.
- No background login prompt or download occurs.
- Cache metadata is not discarded due solely to an API failure.
- A stale presigned/model URL is resolved again only after an explicit user action or restore request.

## Documentation inconsistencies and unresolved items

These are release gates, not implementation guesses:

1. The OAuth/API examples use prompt `load_tone`, while free-tier/terms prose refers to `get_tone`. Initial Amphibia scope needs only `select_tone`; any later load prompt requires confirmation of the canonical name.
2. Random loopback-port support for production native clients is undocumented.
3. No publishable client ID, registered redirect URI, or named TONE3000 registration owner is present in the brief/repository.
4. A single hosted selection spanning both A1 and A2 is undocumented because omission excludes A2 and the architecture parameter appears singular.
5. Native unrestricted search is outside the documented free-tier surface. “Search TONE3000” is product wording for opening the provider-hosted selector, not a native `/tones/search` client.
6. API and terms can change or access can be revoked. Re-verify the contract immediately before implementation and release, and isolate it behind `Tone3000Provider`.
7. Official brand assets and their permissible packaging form must be obtained/confirmed; do not scrape the website logo.

## Validation status

Milestone 4 provides 20 native foundation tests covering PKCE vectors/random
contracts, strict/single-use OAuth callbacks and lifecycle endings, disabled
ambiguous prompts, URL/redirect/redaction/retry/rate policy, typed parsing/API
requests, secure download/promotion/provenance, unconfigured/explicit provider
lifecycle, bounded cross-process refresh locking, real WinHTTP loopback
GET/POST/redirect/limit/truncation/cancellation/streaming, browser validation,
and an isolated Windows Credential Manager store/load/replace/remove cycle.
Schema-1 migration and provider association preservation also run in the
Milestone 3 suite.

These are deterministic mocks and a `127.0.0.1`-only local test server. They
do not establish real provider authorization, production callback approval,
live token refresh/download behavior, certificate failure behavior against a
real service, process-crash recovery, or macOS operation. No registered client
configuration was available, so live-provider validation was not run.

Milestone 5 adds the callback interface and a fail-closed unavailable receiver,
the hosted selection UI/model, explicit variant/attribution presentation,
cross-process browser-flow ownership, and deterministic development mocks.
An actual loopback/custom-URI receiver is intentionally not selected because
TONE3000 has not confirmed Amphibia's production registration strategy.
Production controls remain disabled and no live-provider pass is claimed.

## Milestone 6 setup behavior

The wizard's TONE3000 page is informational and optional. It reads no token and
starts no browser, callback receiver, credential lookup, DNS, or HTTP operation
on entry. In this repository it says that the integration is not configured,
explains that local NAM/IR operation is unaffected, and permits Skip/Next and
normal setup completion. The shared Configuration panel reuses the existing
registration/connection/secure-store status and explicit provider actions.

Setup JSON stores only the boolean `tone3000_configured` readiness projection;
tokens, codes, PKCE values, client secrets, callback material, signed URLs, and
provider payloads are forbidden. See `TONE3000_BRANDING_COMPLIANCE.md`.

## Install and uninstall boundary

No installer artifact contains a provider client ID, secret, token, signed URL,
account payload, model, or brand asset. Normal upgrade/uninstall preserves the
isolated Windows Credential Manager target. Explicit interactive user-data
cleanup calls `CredDelete` only for
`org.amphibiaaudio.Amphibia.TONE3000/oauth-v1`; silent uninstall preserves it.
With credentialed elevation, cleanup may run in the administrator's credential
context, so users should disconnect in Amphibia before uninstall when exact
per-user removal matters.
