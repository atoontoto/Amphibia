# Milestone 4 TONE3000 foundation report

Evidence date: 2026-07-26

Baseline: completed Milestones 0-3 on `main`; product version and frozen identity remain `0.1.0`.

## Outcome

Milestone 4 adds a secure, testable, provider-neutral TONE3000 authentication,
transport, exact-metadata, individual-download, and managed-promotion
foundation. It deliberately does not add Search TONE3000 behavior, Find IR,
native unrestricted search, catalog cards, final variant/attribution UI,
automatic download, or automatic DSP activation. Local referenced/managed
content remains available when provider configuration is absent or offline.

The product build has no registered client ID or approved desktop redirect.
The Settings diagnostic therefore reports that provider registration is not
configured and leaves Connect, Disconnect, Test, and Cancel inert. Milestone 5
will consume the stable interfaces to implement the explicit hosted selection
experience after provider registration and branding requirements are resolved.

## Official documentation findings

Official provider material was accessed on **2026-07-26**. API Terms are
effective **2026-07-07**. Sources and the complete legal/technical map are in
`MILESTONE_4_TONE3000_FOUNDATION_MAP.md`.

- Authorization: `GET /api/v1/oauth/authorize`; token exchange/refresh:
  `POST /api/v1/oauth/token`; PKCE method: `S256`.
- Documented prompts: omitted account authorization and `select_tone`. The API
  page/example uses `load_tone`, while the Terms name `get_tone`; known-tone
  authorization is configurable but disabled until TONE3000 clarifies it.
- The controlling free tier permits hosted prompt flows and bounded
  created/favorited/downloaded/trending/latest lists for free open-source,
  non-commercial integrations. It does not authorize scraping, mirroring,
  bulk/background synchronization, account proxying, redistribution, or
  unrestricted native catalog search.
- Default limit: 100 requests/minute. The implementation bounds queueing and
  retries and honors a valid bounded `Retry-After`.
- Implemented post-selection endpoints are current user, exact tone, exact
  model, and paginated models for an exact tone. The documented page-size cap
  is 300.
- A model record provides a bearer-authorized individual `model_url`. It is
  short-lived transport data and is not persisted. The whole-tone ZIP endpoint
  is partner-only and not implemented.
- Token fields are `access_token`, rotating `refresh_token`, `expires_in`,
  `token_type=bearer`, and optional `scope`. `invalid_grant` is terminal and
  removes the stored credential.
- Redirects must match provider registration. The reviewed material does not
  promise production random-port loopback, a fixed Amphibia loopback URI,
  IPv6 loopback, or an Amphibia custom scheme. No production callback strategy
  was invented.
- Required presentation includes approved TONE3000 identity/branding,
  `Powered by TONE3000` and link treatment where required, no implied
  endorsement, and retained creator/license/source metadata. No unapproved
  logo is vendored.
- Disconnect removes credentials and cached account identity. Downloaded local
  bytes and provenance require a separate explicit removal action and remain
  subject to current creator/provider terms.
- Official example repository reviewed at
  `56a528a163f7a16362983e94174c5e86ce5c4496` (2026-07-14).

## Dependency decision

| Input | Version/pin and license | Purpose and behavior | Remaining risk |
| --- | --- | --- | --- |
| WinHTTP/Schannel | Windows SDK/runtime; verified SDK 10.0.26100.0; Microsoft SDK/runtime terms | HTTPS GET/POST and streamed files, system trust/proxy, native redirects disabled, cancellable request handles | DNS/TLS/native proxy work can take until finite timeout; OS trust roots remain authoritative |
| Windows Credential Manager | Supported Windows runtime (`advapi32`); Microsoft SDK/runtime terms | Per-user token storage, load, replace, remove; no plaintext fallback | Same-user local processes and crash dumps remain inside the local OS threat boundary |
| Windows CNG | Supported Windows runtime (`bcrypt`); Microsoft SDK/runtime terms | CSPRNG, SHA-256, PKCE/state, jitter | No custom crypto; no FIPS claim |
| Security.framework Keychain | Supported macOS SDK/runtime; Apple SDK/runtime terms | Source implementation for per-user credential storage | Not compiled or runtime-tested here |
| System browser APIs | `ShellExecuteW` on Windows; planned AppKit/Foundation path on macOS; OS terms | Explicit external authorization launch | Browser/host behavior still needs interactive host testing |
| `NSURLSession` | Planned system Foundation API | Intended macOS HTTPS/redirect/cancellation parity | Adapter and Xcode integration do not exist yet; macOS provider networking is disabled |

No third-party networking/TLS/credential dependency was added. `libcurl` was
deferred to avoid a new binary/TLS-backend matrix; inherited WDL/WinINet was
rejected as the provider policy boundary. Cookies, browser ambient state,
certificate bypasses, client secrets, background sessions, archive download,
and response-body logging are disabled/not used.

## OAuth architecture

PKCE uses OS cryptographic randomness, a standards-bounded verifier,
base64url-without-padding SHA-256 challenge, and no plain fallback. State and
verifier are single-use and cleared where practical. Every flow has an exact
redirect, bounded query/code, deadline, cancellation, supersession, and
shutdown invalidation. Only explicit user action can launch the validated HTTPS
authorization URL in the system browser.

The callback parser is production-ready as a bounded value boundary, but no
production listener or URI registration is enabled. Code exchange is a
bounded form POST using the exact flow verifier, redirect, and publishable
client ID; there is no client secret. Refresh takes a five-second bounded
cross-process lock, reloads the credential after locking, atomically replaces
a rotated refresh token, and removes terminal invalid grants. Disconnect
cancels auth/download work and removes the credential. Weak shared state and
joined transport workers suppress post-shutdown delivery.

```text
explicit user action
  -> OAuthController: CSPRNG state + PKCE verifier/challenge
  -> system browser: registered authorize URL
  <- approved callback adapter: exact route + state + code/tone_id
  -> token endpoint: code + verifier + exact redirect + public client ID
  <- token set
  -> OS credential store
  -> exact user/tone/models request
  -> explicit individual model download
  -> .part -> inspect -> SHA-256 -> managed promotion/provenance
  -> (Milestone 5 explicit action only) _RequestModel() / _RequestIR()
```

## Secure storage

Windows uses Generic Credentials under service
`org.amphibiaaudio.Amphibia.TONE3000`, account `oauth-v1`. Tests override both
with `.test` names and always remove them. macOS Keychain store/load/update/
delete source exists behind the same interface; platform build/runtime
validation is deferred. Invalid keys/values, missing/corrupt records, access
failures, and unavailable platforms return structured errors. There is no
plaintext fallback. Tokens are absent from ordinary settings, state, library
metadata, filenames, source-controlled configuration, and persistent logs.

## Transport

Production URLs require HTTPS, default certificate/hostname validation, public
destinations, bounded valid ports, and no userinfo/fragments. WinHTTP automatic
redirects are disabled; each hop is reparsed, HTTPS downgrade is rejected, and
Authorization/cookies/API-key headers are stripped cross-origin. Token POSTs
do not redirect. Metadata is capped at 4 MiB and token responses at 64 KiB;
streamed NAM/IR limits use the existing 512 MiB/64 MiB inspection caps and an
actual-byte counter. Connect/send/receive/total timeouts are finite.

The limiter defaults to 100 requests/minute with a bounded queue. Only
idempotent GET/full-restart downloads may use bounded exponential backoff with
jitter for selected transport failures and 429/500/502/503/504. Auth code
exchange, validation failures, forbidden responses, and invalid grants are not
blindly retried. Cancellation applies to active native handles and retry waits;
shutdown rejects new work and joins workers.

## Provider API

Implemented endpoint categories:

- authorize URL construction: `GET /api/v1/oauth/authorize`;
- code exchange and refresh: `POST /api/v1/oauth/token`;
- current user: `GET /api/v1/user`;
- exact tone: `GET /api/v1/tones/{id}`;
- models for an exact tone: `GET /api/v1/models?tone_id=...`;
- exact model: `GET /api/v1/models/{id}`;
- bearer-authenticated individual `model_url` streaming.

Not implemented: `/tones/search`, unrestricted catalog endpoints, mutation,
created/favorited/downloaded/trending/latest list clients, favorites changes,
users search, upload/training/capture, whole-tone ZIP,
bulk/background download, catalog mirroring, local file upload, proxy accounts,
or automatic download.

## Download and promotion

An explicit exact model URL streams to a randomized managed `payload.part`.
Declared and actual bytes, free-space reserve, redirects, cancellation, and
truncation are checked. The file is renamed only inside its staging directory,
validated by the existing NAM/WAV inspector, hashed by the Milestone 3 SHA-256
implementation, copied/rehashed through `ManagedLibrary`, deduplicated by
content hash, and published with an atomic schema-2 association.

The association retains provider/tone/model string IDs, titles, creator,
unknown-preserving license values, canonical source page, format/gear/
architecture, verification times, and object hash. It never retains the full
signed/download URL or credential. Failed/cancelled parts are removed.
Downloaded content is not automatically activated; the coordinator returns a
managed SHA-256 for a later explicit `_RequestModel()`/`_RequestIR()` request.

## Real-time safety

None of the following is reachable from `ProcessBlock()`: browser launch,
OAuth, callback handling, credential access, HTTP, DNS, TLS, JSON parsing,
rate-limit waits, downloads, hashing, library promotion, token refresh, or
provider logging/string formatting. Provider workers do not touch iPlug
controls. The only handoff to audio in Milestone 5 will be an already-promoted
local path through the existing Milestone 2 request/generation/audio-boundary
pipeline.

## Files changed

- Provider-neutral interfaces: `Providers/ProviderTypes.h`,
  `Providers/IRemoteToneProvider.h`.
- TONE3000 adapter/OAuth: configuration, PKCE, callback/controller, typed
  parsing/API client, provider session, and download coordinator under
  `Providers/Tone3000/`.
- Credentials/platform: credential types/interface/store, refresh lock, and
  `Platform/SystemBrowser.*`.
- HTTP: request types/interface, URL/redaction policy, retry/rate policy, and
  WinHTTP transport under `Network/`.
- Library: schema-2 records/parser/writer, schema-1 migration, provider staging
  and promotion in `LibraryIndex.*`, `LibraryTypes.h`, `ManagedLibrary.*`.
- UI/settings: minimal provider-disabled diagnostics in
  `NeuralAmpModeler.cpp` and `NeuralAmpModelerControls.h`.
- Build/CI: Windows props/app/VST3 projects, `.gitignore`, and Windows workflow.
- Tests: Milestone 3 migration/provenance extension and `tests/milestone4/`.
- Documentation: foundation map, OAuth, HTTP, provenance, schema/state/
  threading/threat/architecture/upstream/dependencies/identifiers/milestones,
  integration, README, privacy, security, notices, changelog, and this report.

## Commands run

Representative exact verification commands (all from `D:\amphibia`):

```powershell
python tests\identity\validate_identity.py
python tests\characterization\test_inherited_contract.py
python tests\characterization\test_state_contract.py

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone2\Milestone2Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:2 /nologo
& .\tests\milestone2\build\x64\Debug\Milestone2Tests.exe
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone2\Milestone2Tests.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /m:2 /nologo
& .\tests\milestone2\build\x64\Release\Milestone2Tests.exe

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone3\Milestone3Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:2 /nologo
& .\tests\milestone3\build\x64\Debug\Milestone3Tests.exe
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone3\Milestone3Tests.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /m:2 /nologo
& .\tests\milestone3\build\x64\Release\Milestone3Tests.exe

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone4\Milestone4Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:2 /nologo
& .\tests\milestone4\build\x64\Debug\Milestone4Tests.exe
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\milestone4\Milestone4Tests.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /m:2 /nologo
& .\tests\milestone4\build\x64\Release\Milestone4Tests.exe

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-app /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /p:VST3_64_PATH=D:\amphibia\.no-system-install\m4-Debug /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-app /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:VST3_64_PATH=D:\amphibia\.no-system-install\m4-Release /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-vst3 /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /p:VST3_64_PATH=D:\amphibia\.no-system-install\m4-Debug /p:PostBuildEventUseInBuild=false /m:1 /nologo
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' NeuralAmpModeler\Amphibia.sln /t:Amphibia-vst3 /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:VST3_64_PATH=D:\amphibia\.no-system-install\m4-Release /p:PostBuildEventUseInBuild=false /m:1 /nologo

git diff --check
git status --short
git branch --show-current
git remote -v
git log --oneline -n 25
git submodule status --recursive
```

## Builds

MSBuild 18.8.2.30814, MSVC 19.51.36248, x64, project default v143 with local
`PlatformToolset=v145` override. System VST3 installation/post-build copying
was disabled; isolated paths were used.

| Target | Configuration | Result and output | SHA-256 |
| --- | --- | --- | --- |
| standalone | Debug | PASS — `build-win/app/x64/Debug/Amphibia.exe` | `B386C4EF846833219BE84EC7787ADF0408368A98C14F9D076AC00AE026FFCB80` |
| standalone | Release | PASS — `build-win/app/x64/Release/Amphibia.exe` | `2337DD52B500E5A7271E40BE6A8B8A2CEE3432D7B8BE619BF455B98EB363ED12` |
| VST3 | Debug | PASS — `build-win/vst3/x64/Debug/Amphibia.vst3` | `0BA7DF737679D3BA9437B0515B5C972CBE6B95E92B610D3520FD5DA641BD6A0A` |
| VST3 | Release | PASS — `build-win/vst3/x64/Release/Amphibia.vst3` | `B5238981AC124E30FE0DCEA0F33AF84FA9EB0BCB072B7EBDB334F40989A98F40` |

Recompiled product targets emitted existing C4244/C4305 conversion warnings
from inherited Colors, NAM Core/Eigen, iPlug controls, and resampler/control
code; no Milestone 4 provider source warning was observed. A full Release test
compile emitted four existing minizip C warnings (C4267, C4152, C4201, C4189).
All builds had zero errors.

## Tests

Passed:

- identity validation (35 identity-bearing files), inherited characterization,
  and state compatibility;
- Milestone 2 Debug/Release async loading tests (measured activation maxima
  37,400 ns Debug and 800 ns Release in this run);
- Milestone 3 Debug/Release hashing/path/folder/ZIP/library/provider-provenance/
  invalid/worker/performance tests;
- Milestone 4 Debug and Release: 20/20 composite foundation tests in each
  configuration;
- Windows Credential Manager isolated store/load/replace/remove/disconnect;
  PKCE and OAuth terminal paths; strict URL/JSON/status policy; actual WinHTTP
  loopback GET/POST/redirect/limits/truncation/streaming/cancellation; NAM and
  generated WAV validation/promotion/provenance.

Final failures: none. An initial sandboxed MSBuild attempt was denied by MSVC
file tracking and was rerun with approved access. During development, expanded
tests exposed the missing provider credential-key initialization; it was fixed
and the full matrix rerun. A test-held Windows metadata file handle was also
closed before testing a second atomic commit.

Mock-only: all OAuth/token/API/download/provider tests use injected responses
or a loopback-only `127.0.0.1` server. No provider credential, user identity,
signed URL, or downloaded provider content was used.

Skipped/not claimed: live TONE3000 authorization/API/download, production
callback listener/custom-scheme behavior, provider sandbox, DNS/TLS failure
against a public server, process-crash refresh/download recovery, full
multi-process host matrix, macOS build/Keychain/HTTP/browser runtime, DAW host
discovery/editor lifecycle, pluginval, sanitizers/fuzzers, signing/notarization,
and redistributable released third-party A1/A2 fixture matrix.

## Live-provider validation

**Not run: no registered client configuration**

## Git

- Branch: `main`.
- Remotes: `origin` = `https://github.com/atoontoto/Amphibia.git`;
  `upstream` = `https://github.com/sdatkinson/NeuralAmpModelerPlugin.git`.
- Milestone 4 implementation commits through this report's preparation:
  `43a229f`, `81562f6`, `3cc6056`, `a235e8e`, `e8292a7`, `677b091`,
  `d1f2f45`, `2538b19`, `384da67`, `a9fa11d`.
- Submodules remain pinned: AudioDSPTools `0827c6c`, NAM Core `9c7b185`,
  iPlug2 `66f9060`, and recorded Eigen commits; no dirty submodule marker was
  reported.
- The final clean status and documentation-closing commit are reported in the
  handoff response because a commit cannot contain its own hash.

## Known limitations

- No publishable client ID, registered application/redirect, approved desktop
  callback pattern, brand assets, support contact, or final privacy/repository
  registration URLs are available. Production controls remain disabled.
- The `get_tone`/`load_tone` prompt contradiction is unresolved and that flow
  remains disabled.
- No production callback listener/custom-scheme registration exists; only the
  strict parser and loopback-only transport tests exist.
- macOS Keychain/browser source is unvalidated, and the `NSURLSession` adapter
  plus Xcode provider integration is not implemented.
- Live-provider, process-crash, full multiple-process/DAW lifecycle, public
  DNS/TLS, sanitizer/fuzz, and installer/release validation remain open.
- The minimal Settings diagnostics are not the final compliant provider UX.
- Existing inherited conversion/minizip warnings and broader inherited
  real-time allocation/fixture/release blockers remain documented.

## Milestone 5 handoff

Stable inputs for the next milestone:

- `AuthorizationRequest{SelectTone,...}` builds hosted `select_tone` requests
  for explicit Search TONE3000 / Find IR entry points after registration.
- `CompleteAuthorizationCallback()` returns the validated selected tone ID
  while token details remain inside the provider subsystem.
- `FetchTone(toneId)` returns provider-neutral tone, creator, license, format,
  gear, tags, imagery, and source metadata.
- `FetchModelsForTone(toneId)` and the exact-model API boundary expose raw
  string model IDs, format, A1/A2/unknown architecture, size, and transient
  download identity for an exact variant/IR chooser.
- `BeginModelDownload()` performs only a user-requested individual download,
  validation, deduplication, schema-2 provenance, and managed promotion.
- The returned managed SHA-256 resolves to a local path for explicit
  `_RequestModel()` or `_RequestIR()` submission; no provider code activates
  DSP directly.
- Schema-2 associations supply creator/license/source attribution after byte
  deduplication or disconnect. State 1.1 restores the managed hash offline;
  provider credentials and signed URLs never enter project state.

Milestone 5 must add the registered callback receiver, explicit hosted
selection controls, exact variant selection, approved branding/attribution,
and live development validation. It must not turn the foundation into native
unrestricted search, automatic download/activation, or background sync.
