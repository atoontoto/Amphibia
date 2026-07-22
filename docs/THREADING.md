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
