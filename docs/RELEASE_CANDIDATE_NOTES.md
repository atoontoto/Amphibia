# Amphibia 0.1.0 RC validation notes

Evidence ID: `RC-EVIDENCE-2026-07-28`

Candidate: **Amphibia 0.1.0 RC validation build**

Decision: **BLOCKED FROM PUBLIC RELEASE**

This is an unsigned Windows x64 engineering-validation build. It is not a
stable release, is not production-ready, and has not been published. Do not
redistribute the generated binaries as an Amphibia release.

## Validated in this evidence run

- A clean `main` source commit built the standalone and VST3 Release x64
  targets with MSBuild 18/v145 and Windows SDK 10.0.26100.0.
- The Milestone 0-8 static contracts and Milestone 2-6 native suites passed.
- Inno Setup 6.7.3 compiled the component installer without installing it.
- Product binaries report Amphibia `0.1.0`, x64, `/MT`, ASLR/high-entropy/NX,
  Control Flow Guard, expected imports, and `NotSigned`.
- Archive allowlists, checksums, notices, compiled-asset evidence, and the
  deterministic 51-component CycloneDX 1.5 SBOM passed repository validation.
- CycloneDX CLI 0.33.1 independently accepted the final SBOM schema.
- The candidate package's built-in scanner covered 536 tracked files plus 78
  artifact/archive files with zero findings; the completed documentation tree
  scan covered 539 tracked files. Gitleaks 8.30.1 independently scanned 330
  commits and reported no leaks.
- The unconfigured TONE3000 build remains fail-closed and starts no live
  provider flow.

These are build and static/generated-artifact results. They are not install,
DAW, audio-hardware, signing, live-provider, or macOS results.

## Public-release blockers

- No disposable Windows environment was available for clean install,
  component-change, upgrade, downgrade, repair, uninstall, cleanup,
  multi-user, rollback, held-file, REAPER, or official-NAM coexistence tests.
- No manual real-device, audio, model/IR, project-recall, or offline-render
  matrix was available.
- All artifacts are unsigned. No protected signing identity was available.
- The 31 compiled inherited raster/SVG UI files have exact hashes but
  incomplete origin and redistribution authority. The two embedded fonts are
  individually cleared and licensed.
- The compiled legacy ASIO 2.3 interface remains blocked on an authoritative
  license/owner decision or removal. VST3 SDK 3.8.0 is separately MIT-cleared.
- A current C++ vulnerability result could not be completed without sending
  repository-derived dependency metadata to an external service; the offline
  OSV database has no `GIT` ecosystem database. No zero-vulnerability claim is
  made.
- TONE3000 production registration, approved redirect, branding, and live
  authorization/download evidence are unavailable.
- macOS build, CoreAudio, packaging, signing, notarization, and stapling are
  unavailable and unvalidated.
- Final owner/support/security-contact and protected-publication decisions
  remain open.

## Compatibility and behavior

Milestone 8 does not change product identity, plug-in IDs, VST3 FUIDs, Inno
`AppId`, state layout, settings/library paths, credential namespace, parameter
order, DSP behavior, or provider behavior. State writer version remains
`1.1.0`, preserving the full `1.0.0` prefix and legacy NAM readers.

The only product-build hardening change enables Control Flow Guard for Windows
Release binaries and removes absolute PDB paths from the PE debug record. The
installer now explicitly allowlists the six packaged license texts and enables
the Inno x64 loader/redirection guard.

## Generated candidate

The generated evidence under `artifacts/release/0.1.0/` was built from clean
commit `df22facae5bc517c8c471e3d4c5d2864524733f7`.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `Amphibia-0.1.0-Windows-Setup.exe` | 4,759,579 | `4ce0c16e2afa7be4f1ae217620afc248c92101a5b3b67856ac98a56c8478b80d` |
| `Amphibia-0.1.0-Windows-Portable.zip` | 4,434,067 | `7fe49fcd4f14028c3fefe0989de3cd8216d95e5fb4242db1282463e9f78628e1` |
| `Amphibia-0.1.0-Windows-VST3.zip` | 2,186,414 | `b638785bbdd9311612d7bffe6e9f8903f6e0014026949ef372470218f03bcd34` |
| `Amphibia-0.1.0-Windows-Standalone.zip` | 2,268,921 | `7bea80657423aaa1305203358fd75a5922c8645aeda23bf7faa0d1305328d2f4` |
| `Amphibia-0.1.0-SBOM.cdx.json` | 70,095 | `94b5c19aa88364a39916915b5e631058e191d7cc77866473d70e503ad5df6861` |

The complete checksum set is generated in `SHA256SUMS.txt`. These local,
ignored artifacts were not committed, signed, tagged, uploaded, or published.
