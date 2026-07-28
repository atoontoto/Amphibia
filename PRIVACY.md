# Privacy

TONE3000 access is optional and inactive unless the user explicitly starts an
account action. Amphibia remains a fully local NAM/IR player when provider
registration is absent, the account is disconnected, or the service is
offline. No browser opens automatically during construction, plugin scan,
project restoration, or audio processing.

Local NAM/IR files and guitar/audio samples are never uploaded. Amphibia sends
only the fields needed for an explicit OAuth authorization, requested current
account/tone/model metadata, and an explicitly requested individual model
download. It adds no analytics, telemetry, advertising identifier, background
catalog synchronization, scraping, or whole-catalog cache.

OAuth access and refresh tokens are stored only in Windows Credential Manager
or macOS Keychain under Amphibia's isolated TONE3000 namespace. There is no
plaintext fallback. Tokens, authorization codes, PKCE material, complete
signed URLs, and credential payloads are excluded from settings, DAW state,
library metadata, diagnostics, and filenames. The user can disconnect; that
removes stored credentials and cached account identity.

An explicitly downloaded model remains local in Amphibia's managed library
until the user separately removes it. Its provider tone/model IDs, creator
identity, provider license fields, source page, format, and local SHA-256 are
retained for attribution and restoration even after disconnect or byte
deduplication. Amphibia does not redistribute those bytes or infer a more
permissive license. TONE3000 and the operating-system credential/network
services have their own privacy terms.

Local managed imports continue to store content-addressed copies, normalized
pack associations, display/file properties, timestamps, and optional manual
classifications under the platform application-data directory. Referenced
sources remain read-only. Staging cleanup is confined to Amphibia's verified
managed staging root. No persistent provider request/body log is emitted.

The hosted `Search TONE3000` and `Find IR on TONE3000` controls are explicit
user actions. The development public-client build enables them, but merely
opening Amphibia or Settings starts no listener, credential access, browser,
DNS, or HTTP work. Hosted discovery occurs on TONE3000's own site; Amphibia
implements no unrestricted catalog search, scraping, recommendations,
mirroring, remote image fetch, or background synchronization.

Within a callback-selected pack, the local filter examines only bounded in-memory model names and paths. The panel/task snapshot is transient and is not written to DAW state. Exact provider IDs and attribution enter the managed library only after an explicit successful individual download.

## Setup and configuration

Audio-device display names and NAM/IR folder paths may be stored locally in
Amphibia's versioned application settings. They are not transmitted. Setup
adds no analytics, telemetry, advertising identifier, or background service.
The output-test samples are generated transiently and are neither stored nor
uploaded. Input audio is not recorded; the UI receives only a lock-free peak
measurement.

Entering the optional TONE3000 setup page does not open a browser, read a
credential, connect, or send a request. Managed-library verification and
staging cleanup remain local. Copied diagnostics are generated locally and
must remain bounded/redacted; users should review device names and local paths
before sharing them. No token, authorization code, PKCE value, credential
payload, or signed URL is permitted in setup settings or diagnostics.

## Install and uninstall

The Windows installer contains no model, IR, account, credential, settings, or
telemetry payload and installs no background service. Upgrade and normal
uninstall preserve `%LOCALAPPDATA%\Amphibia` and the isolated Credential Manager
entry. Interactive uninstall offers a separate default-No cleanup choice. If
confirmed, it removes only that fixed Amphibia tree and the exact
`org.amphibiaaudio.Amphibia.TONE3000/oauth-v1` credential; it does not traverse
reparse points or delete any referenced external NAM/IR folder or original ZIP.
Silent uninstall always preserves user data.

## Release-evidence handling

Milestone 8 release evidence contains hashes, dependency/asset identifiers,
tool versions, import lists, and redacted test results. It must not contain
usernames, home/workspace paths, device serials, credentials, provider user
records, private model/IR content, or complete signed URLs. The generated
scanner checks tracked source, artifacts, and archive members for representative
secret and private-path patterns, but its documented limitations remain:
pattern scanning cannot prove every secret format absent and does not decode
compressed installer internals.

No repository-derived dependency metadata was submitted to an external
vulnerability service during the local Milestone 8 run. The resulting
vulnerability gate is reported as blocked rather than weakening this privacy
boundary or claiming a successful scan.

The publishable `t3k_pub_…` client identifier is application configuration,
not user data and not a credential. No client secret is used. The fixed
callback listens only while an explicit authorization is active, only at
`http://127.0.0.1:43821/oauth/callback`, and returns a minimal no-store local
response with no code, state, token, verifier, account identity, or model URL.
The post-patch source/package scan found no private credential or path finding.
