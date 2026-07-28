# Milestone 7 report — Windows installer and packaging

Date: 2026-07-28

Product version: 0.1.0

Platform implemented and validated: Windows x64

Artifact source commit: `55604580c5a10676ade2dc202a8f4abad8c4fc74`

## Outcome

Milestone 7 implements a production-oriented Windows installer source,
deterministic package construction, artifact validation, non-publishing CI, and
the user/operator documentation set. The generated candidate is intentionally
unsigned and is not a stable public release.

The Windows standalone and VST3 were freshly rebuilt in Release x64 with the
v145 toolset. The complete automated Milestone 1–7 contract/native test path,
PE metadata/architecture/runtime inspection, Inno Setup compilation, archive
allowlist inspection, manifest generation, and SHA-256 re-verification passed.

No clean Windows Sandbox/VM was available on this host. Installing the package
into the development machine would have modified shared Program Files/VST3
state, so clean-install, upgrade, repair, interactive uninstall, multi-user
cleanup, DAW discovery, and NeuralAmpModeler coexistence remain explicit manual
release gates.

## Delivered

- Frozen installer `AppId`
  `802D7F30-6B3A-44C3-B809-93DA217821CA`.
- Fixed per-machine standalone and VST3 destinations.
- Standalone-only, VST3-only, combined, and custom component choices.
- Optional Start Menu tasks and a default-off desktop shortcut.
- Fresh interactive install launch through `--first-run` as the original user;
  no silent-install launch.
- Stable upgrade family, previous-choice handling, deselected-component cleanup,
  and downgrade refusal.
- Normal/silent uninstall preservation of settings, library, cache, logs,
  external sources, and credentials.
- Separate default-No interactive cleanup limited to the fixed
  `%LOCALAPPDATA%\Amphibia` root and exact TONE3000 credential, with root/reparse
  safety checks and no traversal of external content references.
- Version-derived Release/x64 packaging with a clean-worktree gate.
- Reconstructed VST3 bundle and explicit exclusion of inherited shell art,
  PDBs, user content, settings, credentials, caches, and logs.
- Source/dependency manifests, human-readable notices, artifact inventory, and
  verified SHA-256 sums.
- Optional certificate-store Authenticode signing and independent `signtool`
  verification hooks.
- Pull-request/manual CI producing short-retention
  `unsigned-development` artifacts without release permissions.

## Artifact set

Generated below `artifacts/release/0.1.0/`:

- `Amphibia-0.1.0-Windows-Setup.exe`
- `Amphibia-0.1.0-Windows-Standalone.zip`
- `Amphibia-0.1.0-Windows-VST3.zip`
- `Amphibia-0.1.0-Windows-Portable.zip`
- `SOURCE-COMMIT.txt`
- `DEPENDENCY-VERSIONS.txt`
- `THIRD_PARTY_NOTICES.txt`
- `ARTIFACT-MANIFEST.txt`
- `SHA256SUMS.txt`

Final generated hashes for this evidence run:

```text
e26fdd0c7a72b1c770b4bcdf6c912e98616aebaa2bbe4b99958af9cf21a9260e  Amphibia-0.1.0-Windows-Setup.exe
3048bbded4e39f37b6fac52e0821d444eb4f6c09aafa1d4ceadf86fbb1c0bf24  Amphibia-0.1.0-Windows-Portable.zip
69b634300577ad4652ea822fff7079c61ffee97b2e8c55c4cc4581b211ca38a3  Amphibia-0.1.0-Windows-VST3.zip
42e49d8dab81080fbfbf272571aae0de0494bad742306fe1df3cfa8fb7d0e912  Amphibia-0.1.0-Windows-Standalone.zip
9c4b8f27e3d9a8ee93c5508f23ea21bff6836313e1b9237e5cf6ea945357d7a4  SOURCE-COMMIT.txt
a8c80fd254d0fa996015536dc4ddb962eae6424a715bc72624dd5d69e7fe0ada  DEPENDENCY-VERSIONS.txt
45efa9074ee742a4ddf4d8be632c1eafe525fe504b1c3d8ab7e10124ebd38647  THIRD_PARTY_NOTICES.txt
18206f804272925147edc622e30f29bd62a092928f2551454b00ad92bd99ba53  ARTIFACT-MANIFEST.txt
```

The installer, standalone, and VST3 report Amphibia product/file version 0.1.0.
All three are `NotSigned`, matching the manifest.

## Toolchain and provenance

- MSBuild `18.8.2+ce25c01082c9c46cd02ad1ff3ff8f16fe5cc2f44`
- MSVC platform toolset `v145`
- Windows SDK `10.0.26100.0`
- Inno Setup `6.7.3`
- Upstream NAM plug-in base `0.7.15` /
  `96337e9ab6e3beb619459779bbb5c47e1b04d8c4`
- NAM Core `9c7b185de346fe0725dea537bcee4bc38b5bb6d6`
- iPlug2 `66f9060b5287afdb2ddb83aab5a2a2a8e66a4d5e`
- AudioDSPTools `0827c6c2fc0deced568536142ea86f189e0b98a1`
- Eigen `ed8cda3ce4cb9d12640f9236fed968655a93b604`

`dumpbin` confirmed x64 PE images and no dynamic MSVC runtime dependency.
Release compilation used `/MT`; no Visual C++ Redistributable is included.

## Validation record

Passed:

- fresh `Amphibia-app:Rebuild`, Release/x64/v145;
- fresh `Amphibia-vst3:Rebuild`, Release/x64/v145;
- Milestone 1 identity validator and inherited characterization;
- Milestone 3 state compatibility contract;
- Milestone 6 setup/VST3/real-time static contract;
- Milestone 7 installer and packaging contracts;
- native Milestone 2 async-loading suite;
- native Milestone 3 managed-library/archive suite;
- native Milestone 4 provider/security/transport suite (20 tests);
- native Milestone 5 hosted-selection suite (9 tests);
- native Milestone 6 setup suite (7 groups);
- Inno Setup compilation with no compiler warnings;
- generated archive allowlist/path/content checks;
- generated artifact inventory and SHA-256 verification.

The first sandboxed product rebuild attempt failed in MSBuild `FileTracker` with
`E_ACCESSDENIED`. The identical native build outside the filesystem sandbox
succeeded; this was an execution-environment restriction, not a source/build
error. The complete successful logs followed that retry.

Skipped or unavailable:

- Windows Sandbox/clean VM install/upgrade/repair/uninstall matrix;
- interactive wizard, real audio hardware, and supported DAW discovery/rescan;
- NeuralAmpModeler side-by-side install/run/uninstall;
- reboot/pending-file behavior while a DAW holds the VST3;
- Authenticode verification with a real certificate;
- macOS build/package/sign/notarize;
- live TONE3000 authorization/download.

## Commits

- `e0172de` — installer and packaging audit/map
- `4815650` — Windows installer, packaging pipeline, tests, and CI
- `385752f` — deployment/security/release documentation
- `5560458` — zero-argument native test execution fix found by the full run

The report itself is committed separately after the artifact evidence run.

## Remaining release blockers

- Replace or prove redistribution rights for inherited compiled UI art/fonts.
- Obtain production TONE3000 registration/redirect/branding approval if that
  integration is enabled.
- Supply an approved Authenticode identity and protected signing environment.
- Complete clean-VM, multi-user elevation, DAW, reboot, downgrade, repair,
  uninstall retention/cleanup, and NAM coexistence evidence.
- Complete ASIO/VST3/toolchain and Inno Setup commercial-license review.
- Generate/review a machine-readable SBOM.
- Establish final owner/support/security-contact identifiers.
- Implement and validate macOS setup integration, packaging, signing, and
  notarization.

Until those gates close, all generated artifacts remain unsigned development
candidates and must not be presented as a stable release.
