# Attribution and asset status

Amphibia is an independent derivative of the open-source [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin), which uses [NeuralAmpModelerCore](https://github.com/sdatkinson/neural-amp-modeler). Steven Atkinson and upstream contributors retain credit and copyright for their work. Amphibia does not claim to be the official Neural Amp Modeler application.

Milestone 1 retains upstream source/project/resource filenames where changing them would add merge risk without changing installed identity. Historical REAPER projects and the `###NeuralAmpModeler###` state header are retained strictly as compatibility fixtures.

## Asset and font status

The inherited `.ico`/`.icns` application icons remain quarantined. The
inherited Windows icon is absent from compiled resources and the macOS plist
does not select the inherited `.icns`.

The exact compiled UI raster/SVG list and hashes are generated into
`ASSET-PROVENANCE.json` and the CycloneDX SBOM. Upstream Git history identifies
the contributing changes, but individual art origin and redistribution
authority are still incomplete. These files are not Amphibia brand assets and
continue to block public binary distribution.

The embedded font metadata clears the two unmodified fonts: Roboto Regular
`1.100141` (2013), copyright Google, is Apache-2.0; Michroma Regular `1.100`,
copyright the Michroma Project Authors, is OFL-1.1. The complete license texts
and exact font hashes accompany the release evidence and archives.

No TONE3000 logo or brand asset is included. Any future provider presentation must follow then-current provider terms and must accurately disclose the independent integration.

Milestone 7 reconstructs the Windows VST3 package and excludes inherited shell
icons, installer background art, and other loose build-tree resources from its
allowlist. Milestone 8 clears the font evidence but not the UI artwork. The
public-binary blocker remains until every embedded visual asset is replaced or
proven redistributable.
