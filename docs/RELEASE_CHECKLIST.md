# Amphibia 0.1.0 release checklist

Evidence ID: `RC-EVIDENCE-2026-07-28`

Candidate: **Amphibia 0.1.0 RC validation build**

Overall result: **BLOCKED FROM PUBLIC RELEASE**

The terms `PASS`, `FAIL`, `BLOCKED`, `NOT RUN`, and `NOT APPLICABLE` have the
exact meanings defined in `MILESTONE_8_RELEASE_VALIDATION_MAP.md`. A generated
package is not evidence for an installation or runtime test.

## Executable validation matrix

| ID | Result | Evidence or blocker |
| --- | --- | --- |
| `M8-A01` | PASS | The original Milestone 7 artifact hashes were recalculated on the host and matched their recorded checksum set before the M8 rebuild. |
| `M8-A02` | BLOCKED | No disposable Windows environment was available for an independent copy/recheck. |
| `M8-A03` | PASS | Candidate built-in scanner: 536 tracked plus 78 artifact/archive files, zero findings; completed-tree scan: 539 tracked files, zero findings. Gitleaks 8.30.1: 330 commits, about 7.88 MB, no leaks. |
| `M8-A04` | PASS | Milestone 7/8 archive tests and security scanner accepted the three archives; no debug files, settings, credentials, logs, models, IRs, unsafe paths, or user content. |
| `M8-A05` | PASS | `dumpbin`, version APIs, import allowlist, and Authenticode inspection recorded x64/0.1.0/expected imports/`NotSigned`. Standalone and VST3 have high-entropy ASLR, NX, and CFG. Inno Setup is high-entropy ASLR/NX but its generated loader does not report CFG. |
| `M8-A06` | PASS | Deterministic CycloneDX 1.5 SBOM: 51 components including 33 compiled assets. Repository validation passed; CycloneDX CLI 0.33.1 independently validated the final schema. |
| `M8-A07` | BLOCKED | OSV-Scanner 2.4.0 could not complete the release-scope C++ scan offline because no offline `GIT` ecosystem database exists, and external metadata submission was not authorized. A broader preliminary scan found `idna 3.9.0` advisory `PYSEC-2026-215`/`GHSA-65pc-fj4g-8rjx` only in excluded `NeuralAmpModelerCore/docs/requirements.txt`; it is not packaged or linked. No zero-vulnerability claim is made. |
| `M8-A08` | PASS | Artifact scanner, PDB alternate-path hardening, archive checks, and binary string/path checks found no private user/workspace path, credential, or live OAuth client identifier. Extracted URLs matched documented NAM/TONE3000, license/schema, and Inno/Windows metadata strings; no unexpected actionable endpoint was found. |
| `M8-T01` | PASS | Identity, inherited behavior, state, Milestone 6 static, Milestone 7 installer/package, and Milestone 8 contracts passed. |
| `M8-T02` | PASS | Native Milestone 2, 3, 4, 5, and 6 Release x64 executables passed: 10 groups, library suite, 20 provider-foundation cases, 9 selection cases, and 7 setup groups respectively. Tests used generated/temporary fixtures and `.test` credentials. |
| `M8-T03` | PASS | Clean commit `df22facae5bc517c8c471e3d4c5d2864524733f7` rebuilt standalone/VST3 and generated the complete unsigned package/evidence set. |
| `M8-I01` | BLOCKED | No disposable Windows environment for standalone-only install/launch. |
| `M8-I02` | BLOCKED | No disposable Windows/DAW environment for VST3-only install/discovery/load. |
| `M8-I03` | BLOCKED | No disposable Windows environment for combined install/documentation/shortcut checks. |
| `M8-I04` | BLOCKED | No disposable Windows environment for component selection/change. |
| `M8-I05` | BLOCKED | No isolated prior-version upgrade fixture/environment. |
| `M8-I06` | BLOCKED | No isolated version fixture/environment for real downgrade refusal. |
| `M8-I07` | BLOCKED | No disposable environment for same-version repair. |
| `M8-I08` | BLOCKED | Default uninstall preservation was not run on the primary machine. |
| `M8-I09` | BLOCKED | Destructive cleanup was not run; no isolated data/credential/sentinel fixture environment. |
| `M8-I10` | BLOCKED | No isolated administrator/standard-user environment. |
| `M8-I11` | BLOCKED | No disposable snapshot environment for interruption/rollback. |
| `M8-I12` | BLOCKED | No disposable DAW environment for held-file/reboot behavior. |
| `M8-D01` | BLOCKED | REAPER exists on the primary host, but primary-host installation/rescan was prohibited and no disposable DAW environment was available. |
| `M8-D02` | BLOCKED | No isolated/manual real-audio-device environment. |
| `M8-D03` | BLOCKED | No cleared real model/IR and audio-hardware matrix. |
| `M8-D04` | BLOCKED | No supported disposable/manual DAW render environment. |
| `M8-C01` | BLOCKED | Official NAM was not installed and no isolated coexistence environment was available. |
| `M8-C02` | PASS | State 1.1 round trip, frozen 1.0 prefix, managed references, and legacy NAM compatibility passed automated native/static contracts. |
| `M8-P01` | PASS | Static/native provider contracts confirm the repository build is unconfigured and fail-closed without credential, browser, listener, DNS, or HTTP work. |
| `M8-P02` | BLOCKED | No approved production client registration, redirect, branding, or live-provider authority. |
| `M8-S01` | BLOCKED | No approved code-signing certificate or protected signer; every inspected executable is `NotSigned`. |
| `M8-M01` | BLOCKED | No macOS runner/hardware or signing/notarization identities. |

No available prerequisite was silently skipped: the remaining unexecuted
environment-dependent items are `BLOCKED`, not `PASS`.

## Release policy gates

| Gate | Result | Required closure |
| --- | --- | --- |
| Product/version/identity freeze | PASS | No change required. |
| Clean source and repeatable Windows package | PASS | Retain immutable source/pin/checksum evidence. |
| SBOM and compiled-asset inventory | PASS | Retain generator, independent schema validation, and final checksums. |
| Secret and artifact contamination review | PASS | Run both scanners again after any source/package change. |
| Dependency vulnerability review | BLOCKED | Complete a current, policy-approved C++/commit/component scan and adjudicate all findings. |
| VST3 SDK 3.8.0 license | PASS | Retain MIT text and immutable commit/tag pin. |
| Roboto/Michroma font evidence | PASS | Retain exact hashes and Apache-2.0/OFL-1.1 texts. |
| Inno Setup 6.7.3, current non-commercial scope | PASS | Retain compiler version/license evidence; owner decision required if scope becomes commercial. |
| Legacy ASIO 2.3 interface | BLOCKED | Obtain authoritative permission/owner approval or remove it and retest. |
| Compiled inherited UI artwork | BLOCKED | Prove origin/redistribution permission for all 31 images or replace/remove them and retest. |
| Windows install/lifecycle/DAW/audio/coexistence | BLOCKED | Execute the mapped isolated/manual matrices and archive redacted evidence. |
| Authenticode and protected publication | BLOCKED | Sign in a protected environment, verify embedded/final signatures, and establish an approved release channel. |
| TONE3000 production integration | BLOCKED | Obtain registration/redirect/brand approval and complete live privacy/security validation. |
| macOS | BLOCKED | Build and validate CoreAudio/setup/package/sign/notarize/staple on supported macOS. |
| Owner/support/security contacts | BLOCKED | Establish canonical private reporting and support/repository identities. |

## Final authorization

- [ ] Public release authorized
- [ ] Stable-channel label authorized
- [ ] Tag authorized
- [ ] Artifact upload authorized
- [ ] Signing identity approved

All authorization boxes intentionally remain open. This checklist records a
useful unsigned validation candidate, not a releasable RC or stable build.
