# Resource provenance status

This directory is inherited from NeuralAmpModelerPlugin `0.7.15` (`96337e9`). It is not an Amphibia release asset library.

| Group | Milestone 8 evidence |
|---|---|
| `NeuralAmpModeler.ico`, `NeuralAmpModeler.icns` | Quarantined; not selected by Amphibia's Windows resource or macOS plist. Replace before release. |
| `img/*` raster/SVG UI files | Compiled-file paths and SHA-256 values are generated into `ASSET-PROVENANCE.json` and the CycloneDX SBOM. Git history attributes the design/art changes to upstream contributors, but individual origin and redistribution authority remain incomplete. Public packaging is blocked. |
| `fonts/Roboto-Regular.ttf` | Embedded name records identify Google, version `1.100141` (2013), and Apache-2.0. SHA-256 and the full Apache-2.0 license-file hash are generated into release evidence. Cleared for unmodified embedding with notice. |
| `fonts/Michroma-Regular.ttf` | Embedded name records identify version `1.100`, the Michroma Project Authors/Vernon Adams, <https://github.com/googlefonts/Michroma-font>, and OFL-1.1. SHA-256 and the full OFL-1.1 license-file hash are generated into release evidence. Cleared for unmodified embedding with notice. |
| plist/XIB/storyboard/RC files | Build metadata, renamed internally to Amphibia while filenames are retained for upstream mergeability. |

When adding a replacement, record creator/source, exact version/hash, license or written permission, modifications, and the packages in which it ships. Update `ATTRIBUTION.md`, `THIRD_PARTY_NOTICES.md`, and `docs/DEPENDENCIES.md` in the same change.

Milestone 7's Windows packaging allowlist excludes loose inherited icons,
installer background art, and build-tree resource debris. Release binaries
embed the UI resources explicitly listed by `main.rc`. Milestone 8 generates a
machine-readable record from that exact list and fails if an unrecorded
compiled resource appears. Font evidence is now resolved, but the UI artwork
remains blocked; successful package generation is not provenance clearance and
does not authorize public release.
