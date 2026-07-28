# TONE3000 OAuth and credential lifecycle

Status: fixed-loopback development public client enabled by the
post-Milestone-8 corrective patch; production approval remains gated.

Amphibia uses external-browser OAuth 2.0 Authorization Code with PKCE S256.
Every flow receives an OS-CSPRNG verifier and state. The callback parser caps
the URI at 4 KiB, requires the exact configured callback base, rejects malformed
encoding and duplicate fields, and accepts exactly one of code/error/cancel.
State and verifier material are consumed before validation so every terminal
or invalid callback is non-replayable. A five-minute deadline and explicit
cancel/supersede/shutdown invalidate pending material.

The authorization request contains the publishable `client_id`, exact
`redirect_uri`, `response_type=code`, S256 challenge, state, and only a
documented prompt. `select_tone` is implemented as a hosted-flow boundary.
Connect omits `prompt`. The API docs/example say `load_tone` while the
2026-07-07 API Terms say `get_tone`; known-tone authorization remains disabled
in production until the provider resolves that discrepancy. No client secret
is accepted or embedded.

The token endpoint is a bounded 64 KiB form POST with redirects disabled.
Refresh uses the same endpoint, a five-second cross-process credential lock,
and reload-after-lock semantics. Rotated refresh credentials atomically
replace the prior platform credential and increment a generation. A terminal
`invalid_grant` removes the invalid session; it is not retried indefinitely.

Windows uses Credential Manager generic credentials. macOS source uses a
generic-password Keychain item. The namespace is
`org.amphibiaaudio.Amphibia.TONE3000/oauth-v1`; test coverage uses a separate
`.test` namespace and removes it. Backend failure disables connection rather
than writing plaintext.

The development product receiver uses only the exact registered redirect
`http://127.0.0.1:43821/oauth/callback`. It binds an exclusive retained socket
to `127.0.0.1:43821`, accepts a bounded `GET` for `/oauth/callback`, verifies
the peer is loopback, returns a minimal no-store response, and selects no
fallback. A bind conflict prevents browser launch and offers Retry or Cancel.
Explicit UI action remains the only browser-launch entry.

Shutdown prevents new work, invalidates OAuth, cancels download/transport
handles, suppresses late callbacks, joins native transport workers, then
releases provider ownership. DNS/TLS cancellation can last until the bounded
native timeout. No OAuth or credential operation is reachable from
`ProcessBlock()`.

## Milestone 5 hosted selection lifecycle

The hosted coordinator starts the callback receiver before it asks the provider to create/open the browser authorization. It then owns one task ID, a five-minute deadline, and a cross-process selection lease. `Open Browser Again` reuses only the already-created authorization URL; it never creates a second state/verifier. Cancel, callback failure, timeout, supersession, and shutdown stop the receiver and stale-suppress every later completion.

Hosted NAM uses `prompt=select_tone`, `format=nam`, and documented `gears` values `amp_amp-cab`. Omitting `architecture` is the documented A1/custom hosted view; A2 is a separate explicit `architecture=2` choice. Hosted IR uses `format=ir&gears=cab`. No `/tones/search` request exists.

The callback is not trusted as tone metadata. After exact state/code handling, only its bounded tone ID advances the flow; exact tone and exact models are then fetched from the authenticated API. A provider cancellation is a normal Cancelled terminal state, not an invalid-state warning. Code, state, verifier, callback URI, token, and browser URL never enter the UI snapshot or DAW state.

The Windows products now inject `FixedLoopbackCallbackReceiver` behind
`IHostedSelectionCallbackReceiver`. It starts before provider authorization,
stops on every terminal path, and is neither a LAN listener nor a permanent
server. Production registration/branding and a completed live account flow
remain unclaimed.
