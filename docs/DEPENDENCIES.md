# Dependency and license inventory

Status: Milestone 4 inventory and release gate; not legal advice

Release rule: no dependency or inherited asset ships without a recorded version, source, license text, notice treatment, and redistribution decision.

## Current source dependencies

| Component | Pin / source in checkout | License observed | Distribution treatment / issue |
|---|---|---|---|
| NeuralAmpModelerPlugin | `96337e9` / root | MIT (`LICENSE`) | Retain copyright/license; document substantial Amphibia modifications. |
| NeuralAmpModelerCore | submodule `9c7b185` (`v0.5.3`) | MIT | Retain license and pin in notices/SBOM. |
| AudioDSPTools | submodule `0827c6c` (`v0.1.1`) | MIT | Retain license and pin. Appears twice through Core; deduplicate notices, not source provenance. |
| iPlug2 fork | submodule `66f9060` | zlib-style (`iPlug2/LICENSE.txt`) | Retain notice; record that this is the pinned NAM fork, not current upstream iPlug2. |
| WDL / SWELL | within iPlug2 | zlib-style / component-specific | Inventory compiled subset; preserve component notices. |
| Eigen | three pinned snapshots | Primarily MPL-2.0, with BSD/MINPACK-licensed portions | Include required licenses/notices and source availability information. Confirm exact compiled modules and avoid copying benchmark/test-only code into binaries. |
| nlohmann/json | within NAM Core | MIT | Retain notice; used for `.nam` parsing. |
| NanoVG | within iPlug2 | zlib-style | Retain notice if compiled. |
| NanoSVG | within iPlug2 | zlib-style | Retain notice if compiled. |
| MetalNanoVG | within iPlug2 | MIT | macOS compiled-subset audit required. |
| Yoga | within iPlug2 | MIT | Include only if the product build links it. |
| RtAudio | within iPlug2 standalone | MIT-like | Retain notice; verify enabled Windows/macOS backends. |
| RtMidi | within iPlug2 standalone | MIT-like | Retain notice. |
| VST3 SDK | downloaded/present through iPlug tooling | MIT in current upstream notice | Pin exact SDK commit/archive and checksum in CI; ship required notice. |
| Steinberg ASIO SDK interfaces | iPlug standalone dependency path | Steinberg ASIO SDK license | **Release review required.** Verify the exact files/binaries redistributed and compliance with the SDK's separate terms. |
| REAPER SDK | within iPlug2 | license file present | Exclude if unused; otherwise review compiled/distributed surface. |
| HIIR | within iPlug2 extras | license file present | Determine whether linked, then include/exclude from notices accordingly. |
| libpng / giflib | within WDL | component licenses present | Determine whether linked; retain required notices if shipped. |
| stb headers | within iPlug2 | public-domain/MIT alternatives as identified per header | Inventory exact headers compiled and record chosen license option. |
| Khronos platform headers | within graphics dependencies | MIT-like | Retain notice if distributed/compiled. |

The existing `NeuralAmpModeler/installer/ThirdPartyNotices.txt` is a useful starting point but is not an Amphibia release manifest. It does not establish the provenance/permission for all fonts and artwork and must be regenerated from the actual compiled and packaged dependency graph.

## Inherited assets

| Asset group | Checkout evidence | Decision |
|---|---|---|
| `Roboto-Regular.ttf` | Present under `NeuralAmpModeler/resources/fonts`; no adjacent license file found. | Do not ship until the exact font version and Apache-2.0/OFL provenance (as applicable to that file) is proven and license text added; replacing it is acceptable. |
| `Michroma-Regular.ttf` | Present under the same folder; no adjacent license file found. | Do not infer a license from the font name. Prove source/version and include its license, or replace. |
| NAM icons/backgrounds/knob/meter images | Added in upstream history, but no asset license/provenance manifest was found. | Replace with original Amphibia assets or obtain explicit provenance/permission before release. Source-code MIT alone is not treated as sufficient evidence for third-party visual assets. |
| NAM application icons (`.ico`, `.icns`, xcassets) | NAM-branded and collision-prone. | Quarantined from Amphibia's selected Windows/macOS icon metadata in Milestone 1; replace before release. |
| TONE3000 brand/logo | Not vendored. Provider guidelines require brand presentation. | Obtain/use only the official asset under documented brand permission; preserve aspect/color/clearspace; otherwise use provider-approved text treatment while seeking clarification. |
| Model/IR fixtures | NAM Core contains example `.nam` files. | Record each fixture's source and redistribution permission. Real provider downloads never enter source control or release archives. |

## Proposed new dependencies

No new library is approved in Milestone 1.

| Capability | Current decision | Evaluation criteria |
|---|---|---|
| HTTP/TLS | Windows WinHTTP selected and implemented; macOS `NSURLSession` target deferred | Native system trust/proxy integration, explicit redirect control, cancellation, streaming limits, and no vendored TLS. The non-Windows source currently fails closed; macOS transport and build integration remain required before macOS provider support is claimed. |
| Loopback HTTP | Prefer a minimal Amphibia-owned single-request parser or a small audited component | Bind-address control, header/request caps, deadline/cancel, no general server surface, license. |
| ZIP | **Selected for Milestone 3 local import:** minizip-ng 4.2.2 plus zlib 1.3.2, vendored exact release source | Reader-only stored/deflate and ZIP64 support. Encryption/crypto, writers, OpenSSL, bzip2, LZMA, zstd, and iconv disabled. Amphibia owns cancellation, path/link checks, declared/actual limits, randomized safe writes, CRC handling, and staging transactions. |
| Hashing / CSPRNG | Prefer audited OS crypto APIs or an already linked maintained library | SHA-256 correctness, cryptographic randomness, FIPS concerns if claimed (none planned), no custom crypto. |
| Persistent index | Start with schema-versioned JSON and atomic replace | Measure startup/write scale first. SQLite is allowed only if justified and then its public-domain status/build provenance must be recorded. |
| Credential storage | Platform APIs | Windows Credential Manager and macOS Keychain; no plaintext fallback. |

## Milestone 4 platform APIs

| API | Version/source | License | Purpose and remaining risk |
| --- | --- | --- | --- |
| WinHTTP / Schannel | Supported Windows runtime; verified with Windows SDK 10.0.26100.0 | Microsoft SDK/runtime terms | HTTPS GET/POST, streamed downloads, system trust/proxy, timeout and native-handle cancellation. Redirects are disabled natively and reevaluated by Amphibia. DNS/TLS cancellation can take until a finite timeout. |
| Windows Credential Manager | Supported Windows runtime (`advapi32`) | Microsoft SDK/runtime terms | Per-user access/refresh token storage and atomic replacement; no plaintext fallback. Local processes running as the same user remain inside the OS trust boundary. |
| Windows CNG | Supported Windows runtime (`bcrypt`) | Microsoft SDK/runtime terms | PKCE/state randomness, SHA-256, and retry jitter. |
| Security.framework Keychain | Supported macOS SDK/runtime | Apple SDK/runtime terms | Source implementation for per-user credential storage and replacement. macOS compile/runtime tests were not available. |
| AppKit/Foundation browser APIs | Supported macOS SDK/runtime | Apple SDK/runtime terms | Explicit default-browser launch. macOS validation was not available. |

No new third-party library was added for Milestone 4. `libcurl` was evaluated and deferred because it would add a pinned binary/TLS-backend matrix; the inherited WDL/WinINet facilities were rejected as the provider policy boundary. Details are in `HTTP_TRANSPORT.md`.

Milestone 5 adds no third-party library or packaged provider asset. The hosted-selection lease uses a Windows named semaphore or macOS `flock`; the UI uses existing iPlug2 controls. Official TONE3000 brand assets were not scraped or copied and remain a production registration blocker.

## Milestone 3 pinned local-import dependencies

| Dependency | Pin | License | Build and risk record |
| --- | --- | --- | --- |
| minizip-ng | release `4.2.2` (2026-06-30) | zlib | Maintained cross-platform C reader with ZIP64, Unicode paths, streaming and CRC. Vendored release source makes builds reproducible. Reader calls are confined to one import task; no shared archive handle. The library does not supply cancellation or Amphibia's security policy, so all reads are bounded and cancellation-checked by the caller. Symlink/special metadata is rejected before extraction. |
| zlib | release `1.3.2` | zlib | Vendored inflater/CRC subset used by minizip-ng. No runtime package lookup. Only inflate-related source enters app/VST3 builds. |
| nlohmann/json | existing pinned checkout | MIT | Reused for schema-1 snapshots and bounded NAM structure checks. Atomic snapshot replacement avoids a new SQLite dependency at current scale. |
| Windows CNG / macOS CommonCrypto | platform API | OS SDK terms | Streaming SHA-256 and OS CSPRNG; no custom cryptography or added crypto binary. |

The dependency evaluation considered minizip-ng maintenance/release cadence, zlib-compatible license, Windows/macOS source support, ZIP64 and filename handling, external-stream design, special-entry metadata exposure, static build size, lack of hidden runtime downloads, and reproducible vendoring. libarchive offered broader format coverage but a substantially larger unused parser surface. A handwritten decoder was rejected. Remaining risks are malformed-archive bugs in native parsers and platform filename edge cases; generated adversarial tests, conservative feature disabling, and outer policy limits reduce but do not eliminate them.

## API and service terms (not software licenses)

| Service | Governing material | Product implication |
|---|---|---|
| TONE3000 API | <https://www.tone3000.com/api> and <https://www.tone3000.com/api/terms> (reviewed 2026-07-26; terms effective 2026-07-07) | Free native integration is conditioned on open-source/non-commercial use and brand/attribution/access rules; terms are revocable and must be rechecked before release. No scraping, bulk synchronization, catalog mirroring, or model redistribution. The `get_tone`/`load_tone` discrepancy blocks that prompt until clarified. |
| NAM project name/branding | Upstream project and repository | Amphibia must accurately attribute its derivative basis without presenting itself as the official NAM plugin or reusing product identity. |

## Release checklist for notices

- Generate a machine-readable dependency lock/SBOM from the exact release checkout.
- Match every linked/static/object dependency to a license and source pin.
- Match every packaged font, image, document, example, and provider logo to provenance and permission.
- Include root license, third-party notices, source offer/link obligations, and creator/asset license metadata where applicable.
- Review ASIO and platform SDK redistribution with a qualified maintainer/legal reviewer.
- Diff installer/package contents against the manifest; fail CI on an unclassified file.
