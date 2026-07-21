# Milestone 0 report

Date: 2026-07-21

Scope: investigation and architecture only

## Outcome

Milestone 0 establishes a traceable upstream baseline and a concrete design for Amphibia. No product source, product identity, build project, installer, provider client, or DSP behavior was changed.

The investigation found that upstream `v0.7.15`/Core `v0.5.3` already contains released A2 support, including the native A2 fast-path build flag and Core tests. It also found that model/IR preparation is synchronous and replacement can destroy the old DSP object in `ProcessBlock()`. The Amphibia plan therefore includes async preparation and non-real-time reclamation as explicit Milestone 2 work.

## Files added

- `ARCHITECTURE_PLAN.md`: system boundaries, decisions, proposed source tree, threads, state, cache, and validation gates.
- `UPSTREAM_MODIFICATION_MAP.md`: pinned provenance and exact code/build/installer hotspots.
- `THREAT_MODEL.md`: assets, trust boundaries, invariants, threats, OAuth, download/archive, and incident behavior.
- `DEPENDENCIES.md`: preliminary source/service/asset license inventory and new-dependency gates.
- `TONE3000_INTEGRATION.md`: exact documented integration surface and unresolved limitations.
- `NAM_ARCHITECTURES.md`: A1/A2 detection, loading, labeling, and fixture plan.
- `IDENTIFIERS.md`: exact identity classes that must change in Milestone 1.
- `MILESTONES.md`: strict Milestones 0–11 acceptance checklist.
- `GIT_PLAN.md`: remotes, branches, commits, GitHub security, sync, and release policy.
- `UPSTREAM_SYNC.md`: durable upstream merge boundaries and procedure.

## Decisions

- Preserve local selection and non-recursive previous/next semantics through a Local provider.
- Use a provider-neutral API; isolate TONE3000 wire/OAuth/brand behavior in its adapter.
- Use hosted `select_tone` for discovery; do not implement unrestricted native search under the currently documented free tier.
- Resolve and download one exact selected model/IR variant, never a whole-tone archive for the initial integration.
- Use one NAM Core processor path for both A1 and A2, with strict local A2 shape inspection.
- Keep active DSP on every load/network/auth failure; fully prepare replacements off-thread and reclaim retired objects off-thread.
- Add an Amphibia state schema but retain a tested reader for existing NAM state.
- Keep standalone device settings and plugin session state separate.
- Defer new product identifiers to Milestone 1 because the repository owner/reverse-DNS authority is not known.
- Add no new library until its license, maintenance, security, cancellation, and reproducibility properties are evaluated.

## Commands and checks

Representative investigation/validation commands:

```powershell
git clone --origin upstream --recurse-submodules https://github.com/sdatkinson/NeuralAmpModelerPlugin.git .
git log --oneline --decorate --all
git tag --sort=version:refname
git submodule status --recursive
rg -n "_StageModel|_StageIR|_ApplyDSPStaging|SerializeState|UnserializeState" NeuralAmpModeler
rg -n "IDirBrowseControlBase|LoadFileAtCurrentIndex|SetupMenu" .
rg -n "NAM_ENABLE_A2_FAST|is_a2_shape|wavenet_a2" NeuralAmpModeler NeuralAmpModelerCore
rg -n "PLUG_NAME|PLUG_UNIQUE_ID|PLUG_MFR_ID|BUNDLE_NAME" NeuralAmpModeler iPlug2
git diff --check
git submodule status --recursive
```

Official release, format, framework, and provider documentation was reviewed at the URLs recorded in the relevant documents.

## Builds and tests

- Documentation whitespace validation: passed (`git diff --check`).
- Required-document presence/content validation: passed.
- Recursive submodule state: all pins initialized with no dirty marker.
- Product build: not run and not claimed. CMake, MSBuild, `cl`, and Visual Studio were not available on `PATH` in this environment, and Milestone 0 intentionally changes no product code.
- Runtime/audio/plugin tests: not run; they begin with the renamed/characterization milestones.

## Limitations and blockers carried forward

- No Amphibia GitHub repository/owner namespace, so no `origin`, reverse-DNS bundle IDs, or public tag was invented.
- No TONE3000 publishable client registration or exact desktop redirect URI is supplied.
- Production random-loopback-port support and a combined A1+A2 hosted selector are not documented by TONE3000.
- The provider's `load_tone` API wording conflicts with `get_tone` in terms/free-tier prose; initial scope avoids that prompt.
- HTTP/TLS and optional ZIP dependencies are not selected.
- Hosted IR loading requires bounded WAV preflight/parser hardening.
- Inherited art/font provenance and ASIO redistribution require review before release.
- Signing/notarization, installers, CI validation, and full A1/A2 integration tests are future milestones.

## Repository handoff

The final assistant handoff records the documentation commit hash and post-commit `git status`. The recursively pinned upstream hashes are recorded in `UPSTREAM_MODIFICATION_MAP.md` and `UPSTREAM_SYNC.md`.
