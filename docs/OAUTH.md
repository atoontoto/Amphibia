# TONE3000 OAuth and credential lifecycle

Status: Milestone 4 foundation; production authorization remains gated on
registered publishable configuration and an approved exact desktop redirect.

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
`org.amphibiaaudio.Amphibia.TONE3000/default`; test coverage uses a separate
`.test` namespace and removes it. Backend failure disables connection rather
than writing plaintext.

No production callback listener or custom URI registration is enabled: the
current provider material does not confirm fixed/random desktop loopback or a
private-use scheme. Tests inject callback URIs and bind mock HTTP only to
loopback. Explicit UI action is the only browser-launch entry.

Shutdown prevents new work, invalidates OAuth, cancels download/transport
handles, suppresses late callbacks, joins native transport workers, then
releases provider ownership. DNS/TLS cancellation can last until the bounded
native timeout. No OAuth or credential operation is reachable from
`ProcessBlock()`.
