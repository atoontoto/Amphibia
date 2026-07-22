# DSP ownership and real-time handoff

Status: Milestone 2 implementation record

Evidence date: 2026-07-22

## Ownership diagram

```text
UI/state/reset caller
      | Submit(path, ProcessingConfiguration, request ID)
      v
+---------------- model worker ----------------+   +---------------- IR worker ----------------+
| newest-only pending request                  |   | newest-only pending request                  |
| inspect -> parse/decode -> construct/prewarm |   | inspect -> decode/resample/preallocate       |
| owns prepared DSP until ready publication    |   | owns prepared DSP until ready publication    |
| reclaims stale and retired DSP               |   | reclaims stale and retired DSP               |
+--------------------+--------------------------+   +--------------------+-------------------------+
                     | one atomic ready pointer                         | one atomic ready pointer
                     v                                                  v
              +------------------ ProcessBlock ----------------------------+
              | compare request ID + processing generation                 |
              | raw-pointer exchange at one block boundary                 |
              | publish old raw pointer in one completion mailbox          |
              +------------------------+------------------------------------+
                                       |
                      worker acquires completion, destroys retired DSP,
                      commits active path/metadata, destroys record
```

There are two instances of `AsyncDSPWorker`: one owns NAM preparation and NAM
reclamation; the other owns IR preparation and IR reclamation. This avoids
model/IR starvation and keeps type-specific destruction on the same dedicated
non-real-time thread that prepares that type. There is no global cache or
mutable global loading state. Every Amphibia instance owns its own two workers.

## Object ownership

- A worker owns a prepared processor through `unique_ptr` while inspecting and
  preparing it.
- Before publication, the worker allocates one `ActivationRecord`, releases
  the prepared processor into that record, and release-publishes the record's
  raw pointer into the one-element ready mailbox.
- At an audio-block boundary, the audio thread acquire-loads the record,
  validates both IDs, and CAS-removes it from ready. It atomically exchanges
  the record's prepared pointer with the stage's active pointer.
- The audio thread writes the former active pointer into the same record and
  release-publishes it into the one-element completion mailbox. It does not
  destroy the processor or record.
- The worker acquire-removes the completed record, deletes the retired
  processor, commits the immutable metadata/path snapshot, and deletes the
  record.
- At plug-in destruction, after callbacks have stopped and both workers have
  joined, the plug-in's non-audio destructor deletes the final active objects.

`shared_ptr` is not used in the callback. A final-reference decrement therefore
cannot run a DSP destructor there.

## Memory ordering

- Worker ready publication is `release`; audio ready load/CAS is
  `acquire`/`acq_rel`. All record fields and fully prepared DSP state happen
  before audio adoption.
- Audio completion publication is `release`; worker completion exchange is
  `acq_rel`. Writes of the retired pointer happen before reclamation.
- Current request ID and processing generation are published/retrieved with
  release/acquire ordering. Both must match the immutable record.
- Active processor exchange is `acq_rel`; compatibility uses release/acquire.
- Pointer atomics are required by `static_assert` to be always lock-free on a
  supported build. Lock-free double atomics are likewise required for the
  model metadata read by audio callbacks.

No correctness argument depends on relaxed ordering. Relaxed ordering is used
only for diagnostics counters and the monotonically allocated request counter,
whose value is release-published separately as the current request.

## Activation and retirement sequence

1. If the completion mailbox is occupied, activation is deferred. Audio keeps
   using the old active processor.
2. Audio acquire-loads ready. A missing record is a no-op.
3. Audio compares `requestId` and `configuration.generation` with the current
   atomics. A stale record remains for worker disposal.
4. Audio CAS-removes the current record.
5. Audio exchanges the stage's active raw pointer, publishes fixed-size model
   metadata when applicable, and marks the stage compatible.
6. Audio stores the old pointer in the record and release-publishes completion.
7. The worker deletes the old pointer and record, commits active path/metadata,
   and changes current status to `Active`.

The callback executes no allocation, deallocation, string operation, file I/O,
parsing, mutex operation, condition-variable operation, or worker notification
as part of this sequence. The completion slot bounds retired depth at one;
audio will not perform another swap until the worker has drained it.

## Cancellation and stale requests

Submission allocates a monotonically increasing request ID and replaces the
single pending request. A running indivisible Core operation may finish, but
its request is checked at inspection/preparation boundaries and again before
publication. A ready record is also checked by the callback. Thus:

```text
submit A -> submit B -> submit C -> A finishes -> C finishes
```

discards A off-thread, never starts overwritten pending B when appropriate,
and permits only C to activate. Supersession is silent in user-facing status.
Explicit cancellation increments the current ID, clears the pending slot, and
reports `Cancelled`. Processing reconfiguration submits a new request with a
new request ID and generation. Any old-generation ready result is ineligible.

## Reconfiguration

`OnReset()` increments the processing generation and captures sample rate,
advertised maximum block size, and connected input/output counts. Active model
and IR pointers remain retained but are marked incompatible, so the affected
stage is temporarily bypassed. The newest pending selection, otherwise the
last active path, is re-prepared. Audio never resets or rebuilds the old object.
Only the matching newly prepared generation re-enables the stage.

This deliberately favors safe passthrough over using an object configured for
the wrong sample rate or maximum block size.

## Shutdown

The plug-in destructor calls `Shutdown()` on both workers before destroying any
state captured by their factories. Shutdown invalidates the current request,
wakes and joins each worker, destroys ready/stale/completed records on that
worker, then returns. Member-destructor calls are idempotent. No thread is
detached and no callback is registered against the UI or owner. The final
active processors are destroyed by the plug-in destruction thread after the
workers have joined and the host has stopped audio callbacks.

Cancellation inside Core parsing/construction is cooperative at call
boundaries; it cannot interrupt a single Core call. Input byte limits and WAV
preflight bound hostile work, but a large valid model can still delay shutdown
until the current Core call returns. This is a real limitation, not a detached
thread or late-callback risk.

## Why use-after-free and unbounded retirement are prevented

Only the audio callback dereferences the active pointer while callbacks are
running. A former active pointer is not published for deletion until after the
audio thread has replaced it at a boundary. The worker cannot delete the new
active pointer because that pointer was nulled in the activation record before
completion publication. A second swap cannot occur while completion is full.
The ready mailbox is also one element and the pending request slot stores only
the newest request. Consequently ready, retired, and pending counts are each
bounded at one per DSP stage under arbitrarily rapid selection.
