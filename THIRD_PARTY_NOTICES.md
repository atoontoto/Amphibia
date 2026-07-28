# Third-party notices

Amphibia is derived from NeuralAmpModelerPlugin by Steven Atkinson and contributors, pinned at upstream version `0.7.15` / commit `96337e9ab6e3beb619459779bbb5c47e1b04d8c4`. The upstream project is MIT-licensed; its copyright and license remain in [LICENSE](LICENSE).

The source tree includes pinned dependencies including NeuralAmpModelerCore, AudioDSPTools, iPlug2, Eigen, nlohmann/json, NanoVG/NanoSVG, RtAudio, RtMidi, and platform/SDK components. Their authoritative license files remain beside their source in the repository and must accompany any distribution as required.

Milestone 3 vendors the reader subset of **minizip-ng 4.2.2** (zlib license) from <https://github.com/zlib-ng/minizip-ng/releases/tag/4.2.2> and **zlib 1.3.2** (zlib license) from <https://zlib.net/> under `ThirdParty/`. Their license files are retained beside the source. Amphibia disables archive writing/compression, encryption/crypto, OpenSSL, bzip2, LZMA, zstd, and iconv integrations; only stored/deflated local ZIP reading is compiled. Windows SHA-256 uses the operating-system CNG API and macOS uses CommonCrypto.

Milestone 4 adds no vendored networking, TLS, OAuth, or credential library. Windows builds use the operating-system WinHTTP, Schannel trust store, Windows CNG, Credential Manager, Winsock, and ShellExecute APIs under the applicable Microsoft SDK/runtime terms. The macOS credential implementation uses Security.framework Keychain APIs and the system browser uses AppKit/Foundation APIs; the planned `NSURLSession` HTTP adapter is not yet implemented or validated. These operating-system components are not redistributed as Amphibia source. TONE3000 remains an external service governed by its own API terms and content licenses; no provider model, user data, signed URL, or brand asset is included in this repository.

Windows installer generation uses Inno Setup 6.7.3 under the Inno Setup license.
The compiler is a build tool and is not redistributed, while the generated
installer includes Inno Setup runtime code. Commercial distribution must
complete the applicable Inno Setup licensing review. MSVC/Windows SDK tools are
also build-time inputs; Amphibia Release binaries use the static MSVC runtime.

This file is the human-readable notice copied into Milestone 7 Windows
artifacts. The generated `DEPENDENCY-VERSIONS.txt` records exact checkout and
toolchain versions; the detailed license/redistribution review is maintained in
[docs/DEPENDENCIES.md](docs/DEPENDENCIES.md). The older
`NeuralAmpModeler/installer/ThirdPartyNotices.txt` is inherited and is not a
Milestone 7 release input.

No Amphibia binary should be publicly distributed until the exact packaged file
inventory has been matched to licenses and notices, including the ASIO/VST3 SDK
review and the asset blockers described in [ATTRIBUTION.md](ATTRIBUTION.md).
