# Amphibia

Amphibia is an independent, free, open-source desktop application and audio plug-in derived from the official open-source [Neural Amp Modeler plug-in](https://github.com/sdatkinson/NeuralAmpModelerPlugin). It is not the official NAM application and is not affiliated with TONE3000.

The repository has completed Milestones 0 through 3. Product identity and side-by-side identifiers are established, model and impulse-response preparation runs outside the audio thread, and Amphibia now includes an optional local managed library with secure folder and ZIP importing. No supported end-user installer exists yet. Online browsing, authentication, downloading, and TONE3000 integration are not implemented; those remain later milestones.

## Current scope

- Standalone application and VST3 product targets named Amphibia.
- Existing referenced `.nam` model and WAV impulse-response workflow inherited from upstream.
- Background model/IR inspection and preparation with bounded audio-boundary activation.
- Optional content-addressed managed library with streamed SHA-256 deduplication, transactional metadata, verification, cleanup, and recovery.
- Local individual-file, recursive-folder, multi-file drag/drop, and reviewed ZIP-pack importing.
- Reader-only ZIP handling with traversal, link, collision, encryption, compression-ratio, count, size, CRC, and disk-space defenses.
- Backward-compatible state 1.1 managed-object references while retaining Amphibia 1.0 and legacy NAM readers.
- New VST3, AU, Windows project, installer, bundle, and data identifiers.

Windows Release and Debug standalone/VST3 targets and the Milestone 2/3 regression suites pass. This is still not release-ready: inherited visual/font assets need provenance or replacement, macOS has not been built in this environment, and host discovery, interactive DAW behavior, side-by-side installation, signing, notarization, and installer behavior remain unverified.

## Build from source

Clone recursively, then follow [BUILDING.md](BUILDING.md). The short form is:

```text
git clone --recursive https://github.com/atoontoto/Amphibia.git
cd amphibia
```

Windows builds use `NeuralAmpModeler/Amphibia.sln`; the retained source directory and many internal filenames deliberately match upstream to reduce future merge conflicts. See [CONTRIBUTING.md](CONTRIBUTING.md) for validation commands.

## Project records

- [Architecture plan](docs/ARCHITECTURE_PLAN.md)
- [Milestone checklist](docs/MILESTONES.md)
- [Identifier registry](docs/IDENTIFIERS.md)
- [Milestone 1 report](docs/MILESTONE_1_REPORT.md)
- [Milestone 2 DSP loading map](docs/MILESTONE_2_DSP_LOADING_MAP.md)
- [Milestone 3 final report](docs/MILESTONE_3_REPORT.md)
- [Local library guide](docs/LOCAL_LIBRARY.md)
- [Secure ZIP import policy](docs/ZIP_IMPORT.md)
- [Attribution](ATTRIBUTION.md) and [third-party notices](THIRD_PARTY_NOTICES.md)
- [Privacy](PRIVACY.md) and [security](SECURITY.md)

## License

The inherited code and Amphibia modifications are provided under the repository [MIT license](LICENSE), subject to the separate licenses and notices of included dependencies. Do not assume that every inherited font or visual asset is cleared for redistribution; see [ATTRIBUTION.md](ATTRIBUTION.md).
