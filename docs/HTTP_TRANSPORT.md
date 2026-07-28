# HTTP transport and TLS policy

Status: Milestone 4 dependency decision and implementation contract

Decision date: 2026-07-26

## Decision

Amphibia uses platform HTTP/TLS clients behind `IHttpTransport`:

- Windows: WinHTTP from the Windows SDK (`winhttp.dll`/`winhttp.lib`).
- macOS target: `NSURLSession` from Foundation, not yet implemented or added
  to the product project. The current non-Windows transport stub fails closed.
- Tests: deterministic injected mock transports; local HTTP is accepted only
  by an explicit test policy and never by production provider configuration.

No runtime shell command, `curl.exe`, browser fetch, raw TLS implementation,
embedded browser, certificate bypass, or plaintext production endpoint is
used.

## Alternatives evaluated

| Candidate | Finding | Decision |
| --- | --- | --- |
| Existing iPlug/WDL jnetlib and inherited WinINet link | General legacy socket/HTTP facilities are present, but there is no maintained cross-platform product TLS policy, bounded response API, consistent cancellation, or controlled redirect seam. WinINet is aimed at interactive applications and is not paired with a macOS implementation here. | Rejected for provider transport. |
| WinHTTP + `NSURLSession` | Maintained with the supported OS, system trust/proxy integration, HTTPS, headers, redirects, streamed bodies, cancellation, and no vendored TLS binary. Both can disable automatic redirects so Amphibia reevaluates every hop. | Selected architecture. WinHTTP is implemented; the `NSURLSession` half is deferred and not counted as working. |
| libcurl | Mature, MIT/X-style licensed, portable, cancellable, streaming, and well understood. It would add a pinned source/binary, TLS-backend selection, CA/proxy build matrix, notices, and more project inputs. | Deferred; reconsider only if native parity becomes unmaintainable. |

## Platform API record

### WinHTTP

- Version/pin: Windows SDK and runtime supplied by the supported OS; local
  verification uses SDK 10.0.26100.0.
- License: Microsoft Windows SDK/runtime terms; no redistributed WinHTTP
  source.
- TLS backend/trust: Windows Schannel and system root store; certificate and
  hostname verification remain enabled. No insecure flags are set.
- Cancellation: closing the request handle aborts in-flight work; Amphibia
  also uses cooperative cancellation between streamed reads.
- Redirects: automatic redirect following is disabled. `Location` is bounded,
  parsed, and reevaluated by Amphibia with HTTPS/destination/origin policy.
- Proxy: WinHTTP default/system proxy behavior. Proxy credentials are not
  persisted by Amphibia.
- Disabled behavior: cookies, ambient browser state, client certificates,
  authentication UI, decompression without post-decompression bounds, and
  certificate-ignore flags.
- Remaining risk: DNS/TLS cancellation and OS proxy behavior can take until a
  configured timeout; shutdown is bounded by those timeouts rather than
  claimed instantaneous.

### Planned `NSURLSession` adapter (not implemented)

- Intended version/pin: Foundation supplied by the supported macOS SDK/runtime.
- License: Apple SDK/runtime terms; no redistributed implementation.
- TLS backend/trust: Network/CFNetwork trust evaluation and macOS system trust
  store. Default server-trust validation is retained.
- Intended cancellation: task cancellation plus session invalidation;
  completion must be generation-checked before delivery.
- Intended redirects: a delegate must decline automatic redirects and return
  the bounded response/Location to the shared redirect policy.
- Proxy: system networking policy.
- Disabled behavior: custom trust acceptance, embedded credentials, cookies,
  background sessions, cache persistence, and unbounded response buffering.
- Remaining risk: no Objective-C++ adapter or Xcode product integration exists
  yet, and macOS compilation/runtime verification is unavailable. Provider
  networking therefore remains disabled on macOS and this gap is not counted
  as a pass.

## Shared request contract

`HttpRequest` contains only GET/POST, a validated URL, bounded headers/body,
connect and total deadlines, maximum response bytes, redirect count, endpoint
category, retry policy, and cancellation identity. Metadata/token JSON is
buffered only within its configured cap (token 64 KiB, provider metadata
4 MiB). Model bytes use a separate streamed file API and are never buffered as
one response.

Production URL policy requires HTTPS and rejects userinfo, fragments, invalid
ports, non-HTTP schemes, HTTPS-to-HTTP redirects, local files, loopback,
link-local/private/multicast/unspecified IP literals, and cloud metadata
addresses. DNS results are checked before connection and each redirect is
reevaluated. DNS rebinding risk remains at the OS resolver/connect boundary;
the transport records and checks the connected destination where the native
API exposes it and never relaxes TLS hostname verification.

Bearer authorization is allowed only for the configured TONE3000 API origin
and the currently documented individual `model_url` request. It is stripped
on every cross-origin redirect. Cookies and other sensitive headers are also
stripped. Token POST redirects are rejected. Signed query values are redacted
from diagnostics and complete signed URLs are never persisted.

## Timeouts, response limits, cancellation, and shutdown

- Connect, send/receive, and total timeouts are explicit and finite.
- Header and body sizes are bounded before parsing; an actual streamed byte
  counter enforces limits even when Content-Length is missing or false.
- Request handles have monotonic IDs. Cancelled, superseded, timed-out, and
  post-shutdown completions cannot update current provider/UI state.
- Provider-owned request workers are joined. No thread is detached. Shutdown
  stops new requests, cancels native handles, joins workers within underlying
  timeout limitations, then releases transport ownership.
- None of these operations are reachable from `ProcessBlock()`.

## Rate limits and retries

The initial provider policy is 100 requests/minute, configurable because the
service contract can change. The limiter has a bounded queue and uses a
monotonic clock. HTTP 429 stops immediate retries and honors a valid bounded
`Retry-After` seconds or HTTP-date value. Retry delay is cancellable.

Only idempotent GETs and full-restart individual downloads may retry. Retryable
statuses are 429, 500, 502, 503, and 504 plus selected transient transport
errors. Backoff is bounded exponential with cryptographic/random jitter in
production and injected deterministic jitter in tests. Authorization-code
exchange, invalid grant/state, forbidden, malformed data, unsupported content,
and local storage failures are never blindly retried.

## Build and reproducibility

Windows links `winhttp.lib`. The future macOS adapter will link Foundation;
the current macOS project does not yet include that transport. These are
supported SDK inputs, not new vendored dependencies. Test builds inject mocks
and need no network or credentials. Production builds accept no HTTP endpoint
override; the override exists only behind a test/development configuration
flag.

## Milestone 5 exact-list and download policy

Hosted selection adds no catalog-search endpoint. After one callback-selected tone ID, the provider fetches one exact tone, then pages only `/api/v1/models?tone_id=...` at at most 300 records per page. Pagination is bounded to 16 pages/4,800 variants and rejects changed totals, duplicate model IDs, repeated pages, empty intermediate pages, missing IDs, and impossible metadata. Model URLs remain only in the task snapshot.

An explicit download refreshes `/api/v1/models/{model_id}` and verifies exact tone/model identity, format, and supported architecture before passing its URL to the existing streamed downloader. Listing never downloads bytes. Provider task generations suppress late metadata completions after cancel/shutdown; native transport limits and timeouts remain the Milestone 4 contract.

## Post-Milestone-8 loopback callback boundary

The OAuth callback is deliberately not part of `IHttpTransport`: it is a
minimal fixed local HTTP receiver. On Windows it binds the retained Winsock
listener exclusively to `127.0.0.1:43821`, accepts one bounded HTTP/1.1 `GET`
for `/oauth/callback`, requires the exact Host value and a loopback peer, and
adds `Cache-Control: no-store` to the local response. Request-line/header caps,
finite deadline, cancellation, shutdown, and terminal socket release are
covered by native tests. It does not follow redirects, serve files, accept
LAN traffic, or fall back to another address or port.
