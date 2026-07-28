# Windows release dependency and license review

Status: Milestone 8 engineering review; not legal advice

Evidence ID: `RC-EVIDENCE-2026-07-28`

Review date: 2026-07-28

Candidate: **Amphibia 0.1.0 RC validation build**

This review reconciles the Windows standalone, VST3, installer, generated
CycloneDX SBOM, dependency pins, and notices. “Reviewed” means the repository
contains enough engineering evidence to apply the stated treatment. It does
not substitute for an owner or legal decision where one is identified.

## Results

| Component | Exact evidence | Terms/treatment | Result |
| --- | --- | --- | --- |
| NeuralAmpModelerPlugin | `0.7.15` / `96337e9ab6e3beb619459779bbb5c47e1b04d8c4`; root `LICENSE` | MIT notice retained; Amphibia modifications identified | Reviewed |
| NeuralAmpModelerCore | `0.5.3` / `9c7b185de346fe0725dea537bcee4bc38b5bb6d6`; adjacent `LICENSE` | MIT notice/pin retained | Reviewed |
| AudioDSPTools | `0.1.1` / `0827c6c2fc0deced568536142ea86f189e0b98a1`; adjacent `LICENSE` | MIT notice/pin retained | Reviewed |
| iPlug2 fork, WDL, NanoVG, NanoSVG | iPlug2 `66f9060b5287afdb2ddb83aab5a2a2a8e66a4d5e`; adjacent component licenses | zlib-style notices retained; exact aggregate pin recorded | Reviewed |
| Eigen | root `ed8cda3`, NAM Core `87300c93`, AudioDSPTools `6d829e76`; adjacent MPL/BSD/MINPACK/Apache files | MPL-2.0 and component licenses retained; test/benchmark material is not packaged | Reviewed |
| nlohmann/json | NAM Core header reports `3.12.0` | MIT; compiled header-only dependency recorded | Reviewed |
| minizip-ng / zlib | vendored `4.2.2` / `1.3.2`; adjacent licenses | Reader/inflate subset; zlib notices retained | Reviewed |
| VST 3 SDK | official tag `v3.8.0_build_66`, commit `9fad9770f2ae8542ab1a548a68c1ad1ac690abe0`; local SDK `LICENSE.txt` | VST 3 SDK 3.8 is MIT. No VST logo is packaged. Immutable CI pin replaces the former moving `master` checkout. | Reviewed |
| RtAudio / RtMidi | headers report `5.1.0` / `4.0.0`; bundled README license texts | Standalone-only; notices retained | Reviewed |
| Steinberg ASIO interface | `asio.h` identifies ASIO Interface Specification `2.3`, copyright 1997-2013; enabled by `__WINDOWS_ASIO__` | The exact legacy file set has no adjacent authoritative license agreement in the downloaded payload. Do not infer rights from the separately MIT-licensed VST 3 SDK. | **Blocked: owner/legal review or removal required** |
| Roboto Regular | embedded records: Google, `1.100141` (2013), Apache-2.0 | Unmodified font; hash and complete Apache-2.0 terms included | Reviewed |
| Michroma Regular | embedded records: Michroma Project Authors/Vernon Adams, `1.100`, OFL-1.1 | Unmodified font; hash and complete OFL-1.1 terms included | Reviewed |
| UI raster/SVG artwork | exact compiled file list and hashes generated from `main.rc`; upstream history recorded | Individual origin/redistribution authority remains incomplete | **Blocked: provenance/permission** |
| Inno Setup | compiler `6.7.3`; installed `LICENSE.TXT`; official revision history dated 2026-05-26 | License permits use for any purpose, including commercial applications, subject to notice/origin/modification conditions. Amphibia uses the unmodified compiler for a non-commercial project. Inno requests a commercial purchase; a future commercial scope requires an owner policy decision. | Reviewed for current non-commercial scope |
| MSVC / Windows SDK | MSBuild 18/v145, SDK `10.0.26100.0`; Release `/MT`; PE import evidence | No VC Redistributable package or private Microsoft DLL is staged. Static runtime use remains subject to the installed tool license; the repository owner must ensure the build environment is properly licensed. | Engineering treatment reviewed; owner license confirmation required |
| Windows system APIs | final PE import inventory | Windows 10+ system components only; no driver, service, or private system DLL is redistributed | Reviewed |

## Authoritative external material checked

- VST 3 SDK official repository/tag and MIT license:
  <https://github.com/steinbergmedia/vst3sdk/tree/v3.8.0_build_66>
- Inno Setup 6.7.3 revision history and license:
  <https://jrsoftware.org/files/is6-whatsnew.htm> and the installed
  `LICENSE.TXT`
- Microsoft Visual Studio 2026 distributable-code list:
  <https://learn.microsoft.com/visualstudio/releases/2026/redistribution>
- Microsoft guidance for determining VC runtime redistribution:
  <https://learn.microsoft.com/cpp/windows/determining-which-dlls-to-redistribute>
- Google Fonts repository licensing layout:
  <https://github.com/google/fonts>
- Michroma project:
  <https://github.com/googlefonts/Michroma-font>

## Decision

The VST 3, font, Inno Setup non-commercial, and no-dynamic-VC-runtime questions
are closed at the engineering level for this candidate. ASIO and inherited UI
artwork remain hard public-release blockers. The final checklist must also
retain owner/build-license confirmation; none of these conclusions authorizes
public distribution while the other Milestone 8 gates remain open.
