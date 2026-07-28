# Post-Milestone-8 UI and TONE3000 Configuration Patch report

Evidence date: 2026-07-29

## Outcome

The release-blocking Windows editor defect was repaired at its layout,
visibility, caption-paint, and native-DPI boundaries. The editor now has
readable hosted NAM/IR actions and one bounded seven-section Settings surface.
Amphibia now uses the owner's centralized public TONE3000 desktop client
configuration, exact fixed IPv4 loopback callback, and the existing hosted
selection/provider/library/activation architecture.

Follow-up input-dispatch repair (2026-07-29): cancelling a hosted selection
correctly moved the coordinator to `Cancelled`, but the full-window panel drew
its terminal `Close` button without dispatching its hit area. It therefore
remained topmost and swallowed later clicks. In parallel, direct application
callbacks bypassed iPlug2's default momentary-button animation, leaving their
pressed/dark value latched at `1`. The terminal Close state now shares its
predicate between drawing and input dispatch, hides only the panel without
re-cancelling provider work, and direct application button actions schedule
the normal momentary reset before performing their immediate action.

No audio contract, identity, version, state, parameter, DSP, local-file,
managed-library, archive, installer-family, or artifact-name contract changed.

## Scope statement

This was a post–Milestone 8 corrective patch.
It was not Milestone 9 and did not implement Milestone 9 scope.

## UI root cause

The defect was structural rather than normal appearance:

1. Buttons stored meaningful labels while `WithShowLabel(false)` explicitly
   prevented pinned iPlug2 from painting them.
2. Legacy Settings and Configuration attached overlapping categories as
   simultaneous siblings with independent hard-coded bands and no page owner.
3. Hiding a page container alone was insufficient: overlay animation
   completion recursively exposed its descendants again.
4. Main rows and long status strings were not allocated by one bounded layout.
5. At fractional Windows DPI, graphics scaled while the standalone native
   parent/iPlug child remained physically 600 x 400, clipping the painted right
   and bottom edges.
6. The main hosted IR action was a no-op, the NAM action did not begin the live
   coordinator, and provider actions were not completely wired.
7. The hosted selector rendered `Close` for terminal Cancelled/Failed states,
   but its mouse handler recognized that rectangle only during the
   browser-waiting state. The visible modal consequently remained a topmost
   input surface after cancellation.
8. `IButtonControlBase` sets a button value to `1` on mouse down and relies on
   `DefaultClickActionFunc` to schedule the animation that returns it to `0`.
   Direct application callbacks did not invoke that default action.

## Main editor

Before the patch, the two blue TONE3000 rectangles had internal captions but
did not paint them, the lower/right content could clip at fractional DPI, and
the hosted actions did not both enter the selection coordinator. The baseline
source/paint path and diagnostic old binary reproduced the defect; the initial
automation capture could not reliably retain the transient old child surface,
so it is not presented as a successful baseline visual pass.

After the patch, `EditorLayout.h` supplies bounded named rectangles for all six
knobs (Input, Threshold, Bass, Middle, Treble, Output), meters, model/IR
selectors and local controls, `Search TONE3000`, `Find IR on TONE3000`, and
Settings. Captions paint in normal and disabled states. The hosted NAM action
uses the architecture choice and `select_tone`; the IR action uses the hosted
IR flow. Neither calls `/tones/search` or enters `ProcessBlock()`.

The real Release standalone was inspected on Windows at actual 125% display
scaling. Its logical editor remained 600 x 400; the measured iPlug child was
750 x 500 physical pixels and the outer native window was approximately
764 x 558. All essential controls remained visible and readable. Central
layout tests exercised the 100%, 125%, 150%, and 200% calculation matrix
without claiming manual inspection at scales that were not manually used.

Ignored local visual evidence includes:

- `artifacts/post-m8-ui-release-main-125-complete.png`;
- `artifacts/post-m8-ui-settings-*-125.png`;
- `artifacts/post-m8-ui-settings-local-files-final-125.png`;
- `artifacts/post-m8-ui-settings-managed-library-final-125.png`;
- `artifacts/post-m8-ui-settings-tone3000-final-125.png`.

Screenshots are not committed and contain no provider account identity.

## Settings/configuration

One native overlay now provides General, Local Files, Managed Library,
TONE3000, Calibration, Diagnostics, and About navigation. A centralized shell
owns header, navigation, content, footer, close, row, gap, status, and button
metrics. Only the active page and its descendants are visible, enabled, and
hit-testable; inactive descendants are hidden and disabled again after overlay
animation completes.

Local folder, library, and provider status lines are deliberately bounded.
About cannot coexist visually with provider, library, or calibration content.
The earlier duplicated Configuration overlay is no longer attached. One tagged
Settings container exists before opening and the same one remains after
repeated open/close (1 before, 1 after); switches change state and attach no
new control tree. Real-window cycling of all seven pages showed no duplicate
or layered page.

Standalone General points to File > Audio Settings. VST3 uses the same tested
layout but shows the bounded statement that its DAW controls devices, routing,
sample rate, and buffer size; it exposes no standalone device controls.
REAPER was not available during this patch, so VST3 host visual inspection is
reported as host-dependent and skipped rather than inferred from the shared
code.

## TONE3000 configuration

- Registration state: `DevelopmentConfigured`.
- Client type: public desktop client.
- Public client: present once in centralized product configuration
  (`t3k_pub_…`).
- Client secret: none; no `t3k_cs` value is used.
- OAuth: Authorization Code with PKCE S256 in the external system browser.
- Redirect URI: `http://127.0.0.1:43821/oauth/callback`.
- Scheme: `http`.
- Host/bind address: `127.0.0.1`.
- Port: `43821`.
- Path: `/oauth/callback`.
- Callback strategy: fixed IPv4 loopback.

The TONE3000 page shows Development registration, Fixed local callback,
Disconnected or Connected-as identity state, and Connect, Disconnect, Test
Connection, Cancel Current Operation, and Open TONE3000 actions. It exposes no
client secret, token, authorization code, verifier, or signed model URL.

## Port behavior

`FixedLoopbackCallbackReceiver` validates the exact registration, creates the
actual listener, applies exclusive-address semantics, binds only
`127.0.0.1:43821`, listens, and retains ownership before the coordinator may
open a browser. There is no separate probe-and-close race and no alternate
port/address/path.

If the bind reports an existing owner, Amphibia opens the required clear
Retry/Cancel dialog and does not open a browser. Retry creates a fresh attempt
after the conflict is removed. Cancel, successful terminal callback, failure,
deadline, supersession, and shutdown close the listener. The cross-instance
hosted-selection lease still prevents concurrent Amphibia browser flows.

A real UI conflict exercise held only `127.0.0.1:43821`: the dialog appeared,
the browser process count remained unchanged, and releasing the test listener
allowed a subsequent bind. Native tests independently cover conflict,
successful single receive, cancellation, timeout, shutdown, wrong path,
oversized input, repeated start, and immediate port reuse.

## Live provider validation

### Attempted

The Release standalone TONE3000 page and main hosted NAM button were invoked
with the development public configuration. Before the explicit action the port
was free. After the action, a listener existed on `127.0.0.1:43821` and its
owning PID matched Amphibia, demonstrating listener readiness before the
browser boundary. Cancelling removed the listener and an immediate exclusive
rebind succeeded.

### Passed

- development registration displayed;
- fixed callback displayed;
- explicit hosted action reached the real listener/browser boundary;
- exact port ownership preceded browser authorization;
- cancellation released the port;
- port-conflict UI suppressed browser launch.

### Failed

No completed validation step produced a security or functional failure. The
first packaging invocation encountered a sandbox-only MSBuild FileTracker
access denial; the same repository-scoped pipeline passed outside the sandbox.

### Blocked or not attempted

Interactive account authorization/selection was not completed. Therefore code
callback, PKCE token exchange, current-user identity, exact tone metadata,
exact model list, explicit provider download, managed promotion,
load/audition, offline restart/restore, disconnect credential deletion with
local-content retention, and a legal live IR selection were not live-validated.
No account identity, credential, callback value, or provider download was
captured or committed.

### Mock-only

Token exchange/refresh, current user, exact tone/models, provider download,
promotion, association/provenance, activation handoff, stale callbacks,
wrong-state handling, and cross-process contention retain deterministic
mock/native coverage. These results are not counted as live provider success.

## Security

The receiver accepts one bounded HTTP/1.1 GET only for the exact Host/path from
a loopback peer. It caps request line and headers, rejects malformed/oversized
input, and delegates duplicate/missing/malformed query, exact state,
single-use, stale, superseded, cancellation, and deadline checks to the
existing parser/controller. Its local response says only that authorization
was received and includes no-store behavior.

Access and refresh tokens remain only in the isolated OS credential service
with no plaintext/settings/state fallback. The public client ID is not stored
there. Construction, scan, state restore, Settings opening, and audio
processing do not bind, access credentials, open a browser, or perform
provider/network work. The rebuilt release security scan passed 623 files with
zero findings. It accepts the expected public `t3k_pub_` configuration while
continuing to reject private-client, token, private-key, authorization-code,
signed-URL, private-path, and credential patterns.

## Tests

### Passed automated

- identity validation;
- inherited characterization;
- state compatibility;
- Milestone 6 static contract;
- post-M8 UI/TONE3000 static contract;
- Milestone 7 installer and packaging contracts;
- Milestone 8 release-readiness contract;
- Milestone 2 Release native suite (10 groups);
- Milestone 3 Release library suite;
- Milestone 4 Release foundation/layout/loopback suite (26/26);
- Milestone 5 Release hosted-selection suite (9/9);
- follow-up Milestone 5 Debug hosted-selection suite (10/10), including the
  shared terminal-panel Close-state regression;
- Milestone 6 Release setup suite (7/7);
- CycloneDX/asset/pin validation (51 components, 33 assets);
- release credential/path scan (623 files, zero findings).

`pytest` was not installed; the repository's Python test entry points were run
directly and passed.

### Manual

The real Windows Release standalone main surface and all seven Settings pages
were inspected at 125%. The real port-conflict dialog and no-browser behavior
were inspected. The waiting/listener boundary and cancellation release were
inspected without completing the provider account flow.

For this follow-up, the real Debug standalone host and its 600 x 400 iPlug
editor child were located and direct Windows input messages were sent to that
target. The automation session's compositor could not produce a trustworthy
capture of the GPU child surface, so the Cancel -> Close -> Settings visual
sequence is not counted as an additional visual manual pass; the native
terminal-state regression covers the dispatch contract deterministically.

### Failed

None in the completed test matrix.

### Skipped or host-dependent

- REAPER/VST3 runtime visual pass: REAPER unavailable;
- completed live provider account/content flow: blocked as recorded above;
- macOS: not built or claimed;
- AAX: intentionally excluded;
- Authenticode: no signing credentials, intentionally unsigned;
- Milestone 8 isolated Windows install/DAW/audio/coexistence matrices:
  still blocked;
- external/current C++ vulnerability metadata review: still blocked.

Mock-only provider results are separated in the live-validation section.

## Builds and artifacts

Debug standalone, Release standalone, Debug VST3, and Release VST3 all built
successfully with the v145 x64 toolchain. Existing inherited conversion
warnings remain; there were no build errors. AAX was not enabled.

The input-dispatch follow-up rebuilt the Debug standalone and Release VST3
targets successfully with the same v145 x64 toolchain. It intentionally did
not regenerate installers, portable ZIPs, SBOMs, scans, or checksum manifests.
Accordingly, the package hashes below remain historical evidence for the prior
package-source commit and must not be used to represent this follow-up source
commit; a release candidate must rerun the package and security evidence.

### Prior package-evidence run (historical)

The Windows packaging pipeline rebuilt Release standalone/VST3 and regenerated:

- `Amphibia-0.1.0-Windows-Setup.exe`;
- standalone, VST3, and portable ZIPs;
- CycloneDX SBOM and compiled-asset provenance;
- binary inspection and release-security scan;
- source, dependency, artifact, and checksum manifests.

All outputs are under ignored `artifacts/release/0.1.0` and are not committed,
published, uploaded, tagged, or called stable. They are unsigned. The final
package/evidence source commit is
`b69dd56a3a77784a001249278bc6f0ba66980a58`. The canonical checksum file is
`artifacts/release/0.1.0/SHA256SUMS.txt`; its final SHA-256 values are:

| File | SHA-256 |
| --- | --- |
| `Amphibia-0.1.0-Windows-Setup.exe` | `dd12bca12c4e444fec818408278cbfc6290ccadb5dcb2089205562bb12eaba57` |
| `Amphibia-0.1.0-Windows-Portable.zip` | `e6d2b04cf75e166fd4dbec3a9575a246fdb85e65ad62717af74f27316ee584b5` |
| `Amphibia-0.1.0-Windows-VST3.zip` | `51554e7b50274014e419371776216a60e5ca025b33b6a3e245e711208151ac5d` |
| `Amphibia-0.1.0-Windows-Standalone.zip` | `22828df08a417d9ecb8bc7ab27e90f9a714304376edd6c809394a8d5640a3859` |
| `SOURCE-COMMIT.txt` | `f4490074beac6fa6014d24d5671bf15c3f95ad1403b34762efd8cc7cd1143769` |
| `DEPENDENCY-VERSIONS.txt` | `d56a68afc6826f3913b1ca2b24bf04a99c67e7e83f3148e70a7344ad07ae1713` |
| `THIRD_PARTY_NOTICES.txt` | `907503544adb1874ac8c47b02ca70eca734bdda32948d1e52c9633e2147af6c7` |
| `Amphibia-0.1.0-SBOM.cdx.json` | `027d945061e99072cc748b9f353a6c64e2d4338c16a4f5eb916f2dfb5980e131` |
| `ASSET-PROVENANCE.json` | `12dbf349c79acc8ac8df9ac6691e80633e0b51a0d010ed7a040f21716c03b4e2` |
| `RELEASE-SECURITY-SCAN.json` | `744e5798dffae8b3cfa21706b167e29181515ed67650693d0a404192fe229acc` |
| `BINARY-INSPECTION.txt` | `78d469c9d4c480f1eebea9e8628a72c94f302b2e80c589727f7a4902c6d8ca9b` |
| `ARTIFACT-MANIFEST.txt` | `994f140dc525cbc91c678071105c17f5a39842b88702364cf44b54b9f17b7984` |

The existing public-release blockers remain: inherited compiled UI artwork
provenance, legacy ASIO authority, current external vulnerability metadata,
isolated/manual Windows install/DAW/audio/coexistence matrices, protected
signing/publication, production TONE3000 registration/branding and complete
live validation, macOS, and canonical support/security contacts.

## Future Milestone 9 compatibility

No Milestone 9 feature or acceptance criterion was implemented or marked
complete, and no roadmap milestone was renumbered. Provider-neutral interfaces
remain extensible, the centralized Settings section model can accept future
sections, and no temporary fallback port, callback scheme, or duplicate UI
architecture was introduced. No speculative Milestone 9 type, placeholder,
report, marker, tag, or artifact exists.

## Git

The final audit uses:

```powershell
git status
git branch --show-current
git remote -v
git log --oneline -n 40
git submodule status --recursive
```

Branch: `main`.

Remotes:

- `origin`: `https://github.com/atoontoto/Amphibia.git`;
- `upstream`: `https://github.com/sdatkinson/NeuralAmpModelerPlugin.git`.

Corrective local commits through the final package source:

- `9311b48` — `docs: map post-M8 UI and TONE3000 corrective patch`;
- `eee634e` — `fix: repair UI and enable public TONE3000 OAuth`;
- `cf10b1f` — `test: cover UI layout loopback callback and port conflicts`;
- `0238af6` — `fix: complete DPI and settings visibility repair`.
- `b69dd56` — `docs: close post-M8 UI and TONE3000 patch report`.

The prior final hash-record commit is `289ac58` (`build: record final unsigned
candidate hashes`).

The final hash-record commit changes only this report after package generation
and is necessarily identified by the final `git log` after it is committed.
The worktree and recursive submodule state are checked again after that commit.
The pinned iPlug2 submodule remains at
`66f9060b5287afdb2ddb83aab5a2a2a8e66a4d5e`; no submodule change is part of
this patch.

Git push was not performed. The repository owner will push manually.
