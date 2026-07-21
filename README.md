# Amphibia

Amphibia is an independent, free, open-source desktop application and audio plug-in derived from the official open-source [Neural Amp Modeler plug-in](https://github.com/sdatkinson/NeuralAmpModelerPlugin). It is not the official NAM application and is not affiliated with TONE3000.

The repository is currently at the Milestone 1 development baseline. Product identity and side-by-side identifiers are established, but no supported end-user installer exists yet. Online browsing, authentication, downloading, and TONE3000 integration are not implemented.

## Current scope

- Standalone application and VST3 product targets named Amphibia.
- Existing local `.nam` model and WAV impulse-response workflow inherited from upstream.
- New VST3, AU, Windows project, installer, bundle, and data identifiers.
- Legacy NeuralAmpModeler state header accepted for future compatibility testing.

This is not release-ready: inherited visual/font assets need provenance or replacement, macOS has not been built in this environment, and host discovery, launch, side-by-side installation, signing, notarization, and installer behavior remain unverified.

## Build from source

Clone recursively, then follow [BUILDING.md](BUILDING.md). The short form is:

```text
git clone --recursive <your-amphibia-repository-url>
cd amphibia
```

Windows builds use `NeuralAmpModeler/Amphibia.sln`; the retained source directory and many internal filenames deliberately match upstream to reduce future merge conflicts. See [CONTRIBUTING.md](CONTRIBUTING.md) for validation commands.

## Project records

- [Architecture plan](docs/ARCHITECTURE_PLAN.md)
- [Milestone checklist](docs/MILESTONES.md)
- [Identifier registry](docs/IDENTIFIERS.md)
- [Milestone 1 report](docs/MILESTONE_1_REPORT.md)
- [Attribution](ATTRIBUTION.md) and [third-party notices](THIRD_PARTY_NOTICES.md)
- [Privacy](PRIVACY.md) and [security](SECURITY.md)

## License

The inherited code and Amphibia modifications are provided under the repository [MIT license](LICENSE), subject to the separate licenses and notices of included dependencies. Do not assume that every inherited font or visual asset is cleared for redistribution; see [ATTRIBUTION.md](ATTRIBUTION.md).
