# Amphibia threading contract

Status: Milestone 3 implementation

## Threads and responsibilities

| Context | Permitted work | Prohibited work |
|---|---|---|
| UI/message or host state callback | Submit/cancel requests, copy status, serialize active paths, update controls | NAM/IR construction, direct worker-buffer access |
| Host reset callback | Capture/increment processing configuration, reserve plug-in buffers, submit re-preparation | Reset active model/IR in place, wait for workers |
| NAM worker | File preflight, JSON parse, A2 inspection, Core construction, resampler reset/prewarm, stale/retired NAM destruction | UI control mutation, audio-buffer processing after publication |
| IR worker | RIFF preflight, WAV decode, cubic resampling, weight/history/output preparation, stale/retired IR destruction | UI control mutation, audio-buffer processing after publication |
| Audio callback | Fixed-size request/generation validation, pointer handoff, parameter DSP, audio processing, atomic diagnostics | File I/O, parsing, construction, mutex/wait, logging, string formatting, DSP destruction |
| Plug-in destruction thread | Invalidate, wake and join workers; destroy final active processors after callbacks stop | Detach workers or permit callbacks into a destroyed owner |

## Request state

Model and IR expose copied `LoadStatus` values with `Idle`, `Inspecting`,
`Preparing`, `ReadyToActivate`, `Active`, `Failed`, and `Cancelled`. Status
strings live behind the worker mutex and are read only from `OnIdle()` or other
non-audio callers. The audio callback never reads them.

The UI and state reader submit through the same path. `OnIdle()` converts a
new status snapshot into framework control messages. Workers never call iPlug
controls. Loading and failure labels include requested basename and active
basename; full personal paths are not placed in error text.

## Locks

`mProcessingConfigurationMutex` serializes reset with UI/state submission.
`mPathMutex` protects active paths used by serialization/UI. Each worker mutex
protects pending request, status strings, and non-audio metadata. All are
outside `ProcessBlock()`.

`ProcessBlock()` and everything it calls in the new loading bridge acquire no
mutex and wait on no condition variable. NAM Core's version registry mutex is
reachable only from the NAM worker's `nam::get_dsp()` call.

Host/UI Slim changes increment only an atomic revision in `OnParamChange()`.
`OnIdle()` observes that revision and submits a full worker-prepared model
replacement using the new Slim value. Core `SetSlimmableSize()` is explicitly
not real-time safe for all model variants, so it is never invoked from
`ProcessBlock()`. This is a compatible re-preparation: the old model remains
active until success, unlike sample-rate reconfiguration, which must bypass an
incompatible object.

## Configuration and buffers

Every preparation captures `ProcessingConfiguration`. Reset increments its
generation before re-preparation. The worker and audio boundary both reject a
generation mismatch.

The plug-in input/output vectors are grown to the advertised maximum in
`OnReset()` and no longer shrink for smaller variable blocks. A host providing
more than its advertised maximum can still trigger one defensive grow in
`ProcessBlock()`. Inherited noise gate, tone stack, filter, meter, and DSP base
buffer helpers may resize vector lengths as block sizes vary; capacity normally
prevents allocation after the maximum-size preparation, but these general DSP
paths are not instrumented with a global allocator trap in this milestone.

## Cancellation meanings

- User cancellation: `Cancel()`, status `Cancelled`, never activates.
- Superseded: current request ID changed; discarded silently off-thread.
- Stale processing configuration: generation mismatch; discarded off-thread
  or reported if preparation began without a valid configuration.
- Shutdown: stop flag plus request invalidation; worker joins after the current
  cooperative boundary/Core call.
- Failure: structured `LoadErrorCode` plus concise message; active DSP/path is
  unchanged.

The model/IR loader UI does not expose a separate cancel button. Its cancellation
API remains exercised by tests. Local-library review can cancel its independent
import task before publication.

## Diagnostics and tests

The bridge exposes peak retired depth without formatting on the callback. Unit
tests use fake DSP lifetime counters and thread IDs to prove preparation and
retired destruction do not run on the simulated audio thread. They also time
the activation call externally. No external telemetry or release-time logging
was added.

ThreadSanitizer was not claimed on Windows. The available MSVC build and race
stress suite were used; ASan/UBSan and macOS/Linux TSan remain platform matrix
work.
# Milestone 3 import/library worker

Each plugin instance may own one `ImportWorker`, separate from the model and IR preparation workers. It serializes scan, review-selected import, verify, staging cleanup, and unused cleanup tasks. New task IDs supersede pending work; cancellation is cooperative; destruction cancels and joins. It never detaches and is explicitly shut down before library ownership ends.

Folder traversal, ZIP enumeration/extraction, SHA-256, validation, copying, JSON index access, free-space checks, verification, and cleanup run only on this worker (startup initialization is a constructor-time non-audio exception). UI callbacks submit immutable sources/selections and poll copied progress/results from `OnIdle()`. No import worker calls `ProcessBlock()` or accesses an audio processor.

Managed activation resolves a path and submits it through the existing Milestone 2 request gateway. Object `last_used_at` and active managed state change only after the DSP worker reports Active. Model/IR preparation, block-boundary pointer exchange, and old-processor reclamation retain the Milestone 2 ownership model.

# Milestone 4 provider workers

OAuth/controller state, platform HTTP request workers, credential operations,
and model downloads are independent of the NAM, IR, and local-import workers.
WinHTTP owns one joined worker per live request; a long download therefore does
not serialize token refresh or local import. Download completion performs
bounded inspection, SHA-256, and `ManagedLibrary` promotion on that request
worker. Provider task generations and weak shared state prevent stale or late
callbacks from mutating a destroyed adapter or a newer operation.

The provider is inert at plugin construction and state restoration. Browser
launch, OAuth, credential access, DNS, TLS, HTTP, retry waits, JSON parsing,
downloads, hashing, library promotion, refresh, and provider diagnostics are
not referenced by `ProcessBlock()`. Controls poll immutable/copy status from
the message thread; provider workers never touch iPlug controls.

Shutdown order is: reject new provider work, invalidate OAuth, cancel the
current download and native requests, suppress callbacks, join transport
workers, release provider/transport, then release the managed library and DSP
workers. Native DNS/TLS can take until its finite configured timeout; no worker
is detached and shutdown is not described as instantaneous.

# Milestone 5 hosted selection ownership

`Tone3000HostedSelectionCoordinator` is a non-audio state machine. It stores copied bounded snapshots behind its own mutex and uses task IDs plus weak shared state to reject stale callback, metadata, exact-model, download, and activation completions. It never calls iPlug controls. The message thread may copy a snapshot and update/reconstruct the native panel, including after an editor closes and reopens.

The callback receiver starts before browser launch and is cancelled before exact API work. A named semaphore on Windows and `flock` lease on macOS ensure only one Amphibia instance owns a hosted browser/callback flow for the credential namespace. The semaphore is intentionally releasable from an asynchronous completion thread. Credential refresh retains its separate Milestone 4 lock.

Download completion may request managed activation, but the coordinator does not construct or exchange DSP. The product-side callback must resolve the managed SHA-256 and call `_RequestModel()` or `_RequestIR()`; only a later Milestone 2 Active/Failed result may call `NotifyActivation`. Downloaded, Preparing, and Active are therefore independent states. The existing active DSP remains in place throughout failed or cancelled provider work.

# Milestone 6 setup and settings ownership

The Windows wizard, command-line policy, device enumeration/open/close, folder
validation/counting, library verification/import/cleanup, provider actions,
settings load/merge/write, and diagnostics formatting run on the startup/message
thread or their joined workers. None is called by `ProcessBlock()`.

The APP callback performs only existing audio processing, lock-free peak
publication, underrun accounting, silent-input routing for output-only mode,
and the already-configured bounded test oscillator. Test start/stop parameters
are published atomically; there is no callback file I/O, parsing, UI access, or
settings lock.

Shared preferences are read on `OnUIOpen()` and explicit panel actions only.
The settings write lock is bounded and never acquired from audio code. Folder
validation uses an asynchronous same-directory scan. Managed library work uses
the existing joined `ImportWorker`; UI timers copy its status. Closing the
wizard can cancel its current library task, while navigating forward may leave
the bounded task running.

# Post-Milestone-8 callback and section ownership

`FixedLoopbackCallbackReceiver` owns one joined receive worker and a retained
listener socket. `Start()` validates and binds synchronously so the hosted
coordinator can prove readiness before browser launch. The worker accepts and
parses one bounded request; `Cancel()`, timeout, terminal completion, and
`Shutdown()` close the listener and suppress stale completion. Socket work,
OAuth state, token exchange, provider metadata, and browser launch remain
outside `ProcessBlock()`.

The message thread owns Settings section selection. The overlay's animation
completion reapplies visibility and enablement for every descendant so an
inactive page cannot paint or receive input. Section switches do not attach
new controls; repeated open/close reuses the original control tree.
