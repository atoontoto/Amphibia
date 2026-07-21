# Contributing to Amphibia

Amphibia is an independent derivative of NeuralAmpModelerPlugin. Keep changes focused, preserve upstream attribution, and do not present the project as an official NAM or TONE3000 product.

## Set up

Clone recursively and verify the pins:

```text
git clone --recursive <your-amphibia-repository-url>
cd amphibia
git submodule status --recursive
python tests/identity/validate_identity.py
```

Follow [BUILDING.md](BUILDING.md) for native prerequisites and commands. Do not run packaging or release workflows merely to validate a source change.

## Change rules

- Do not modify DSP order, defaults, resampling, navigation, or state layouts without characterization tests.
- Do not commit credentials, downloaded models/IRs, generated packages, build outputs, or proprietary SDK material.
- Keep TONE3000-specific types out of DSP and provider-neutral domain code.
- Record every new dependency and packaged asset in `docs/DEPENDENCIES.md` and the applicable notice file.
- Retain technical upstream filenames when a rename adds merge cost without changing product identity; document exceptions in `docs/UPSTREAM_MODIFICATION_MAP.md`.
- Treat state and plug-in identifiers as compatibility contracts. Never regenerate them casually.

## Before a pull request

Run the identity validator and the native build available on your platform. In the pull request, include exact commands/results, affected formats, state compatibility impact, real-time safety impact, asset/license changes, and a focused test plan. Never include secrets or user-downloaded content in logs or fixtures.

By contributing, you agree that your contribution may be distributed under this repository's MIT license.
