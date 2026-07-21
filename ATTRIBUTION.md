# Attribution and asset status

Amphibia is an independent derivative of the open-source [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin), which uses [NeuralAmpModelerCore](https://github.com/sdatkinson/neural-amp-modeler). Steven Atkinson and upstream contributors retain credit and copyright for their work. Amphibia does not claim to be the official Neural Amp Modeler application.

Milestone 1 retains upstream source/project/resource filenames where changing them would add merge risk without changing installed identity. Historical REAPER projects and the `###NeuralAmpModeler###` state header are retained strictly as compatibility fixtures.

## Asset quarantine

The inherited `.ico`/`.icns` application icons, UI raster/SVG artwork, Roboto font file, and Michroma font file lack a complete asset provenance manifest in this checkout. The inherited Windows icon has been removed from compiled resources and the macOS plist no longer selects the inherited `.icns`. All inherited visual/font files are development-only, must not be treated as Amphibia brand assets, and block public binary distribution until replaced or individually cleared with source/version/license evidence.

No TONE3000 logo or brand asset is included. Any future provider presentation must follow then-current provider terms and must accurately disclose the independent integration.
