# Milestone 8 release-candidate validation report

Evidence ID: `RC-EVIDENCE-2026-07-28`

Evidence date: 2026-07-28

Candidate: **Amphibia 0.1.0 RC validation build**

Final decision: **BLOCKED FROM PUBLIC RELEASE**

## Outcome

Milestone 8 closes the repository-side release-evidence work that was
available on the Windows development host. The source now has deterministic
CycloneDX/asset evidence, immutable release-input pins, complementary secret
scans, PE/import/hardening inspection, explicit license staging, release
workflow hardening, and a complete pass/block checklist.

A clean committed source revision rebuilt the Windows standalone and VST3,
ran every available static/native suite, compiled the installer, generated
three archives, and passed package/checksum/security/SBOM validation. The
artifacts are intentionally unsigned and local.

This does not close the environmental and authority gates. No Amphibia
installer or uninstaller was executed on the primary machine. Clean-VM
install/lifecycle, DAW, hardware/audio, official-NAM coexistence, real signing,
live TONE3000, macOS, inherited UI-art authority, legacy ASIO authority,
complete vulnerability review, and owner/contact policy remain blocked.

## Source and commits

| Purpose | Commit |
| --- | --- |
| Milestone 7 validation baseline | `498cc4f13395a4c5715282f8dd6b24a89c4e46b7` |
| Pre-test M8 validation map | `59ec41378300538ad85af1cd02985d8607d50966` |
| M8 validation implementation and clean artifact source | `df22facae5bc517c8c471e3d4c5d2864524733f7` |

The validation map was committed before packaging or any possible
environment-changing validation. The final documentation commit is newer than
the generated-artifact source and does not alter the candidate binaries.

## Environment

| Item | Recorded value |
| --- | --- |
| Host | 64-bit Windows numeric build `26200.8875`, display version `25H2` |
| Toolchain | MSBuild `18.8.2+ce25c01082c9c46cd02ad1ff3ff8f16fe5cc2f44`, platform toolset `v145` |
| Windows SDK | `10.0.26100.0` |
| Installer compiler | Inno Setup `6.7.3`, non-commercial compiler mode |
| Python | CPython 3.14 used for repository contracts |
| DAW observed | REAPER x64 present on the primary host; not used for install/rescan evidence |
| Isolation | No Windows Sandbox, Hyper-V module, VirtualBox CLI, or other disposable Windows target available |
| macOS | Unavailable |
| Signing identity | Unavailable |
| TONE3000 production registration | Unavailable/unconfigured |

The OS registry marketing name was inconsistent with the numeric
build/display version, so this report records the numeric values without
inferring an edition name.

## Implemented evidence controls

- `release/dependency-pins.json` records exact source revisions, immutable
  VST3 SDK tag/commit, dependency/build-input hashes, validation-tool hashes,
  and immutable GitHub Action commits.
- `scripts/generate-sbom.py` deterministically generates CycloneDX 1.5 and the
  exact `main.rc` compiled-asset evidence. Repeated runs are byte-identical for
  the same inputs.
- `scripts/validate-sbom.py` checks repository/submodule pins, component
  relationships, licenses, asset hashes, private-path exclusion, archive
  evidence, and final checksums.
- `scripts/scan-release-secrets.py` checks tracked source, artifacts, and ZIP
  members for credential/key patterns, sensitive file types, unsafe paths,
  debug/user content, and private workspace/profile paths.
- `tests/milestone8/test_release_readiness.py` enforces the evidence,
  hardening, installer, action-pin, dependency-pin, asset, SBOM, and scanner
  contracts.
- Release Windows compilation enables Control Flow Guard and
  `/PDBALTPATH:%_PDB%`. The Inno source uses the x64 setup loader,
  redirection guard, and explicit license-file allowlist.
- All workflow action references are immutable commits. The Windows package
  workflow has read-only repository permission, verifies downloaded inputs and
  scanners by SHA-256, independently validates CycloneDX, and uploads only a
  seven-day artifact labelled unsigned/not-for-public-release. It contains no
  publishing step.

## Commands and results

The principal local validation sequence was:

```text
python tests/identity/validate_identity.py
python tests/characterization/test_inherited_contract.py
python tests/characterization/test_state_contract.py
python tests/characterization/test_milestone6_contract.py
python tests/milestone7/test_installer.py
python tests/milestone7/test_packaging.py
python tests/milestone8/test_release_readiness.py
.\scripts\package-windows.ps1
cyclonedx validate --input-file <final SBOM> --input-format json --input-version v1_5 --fail-on-errors
gitleaks detect --source . --redact --report-format json
osv-scanner scan source --recursive --include-git-root <release exclusions> --offline --offline-vulnerabilities
rg -a <URL/private-path patterns> <standalone/VST3/setup binaries>
```

Results:

| Evidence | Result |
| --- | --- |
| Identity contract | PASS, 35 identity-bearing files |
| Inherited characterization contract | PASS |
| State compatibility contract | PASS |
| Milestone 6 setup/VST3/real-time static contract | PASS |
| Milestone 7 installer contract | PASS |
| Milestone 7 packaging contract | PASS, both source and final artifacts |
| Milestone 8 deterministic evidence contract | PASS |
| Milestone 2 native suite | PASS, 10 groups |
| Milestone 3 native library suite | PASS |
| Milestone 4 native foundation suite | PASS, 20 cases |
| Milestone 5 native selection suite | PASS, 9 cases |
| Milestone 6 native setup suite | PASS, 7 groups |
| Clean standalone/VST3 Release rebuild | PASS |
| Inno Setup compile | PASS; installer was not run |
| Final checksum verification | PASS |
| Repository SBOM/asset/archive validation | PASS, with asset release gate intentionally BLOCKED |
| CycloneDX CLI 0.33.1 independent schema validation | PASS |
| Built-in source/artifact scan | PASS, 614 candidate-package file observations and 539 completed-tree files, zero findings |
| Gitleaks 8.30.1 | PASS, 330 commits/about 7.88 MB, no leaks |
| OSV-Scanner 2.4.0 release-scope scan | BLOCKED, no offline `GIT` database and no authorization to submit repository-derived metadata externally |

Build warnings from inherited/upstream conversion diagnostics remain visible
in the MSBuild output; both product targets completed successfully. This
report does not convert compiler warnings into a warning-free claim.

Binary string review found the documented Neural Amp Modeler source/user
links, TONE3000 site/authorize/token endpoints, standard license/schema URLs,
and the setup manifest namespace. No private user/workspace path, credential,
live OAuth client identifier, or unexpected actionable endpoint was found.
Short non-actionable regex fragments were treated as binary-string false
positives rather than URLs.

## SBOM and vulnerability evidence

The final SBOM contains 51 components: the Amphibia dependency surface plus
33 compiled assets (31 images and 2 fonts). It records clean source commit
`df22facae5bc517c8c471e3d4c5d2864524733f7` and explicitly marks public
release, signing, TONE3000, macOS, ASIO, and artwork status.

CycloneDX CLI `0.33.1+b3cfa4b0edc356dad07e0b6e7ab6da0a94af0246`
was downloaded from the official project release and pinned locally by
SHA-256
`9b360974c41a5d612eb026fb5e3bb1fa28e08865f7a4f915665b8bc94bd3bc31`.
It independently reported `BOM validated successfully`.

OSV-Scanner `2.4.0` / osv-scalibr `0.4.5`, commit
`b56b5191101d5f27d4787d5583d8d01e9518a7af`, was used in offline mode.
The release-scope scan reached the exact Git-pinned dependency set but exited
`127` because no offline OSV `GIT` ecosystem database was available. The
policy reviewer did not authorize an online scan that would disclose
repository-derived dependency metadata. A preliminary broader scan found one
advisory for `idna 3.9.0`
(`PYSEC-2026-215`/`GHSA-65pc-fj4g-8rjx`) in
`NeuralAmpModelerCore/docs/requirements.txt`; that documentation dependency is
excluded from the release workflow and is neither linked nor packaged.

Accordingly, dependency vulnerability review is `BLOCKED`. It is inaccurate
to claim that this candidate has zero known vulnerabilities.

## Binary inspection

| Binary | Bytes | SHA-256 | x64/version | Hardening | Signature |
| --- | ---: | --- | --- | --- | --- |
| Standalone | 3,767,808 | `813ba242c2d55c9bcfb2c2bc8fd75fe3f0d1131e60c58c47ca8a110fa0a98b5b` | x64 / 0.1.0 | high-entropy ASLR, dynamic base, NX, CFG | `NotSigned` |
| VST3 | 3,670,016 | `06ecda668d6b93d956b12363bce474aff73ff7606073cc0655f1ba2eea21d0c6` | x64 / 0.1.0 | high-entropy ASLR, dynamic base, NX, CFG | `NotSigned` |
| Setup | 4,759,579 | `4ce0c16e2afa7be4f1ae217620afc248c92101a5b3b67856ac98a56c8478b80d` | x64 / 0.1.0 | high-entropy ASLR, dynamic base, NX; Inno loader does not report CFG | `NotSigned` |

The VST3 exports `InitDll`, `ExitDll`, and `GetPluginFactory`. The standalone
exports none. The setup exports only its Inno runtime wrappers. Product
imports were within the Windows system-library allowlist, and neither product
binary dynamically imports the MSVC runtime. PDB records contain only a
basename, not the workspace path.

## Artifact inventory

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `Amphibia-0.1.0-Windows-Setup.exe` | 4,759,579 | `4ce0c16e2afa7be4f1ae217620afc248c92101a5b3b67856ac98a56c8478b80d` |
| `Amphibia-0.1.0-Windows-Portable.zip` | 4,434,067 | `7fe49fcd4f14028c3fefe0989de3cd8216d95e5fb4242db1282463e9f78628e1` |
| `Amphibia-0.1.0-Windows-VST3.zip` | 2,186,414 | `b638785bbdd9311612d7bffe6e9f8903f6e0014026949ef372470218f03bcd34` |
| `Amphibia-0.1.0-Windows-Standalone.zip` | 2,268,921 | `7bea80657423aaa1305203358fd75a5922c8645aeda23bf7faa0d1305328d2f4` |
| `Amphibia-0.1.0-SBOM.cdx.json` | 70,095 | `94b5c19aa88364a39916915b5e631058e191d7cc77866473d70e503ad5df6861` |
| `ASSET-PROVENANCE.json` | 19,342 | `54ac20f803a1af707fa2f12305f4d2e9f3a0e8f0e719a020eb8779aadabfa37b` |
| `RELEASE-SECURITY-SCAN.json` | 1,151 | `3a0fc842b253afc9ede63733b7013a35b8a8372756bccbb34063a803e5a7b9c0` |
| `BINARY-INSPECTION.txt` | 2,010 | `95d98fa1b654f0533e70312986abede0f1b40ad30369c222ef4ad5ece4654872` |

`ARTIFACT-MANIFEST.txt`, `SOURCE-COMMIT.txt`,
`DEPENDENCY-VERSIONS.txt`, notices, and the files above are all covered by the
generated `SHA256SUMS.txt`. The three ZIP archives carry the SBOM,
compiled-asset evidence, notices, and complete staged font/dependency license
texts. The setup carries the explicit license set.

## License and asset result

Engineering review closed the VST3 SDK 3.8.0 MIT treatment, Roboto
Apache-2.0, Michroma OFL-1.1, current non-commercial Inno Setup 6.7.3 use, and
the absence of a separately bundled VC Redistributable. Exact license texts
and asset hashes accompany the candidate.

The review did not establish redistribution authority for the 31 inherited
compiled raster/SVG UI files. It also found no adjacent authoritative
agreement for the legacy ASIO 2.3 interface compiled into the standalone.
Those are independent public-release blockers. See
`LICENSE_REVIEW_WINDOWS.md` and `ASSET_PROVENANCE.json`.

## Installation and destructive-test restraint

The primary host was not a disposable release-test environment. Therefore
the setup executable was compiled and inspected but never launched.
Installation, uninstallation, upgrade, repair, component mutation, registry
changes, shortcuts, Credential Manager cleanup, user-data cleanup, REAPER
rescan, and official-NAM coexistence were not exercised. No real user content,
credential, or application-data root was modified.

Those gates are `BLOCKED`, not `PASS` and not `NOT APPLICABLE`. The exact
rollback/sentinel procedure remains in the pre-test validation map for a
future disposable environment.

## Final conclusion

Milestone 8 produced a stronger, internally consistent unsigned Windows
validation candidate and closed all safe repository-side work available in
this environment. It did not produce a releasable RC.

Public release remains blocked until every open item in
`RELEASE_CHECKLIST.md` has actual evidence and owner authorization. No tag,
push, release, upload, signature, installer execution, or stable-channel
label was created during this work.
