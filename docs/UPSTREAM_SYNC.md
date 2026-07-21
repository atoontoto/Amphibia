# Upstream synchronization policy

Amphibia is a derivative of NeuralAmpModelerPlugin and should remain able to consume upstream correctness, architecture, and platform fixes without surrendering its independent identity or provider/security boundaries.

## Recorded baseline

- NeuralAmpModelerPlugin `v0.7.15`: `96337e9ab6e3beb619459779bbb5c47e1b04d8c4`
- NeuralAmpModelerCore `v0.5.3`: `9c7b185de346fe0725dea537bcee4bc38b5bb6d6`
- AudioDSPTools `v0.1.1`: `0827c6c2fc0deced568536142ea86f189e0b98a1`
- iPlug2 fork: `66f9060b5287afdb2ddb83aab5a2a2a8e66a4d5e`

## What remains close to upstream

- DSP parameter definitions/order and signal-flow behavior unless a documented Amphibia change is tested.
- NAM Core registry/model construction and resampling wrapper behavior.
- Core and AudioDSPTools as pinned submodules rather than copied source.
- iPlug platform glue where an adapter can avoid a fork.
- Legacy NAM state parsing, isolated behind an Amphibia reader.

## Amphibia-owned seams

- Product identity, projects, packages, installers, signing, and application data.
- Async model/IR preparation and real-time handoff/reclamation.
- Provider-neutral catalog/library/download APIs and TONE3000 adapter.
- OAuth, credentials, network/archive policy, managed cache, and attribution.
- Amphibia state schema, standalone session persistence, and expanded UI.

Milestone 1 deliberately retains the `NeuralAmpModeler/` directory, primary C++ filenames, platform project filenames, plist/interface filenames, and upstream submodule layout. Product identity lives in central metadata plus platform output/bundle settings. This keeps future merges focused on semantic conflicts instead of wholesale path renames.

The highest-conflict Milestone 1 files are `config.h`, `NeuralAmpModeler.cpp/.h`, `NeuralAmpModelerControls.h`, `Unserialization.cpp`, platform project/config files, `main.rc`, Apple plists/schemes, and distribution scripts. After every upstream merge, run `python tests/identity/validate_identity.py` before native builds and inspect emitted binary/bundle metadata for reintroduced NAM IDs.

## Sync review

For every upstream release:

1. Record old/new commits and recursive submodule diff.
2. Classify changes by DSP, model format/A1/A2, state, UI/navigation, framework/platform, packaging, license/assets, and security.
3. Pay special attention to the symbols listed in `UPSTREAM_MODIFICATION_MAP.md`.
4. Merge on a dedicated branch. Avoid carrying product-specific edits in submodules; if unavoidable, maintain a documented fork/patch series.
5. Re-run characterization before accepting changed behavior. Update compatibility/state migrations and docs as needed.
6. Verify that no upstream product identifiers or branded assets re-enter Amphibia outputs.
7. Review provider/network changes independently; an upstream merge cannot weaken Amphibia security invariants.
8. Land only with platform build/test and license evidence.

## Conflict priorities

When an upstream change conflicts with Amphibia:

1. Preserve audio correctness and host/session compatibility.
2. Preserve real-time safety and security invariants.
3. Preserve Amphibia product identity and side-by-side installation.
4. Prefer upstream DSP/platform fixes behind Amphibia-owned adapters.
5. Document intentional divergence and the upstream issue/commit that caused it.

## Contribution back

Generic fixes in NAM Core, AudioDSPTools, iPlug behavior, parsers, or tests should be proposed upstream when separable. Provider-specific features and Amphibia branding/state remain downstream. Upstream contributions must not include TONE3000 credentials, downloaded content, or Amphibia release secrets.
