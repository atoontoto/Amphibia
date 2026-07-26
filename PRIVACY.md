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

The hosted `Search TONE3000` and `Find IR on TONE3000` controls are explicit user actions. In the unconfigured repository build they are disabled and start no listener, credential access, browser, DNS, or HTTP work. Hosted discovery occurs on TONE3000's own site; Amphibia implements no unrestricted catalog search, scraping, recommendations, mirroring, remote image fetch, or background synchronization.

Within a callback-selected pack, the local filter examines only bounded in-memory model names and paths. The panel/task snapshot is transient and is not written to DAW state. Exact provider IDs and attribution enter the managed library only after an explicit successful individual download.
