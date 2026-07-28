# Milestone 8 release-candidate validation map

Status: pre-validation release-readiness gate

Evidence date: 2026-07-28

Evidence ID: `RC-EVIDENCE-2026-07-28`

Candidate label: **Amphibia 0.1.0 RC validation build**

Publication status: unsigned Windows x64 validation candidate only; not a
stable release

This map was created before any Milestone 8 install, uninstall, repair,
upgrade, destructive-cleanup, packaging, or environment-changing test. It
defines the candidate under review, the available environments, isolation
rules, evidence vocabulary, exact executable test matrix, and release gates.
It does not turn an unrun test into evidence.

## Candidate and source baseline

The repository was clean on `main` at
`498cc4f13395a4c5715282f8dd6b24a89c4e46b7` when the Milestone 8 audit began.
The existing Milestone 7 artifacts were built from the clean source commit
`55604580c5a10676ade2dc202a8f4abad8c4fc74`.

The following compatibility identities remain frozen:

| Contract | Candidate value |
| --- | --- |
| Product | `Amphibia` |
| Product/file version | `0.1.0` |
| Plug-in/manufacturer codes | `AmPh` / `AmBi` |
| VST3 processor FUID | `7DBF8585-2FC5-4817-AE21-F7910D0330C1` |
| VST3 controller FUID | `893C1354-A5D5-416D-B84C-CA8AE0C27034` |
| Inno Setup `AppId` | `802D7F30-6B3A-44C3-B809-93DA217821CA` |
| State writer magic/version | `###Amphibia###` / `1.1.0` (the frozen `1.0.0` prefix plus optional managed references) |
| Settings and managed-library root | `%LOCALAPPDATA%\Amphibia` |
| Credential target | `org.amphibiaaudio.Amphibia.TONE3000/oauth-v1` |

Milestone 8 must not change these values, DSP behavior, parameter/state
contracts, setup ownership, library schemas, or product-owned paths unless a
concrete validation failure requires a narrow correction and a regression
test.

## Existing artifact intake

The Milestone 7 artifact set is present under
`artifacts/release/0.1.0/`. A read-only host-side SHA-256 recalculation matched
the recorded values:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `Amphibia-0.1.0-Windows-Setup.exe` | 4,257,997 | `e26fdd0c7a72b1c770b4bcdf6c912e98616aebaa2bbe4b99958af9cf21a9260e` |
| `Amphibia-0.1.0-Windows-Portable.zip` | 4,381,623 | `3048bbded4e39f37b6fac52e0821d444eb4f6c09aafa1d4ceadf86fbb1c0bf24` |
| `Amphibia-0.1.0-Windows-VST3.zip` | 2,150,819 | `69b634300577ad4652ea822fff7079c61ffee97b2e8c55c4cc4581b211ca38a3` |
| `Amphibia-0.1.0-Windows-Standalone.zip` | 2,233,614 | `42e49d8dab81080fbfbf272571aae0de0494bad742306fe1df3cfa8fb7d0e912` |
| `SOURCE-COMMIT.txt` | not release-significant | `9c4b8f27e3d9a8ee93c5508f23ea21bff6836313e1b9237e5cf6ea945357d7a4` |
| `DEPENDENCY-VERSIONS.txt` | not release-significant | `a8c80fd254d0fa996015536dc4ddb962eae6424a715bc72624dd5d69e7fe0ada` |
| `THIRD_PARTY_NOTICES.txt` | not release-significant | `45efa9074ee742a4ddf4d8be632c1eafe525fe504b1c3d8ab7e10124ebd38647` |
| `ARTIFACT-MANIFEST.txt` | not release-significant | `18206f804272925147edc622e30f29bd62a092928f2551454b00ad92bd99ba53` |
| `SHA256SUMS.txt` | not release-significant | `faaac3a8c696f9aca1f093909401f49d4d1191b2287cbad5c8e4455a71b7b913` |

This is only artifact-intake evidence on the development host. It is not a
clean-environment copy, install, launch, or runtime result. The setup
executable is `NotSigned`, matching the manifest. Any Milestone 8 rebuild
supersedes these hashes and must receive a new manifest and verified checksum
set.

## Installer and package contract under test

| Area | Candidate contract |
| --- | --- |
| Architecture | Windows x64 only |
| Minimum OS | Windows 10 x64, build `10.0.10240` or later |
| Privileges | Per-machine, administrative install |
| Standalone destination | `%ProgramFiles%\Amphibia\Amphibia.exe` |
| VST3 destination | `%CommonProgramFiles%\VST3\Amphibia.vst3\` |
| Components | `standalone`, `vst3`, `documentation`; at least one executable component |
| Shortcuts | Start Menu default-on with standalone; desktop default-off; no broken VST3-only application shortcuts |
| Upgrade identity | Frozen Inno `AppId`; prior component/task choices preserved where supported |
| Repair | Same-version reinstall |
| Default uninstall | Remove only installer-owned files, shortcuts, and registration; preserve all user data and credentials |
| Optional cleanup | Interactive, default-No, fixed Amphibia Local AppData root plus exact Amphibia credential only |
| External content | Never delete configured NAM/IR paths, ZIP sources, official NAM data, or other user-owned paths |
| Runtime | Release `/MT`; no Visual C++ Redistributable bundled |
| Expected imports | Windows system UI/shell, audio, transport/security, credential, socket, and host APIs only |
| Signing | Unsigned; no publisher/reputation claim |

Component-selection validation must cover standalone-only, VST3-only,
combined, component change, and attempted documentation-only selection.
Install and uninstall evidence must compare pre/post snapshots of fixed
product roots, uninstall registration, shortcuts, isolated user data,
isolated exact-test credentials, and external sentinels.

## Available environments at map time

Only one primary development environment is currently observable:

| Property | Observed state |
| --- | --- |
| OS | 64-bit Windows, registry build `26200.8875`, display version `25H2` |
| Role | Primary development profile; not disposable |
| Windows Sandbox executable | Not present |
| Hyper-V PowerShell module | Not present |
| VirtualBox command line | Not present |
| REAPER | x64 installation present at `C:\Program Files\REAPER (x64)` |
| Amphibia standard install | Standalone and VST3 not present |
| Official Neural Amp Modeler standard install | Standalone and VST3 not present |
| Test data-root overrides | Not configured |
| Test credential override | Not configured |
| Audio-device inventory | Not established; protected CIM inventory was unavailable |
| Real audio input/output | Unconfirmed and not authorized for automated mutation |
| Secondary standard user | Not provisioned for this evidence run |

The registry product name reported `Windows 10 Home` while the build/display
version are newer; the numeric build is the recorded fact and no marketing
edition inference is used. The installed REAPER copy may support future
manual DAW evidence, but it does not make the primary machine an acceptable
installer test environment.

No isolated Windows environment has been confirmed. Therefore no installer,
uninstaller, repair, upgrade, component-change, multi-user, held-file,
credential-cleanup, registry-mutation, or DAW-rescan test may run on the
primary environment. These items remain `BLOCKED` until a disposable VM or
Windows Sandbox with rollback is supplied.

macOS hardware/runner and signing/notarization credentials are unavailable.
TONE3000 production OAuth registration, redirect ownership, and branding
approval are unavailable. Both are release blockers, not omitted test cases.

## Isolation and rollback protocol

All environment-changing Windows validation must use a disposable VM or
Windows Sandbox that can be destroyed or reverted after the run.

Before copying artifacts into that environment:

1. record the environment identifier, Windows edition/build/architecture,
   account/elevation model, snapshot identifier, and installed DAW/NAM state;
2. take a rollback snapshot or confirm disposable Sandbox teardown;
3. use synthetic fixtures only, with sentinels outside every Amphibia-owned
   root;
4. use only a `.test` credential target created for the validation run;
5. record original and copied artifact hashes independently; and
6. capture logs under an evidence directory that contains no username,
   credential value, machine serial, access token, model content, or private
   path.

For destructive-cleanup testing, the isolated environment must include:

- Amphibia-owned settings, managed-library objects, caches, logs, staging, and
  quarantine fixtures;
- at least one external NAM directory, IR directory, ZIP source, official-NAM
  sentinel, unrelated Local AppData sentinel, and unrelated credential;
- a reparse/junction boundary fixture if the environment permits it; and
- a synthetic exact-test Amphibia credential.

The test must first prove default uninstall preservation. Explicit cleanup may
then remove only the fixed isolated Amphibia root and exact test credential.
It must not enumerate or delete unrelated credentials and must not traverse
external or reparse-point targets. Revert or destroy the environment
afterward.

Local automated tests that do not install software may use temporary
directories and process-scoped environment overrides. They must never point a
test data root at the real `%LOCALAPPDATA%\Amphibia` tree, and credential tests
must use the repository's `.test` target enforcement. Build/package output
must remain below ignored `artifacts/` paths.

## Evidence vocabulary

Every matrix item and report conclusion uses exactly one of:

- `PASS`: the documented procedure ran in the recorded environment and the
  captured evidence met its acceptance criteria;
- `FAIL`: the procedure ran and did not meet an acceptance criterion;
- `BLOCKED`: the procedure could not safely run because a required
  environment, identity, approval, credential, tool, or external system was
  unavailable;
- `NOT RUN`: no attempt was made even though the prerequisites were available;
  or
- `NOT APPLICABLE`: the product contract makes the item irrelevant, with a
  recorded reason.

Static source inspection, generated-package inspection, and real installation
are separate evidence classes. A pass in one class cannot be used to mark
another class passed. A test skipped by a script remains `NOT RUN` or
`BLOCKED`; it is never reported as a pass.

## Exact executable validation matrix

The initial state below records availability before Milestone 8 execution.
Final results belong in the release checklist and report with commands, logs,
and evidence paths.

| ID | Validation | Required environment and method | Initial state |
| --- | --- | --- | --- |
| `M8-A01` | Recalculate all intake hashes and compare with `SHA256SUMS.txt` | Primary host, read-only | Available; host intake complete |
| `M8-A02` | Copy candidate into clean environment and independently recheck hashes | Disposable Windows environment | `BLOCKED` |
| `M8-A03` | Secret/credential scan of source, history-visible tracked content, manifests, archives, and binaries using complementary scanners | Primary host, read-only/generated output | Available subject to installed scanners |
| `M8-A04` | Inspect archive allowlists and reject settings, credentials, tokens, caches, logs, PDBs, user content, inherited shell art, or unsafe paths | Primary host, read-only | Available |
| `M8-A05` | Inspect PE architecture, version metadata, imports, exports, runtime, hardening metadata, and Authenticode state | Primary host, read-only | Available |
| `M8-A06` | Generate and validate machine-readable SBOM; compare it with pins, notices, and packaged contents | Primary host/generated output | Available |
| `M8-A07` | Run reputable dependency vulnerability scan and record tool/database timestamp, scope, findings, and limitations | Primary host or isolated CI with scanner database | Tool/database availability unconfirmed |
| `M8-A08` | Confirm no unexpected network endpoint, path, developer workspace, secret, or personal identifier in packaged binaries/manifests | Primary host, static inspection | Available |
| `M8-T01` | Full Python/static Milestone 0-8 suite | Primary host, non-installing | Available |
| `M8-T02` | Native Milestone 2-6 suites with isolated temporary roots and `.test` credentials | Primary host, non-installing | Available |
| `M8-T03` | Fresh Release x64 standalone/VST3 rebuild and clean package generation | Primary host, ignored build/artifact roots | Available, subject to tool access |
| `M8-I01` | Fresh standalone-only install and launch | Disposable Windows environment | `BLOCKED` |
| `M8-I02` | Fresh VST3-only install; host discovery and load | Disposable Windows environment with supported DAW | `BLOCKED` |
| `M8-I03` | Fresh combined install and documentation/shortcut validation | Disposable Windows environment | `BLOCKED` |
| `M8-I04` | Component selection/change and documentation-only rejection | Disposable Windows environment | `BLOCKED` |
| `M8-I05` | Upgrade from prior lower-version package, preserving state/component choices | Disposable Windows environment and prior fixture | `BLOCKED` |
| `M8-I06` | Downgrade refusal with accurate message | Disposable Windows environment and version fixture | `BLOCKED` |
| `M8-I07` | Same-version repair restores installer-owned files only | Disposable Windows environment | `BLOCKED` |
| `M8-I08` | Default uninstall preserves all user data, external content, and credentials | Disposable Windows environment | `BLOCKED` |
| `M8-I09` | Explicit cleanup removes only isolated Amphibia root and exact test credential | Disposable Windows environment using cleanup fixtures | `BLOCKED` |
| `M8-I10` | Multi-user elevation/original-user launch and ownership | Disposable Windows environment with admin + standard user | `BLOCKED` |
| `M8-I11` | Installer interruption/rollback and failed-install cleanup | Disposable Windows environment with snapshot | `BLOCKED` |
| `M8-I12` | Upgrade/uninstall while DAW holds VST3; reboot/pending-file behavior | Disposable Windows environment with supported DAW | `BLOCKED` |
| `M8-D01` | REAPER x64 discovery/rescan, plug-in load, project save/reopen, and state recall | Disposable Windows environment; local host presence alone is insufficient | `BLOCKED` |
| `M8-D02` | Standalone first-run/setup/audio-settings flows with real supported hardware | Disposable/manual Windows environment with hardware | `BLOCKED` |
| `M8-D03` | Real NAM and IR load, bypass, input/output, normalization, gate, tone stack, resampling, latency, rapid selection, and malformed-input resilience | Manual Windows audio environment with cleared fixtures | `BLOCKED` |
| `M8-D04` | Offline render/bounce and reload comparison | Supported DAW/manual audio environment | `BLOCKED` |
| `M8-C01` | Official Neural Amp Modeler side-by-side install, independent load/state, and uninstall isolation | Disposable Windows environment with official NAM | `BLOCKED` |
| `M8-C02` | `0.1.0` state round trip and documented legacy read compatibility | Automated/native tests | Available |
| `M8-P01` | TONE3000 unconfigured state fails closed without live credentials or endpoints | Automated/static tests | Available |
| `M8-P02` | Live TONE3000 authorization, redirect, branding, download, and revocation | Approved production registration and manual environment | `BLOCKED` |
| `M8-S01` | Authenticode signing and post-sign verification with approved identity | Protected signing environment and real certificate | `BLOCKED` |
| `M8-M01` | macOS build, setup, CoreAudio, package, signing, notarization, and stapling | macOS runner/hardware and approved identities | `BLOCKED` |

## Supply-chain and SBOM plan

Milestone 8 will add one deterministic CycloneDX JSON SBOM generator and a
repository validator. The SBOM must identify the Amphibia application,
inherited Neural Amp Modeler plug-in base, NAM Core, iPlug2,
AudioDSPTools, Eigen, minizip-ng, zlib, nlohmann/json, and other compiled or
packaged runtime components discovered by the build. It must record exact
repository/submodule revisions, declared licenses where established, package
relationships, the generator identity/version, and hashes where practical.

The generated SBOM will be staged beside the release artifacts, included in
the artifact manifest and checksum coverage, and checked against
`DEPENDENCY-VERSIONS.txt`, `THIRD_PARTY_NOTICES.txt`, repository pins, and
archive contents. The validation report must state whether an independent
CycloneDX schema/CLI validator was available; repository structural validation
must not be described as independent validation.

Vulnerability review must name the scanner and version, advisory database
timestamp, scanned manifests/components, exclusions, and every finding or
accepted limitation. No finding may be summarized as “zero vulnerabilities”
unless the scanner completed successfully against a current database and its
scope supports that conclusion.

## Asset, license, and provenance gates

The authoritative asset record is
`NeuralAmpModeler/resources/ASSET_PROVENANCE.md`. Its current unresolved items
are release blockers:

- inherited compiled UI raster/SVG art has incomplete redistribution
  provenance;
- Roboto and Michroma font files lack exact source/version/license records;
- quarantined inherited icons must remain excluded; and
- an allowed file is not automatically an allowed public-release asset.

Milestone 8 must either establish exact origin, license, redistribution
conditions, hashes, and packaged usage for every compiled/packaged asset or
remove/replace the asset through a narrow, tested change. A regression test
must fail if an unresolved asset re-enters compiled resources, staging, or
archives.

The Windows release license review must reconcile actual files and versions
with `THIRD_PARTY_NOTICES.md`, `ATTRIBUTION.md`, `docs/DEPENDENCIES.md`, the
SBOM, and artifact notices. It must explicitly cover upstream NAM, NAM Core,
iPlug2, AudioDSPTools, Eigen, minizip-ng, zlib, nlohmann/json, ASIO/VST3 SDK
terms where applicable, Microsoft compiler/SDK outputs, Inno Setup, and every
asset/font. Legal-policy decisions that require the repository owner remain
`BLOCKED`; they are not silently accepted by engineering.

## Security and privacy gates

The source and generated candidate must be scanned for private keys, PFX/P12
material, passwords, bearer/OAuth tokens, API keys, live OAuth identifiers,
credential values, user names, home/workspace paths, machine identifiers, and
unexpected endpoints. Known inert test tokens and `.test` identifiers require
explicit allowlist rationale.

Binary/archive inspection must also verify:

- x64 PE type and release version metadata;
- no dynamic MSVC runtime dependency when `/MT` is claimed;
- no unexpected bundled private DLL, executable, shell extension, driver,
  service, updater, or installer payload;
- no settings, caches, logs, models, IRs, ZIP sources, or credentials;
- no debug symbols or developer build path leakage;
- no unexpected export/import or network endpoint; and
- `NotSigned` status is reported accurately until real signing succeeds.

## CI evidence policy

Release-candidate CI remains non-publishing with read-only repository
permissions. It must run the SBOM, secret, provenance, archive, binary,
checksum, installer/static-contract, and existing regression gates and upload
short-retention evidence labelled `unsigned` and `not-for-public-release`.
It must not create a tag, GitHub Release, public download, stable-channel
artifact, signature claim, or external deployment.

Environment-dependent manual gates remain separate from CI. CI success cannot
close clean-install, DAW, hardware, multi-user, coexistence, real signing,
TONE3000 approval, or macOS gates.

## Release-blocker classification at map time

| Gate | Initial classification |
| --- | --- |
| Existing artifact checksum intake | Host-side verification complete; clean-environment intake still blocked |
| SBOM generation/validation | Open implementation work |
| Secret/archive/binary review | Open validation work |
| Dependency vulnerability scan | Scanner/database availability unconfirmed |
| Asset/font provenance | Blocking |
| Windows dependency/tool/license review | Open; owner/legal decisions may block |
| Clean install/upgrade/repair/uninstall matrix | Blocking: no disposable environment |
| DAW/manual audio validation | Blocking: no disposable/manual audio environment |
| Official NAM coexistence | Blocking: no isolated environment or NAM installation |
| Authenticode | Blocking: no approved identity/protected signer |
| TONE3000 live integration | Blocking: unconfigured/unapproved |
| macOS | Blocking: unavailable and unvalidated |
| Final owner/support/security contacts | Blocking until established by repository owner |

Unless every required Windows release gate is closed with recorded evidence,
the final Milestone 8 conclusion must be **blocked from public release**. The
most favorable permitted label in that state is the candidate label at the
top of this map; it must never be shortened to “stable,” “production-ready,”
or an equivalent public-release claim.

## Planned Milestone 8 evidence

Create:

- deterministic SBOM generation and validation tooling;
- Milestone 8 release/static regression tests;
- `docs/RELEASE_CANDIDATE_NOTES.md`;
- `docs/RELEASE_CHECKLIST.md`;
- `docs/MILESTONE_8_REPORT.md`; and
- any narrowly scoped license/provenance review record needed to reconcile
  notices and the SBOM.

Modify only as supported by validation evidence:

- `scripts/package-windows.ps1`;
- `.github/workflows/package-windows.yml`;
- packaging, signing, release, dependency, attribution, security, privacy,
  changelog, milestone, and threat-model documentation;
- `NeuralAmpModeler/resources/ASSET_PROVENANCE.md`; and
- a product/resource source only if a concrete release gate cannot otherwise
  be closed, accompanied by a regression test.

No public release, repository push, tag, GitHub Release, external upload,
certificate enrollment, production provider registration, user-data
destruction, or primary-machine installation is authorized by this map.
