# Git and GitHub plan

Status: Milestone 0; no Amphibia GitHub repository has been supplied

## Current repository state

- `main` is at upstream tag `v0.7.15`, commit `96337e9ab6e3beb619459779bbb5c47e1b04d8c4`.
- `upstream` points to `https://github.com/sdatkinson/NeuralAmpModelerPlugin.git` for fetch and push configuration, though no upstream push is intended.
- No `origin` exists. This is correct until the user supplies or creates a real Amphibia repository.
- Submodules are recursively initialized at the pins recorded in `UPSTREAM_MODIFICATION_MAP.md`.
- The upstream history and tags are preserved.

## Remote model

When the actual repository exists:

```text
origin    <owner-controlled Amphibia repository>   # fetch/push
upstream  https://github.com/sdatkinson/NeuralAmpModelerPlugin.git  # fetch only by policy
```

Do not rename `upstream` to `origin`, squash the inherited history, or force-push shared branches. Configure upstream push protection (`remote.upstream.pushurl` to a non-push sentinel or server permissions) when practical.

## Branches and commits

- `main`: protected, releasable history; merge only green reviewed changes.
- `integration/<milestone>`: optional short-lived milestone integration branch.
- `feat/<scope>`, `fix/<scope>`, `docs/<scope>`: small reviewable branches.
- Automated dependency/submodule update PRs: never direct pushes to `main`.

Commit principles:

- One coherent concern per commit; builds/tests should pass at meaningful boundaries.
- Preserve authorship and upstream commit topology.
- Never commit access/refresh tokens, client secrets, signing credentials, presigned URLs, downloaded user content, build output, caches, or platform credential exports.
- Submodule pointer changes must name old/new commit, upstream release, compatibility notes, and test evidence.
- Generated product IDs are committed once and then treated as immutable compatibility data.
- Large/binary test fixtures require license/source/hash review before entry; Git LFS is not assumed.

Proposed initial commits:

1. `docs: record Amphibia architecture and upstream investigation` (Milestone 0).
2. `refactor: establish independent Amphibia product identity` (reviewable subcommits by build platform are acceptable).
3. `test: characterize inherited NAM behavior`.
4. Subsequent commits follow component/milestone boundaries, with tests in the same commit as behavior where practical.

## Upstream sync procedure

1. Fetch `upstream` and inspect release notes, commit range, submodule changes, security notices, and build-tool changes.
2. Create `sync/nam-vX.Y.Z` from current `main`.
3. Merge upstream; do not rebase published Amphibia history. Resolve conflicts by preserving Amphibia identity/provider boundaries and reevaluating upstream behavior.
4. Update the modification map if load/state/UI/build hotspots change.
5. Run characterization, DSP fixture, realtime, state, provider, app, VST3, installer, and side-by-side test matrices.
6. Review third-party notices and submodule pins.
7. Land through a reviewed PR that records the upstream range and verification artifacts.

`docs/UPSTREAM_SYNC.md` will become the operational version once a CI/build baseline exists; this plan is the Milestone 0 policy.

## GitHub configuration

After `origin` is available:

- Protect `main`; require PRs, resolved reviews, current branches, signed commits/tags if the project policy supports them, and all platform/security checks.
- Use least-privilege default workflow permissions (`contents: read`) and elevate only specific release jobs.
- Block force pushes and branch deletion.
- Enable secret scanning/push protection, Dependabot (or equivalent), code scanning, and private vulnerability reporting where available.
- Add `CODEOWNERS` for DSP/realtime, auth/network, installers/signing, and dependency/license manifests.
- Use protected release environments for Apple/Windows signing and provider configuration. Never expose secrets to fork PR jobs.
- Pin third-party actions by full commit SHA and document the human-readable release in comments.
- Upload immutable test/build artifacts with short retention for PRs; release artifacts originate only from the tagged commit.
- Generate checksums, SBOM, provenance/attestation where supported, and an exact package-content manifest.

## Pull request evidence template

Each milestone PR should include:

- Scope and explicit non-scope.
- Files/components changed.
- Upstream compatibility impact.
- Security, realtime, state, installer, and license impact.
- Commands and environment used.
- Build/test results with artifact hashes where applicable.
- Manual host/standalone checks.
- Known limitations and rollback strategy.

## Release/tag policy

- Use Amphibia-owned semantic versions beginning only after the renamed baseline; do not continue NAM's `0.7.x` sequence as if it were the same product.
- Annotated/signed tags point to reviewed commits on protected `main`.
- CI creates a draft release; a maintainer verifies signatures, hashes, packages, notices, and smoke tests before publication.
- Release notes identify the exact upstream NAM plugin/Core bases and disclose material deviations.
- Never move a published tag or replace an artifact under the same version.

## Milestone 0 commit/tag handling

The documentation commit should use the configured developer identity if present and contain only `docs/`. A local lightweight milestone marker may be created only if requested; public tags wait for `origin` and review. The final handoff records the commit hash and dirty/submodule status.
