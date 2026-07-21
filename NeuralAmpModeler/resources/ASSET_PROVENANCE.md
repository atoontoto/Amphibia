# Resource provenance status

This directory is inherited from NeuralAmpModelerPlugin `0.7.15` (`96337e9`). It is not an Amphibia release asset library.

| Group | Milestone 1 status |
|---|---|
| `NeuralAmpModeler.ico`, `NeuralAmpModeler.icns` | Quarantined; not selected by Amphibia's Windows resource or macOS plist. Replace before release. |
| `img/*` raster/SVG UI files | Used only for developer baseline builds; provenance is incomplete and public packaging is blocked. |
| `fonts/Roboto-Regular.ttf`, `fonts/Michroma-Regular.ttf` | Used only for developer baseline builds; exact source/version/license evidence is missing. Replace or clear before release. |
| plist/XIB/storyboard/RC files | Build metadata, renamed internally to Amphibia while filenames are retained for upstream mergeability. |

When adding a replacement, record creator/source, exact version/hash, license or written permission, modifications, and the packages in which it ships. Update `ATTRIBUTION.md`, `THIRD_PARTY_NOTICES.md`, and `docs/DEPENDENCIES.md` in the same change.
