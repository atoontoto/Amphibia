# Amphibia state format

Status: Milestone 2 compatibility record

## Current writer

Milestone 2 does not change the Milestone 1 payload:

```text
IByteChunk string  "###Amphibia###"
IByteChunk string  "1.0.0"
IByteChunk string  active local NAM path (or empty)
IByteChunk string  active local IR path (or empty)
double[13]         parameters in EParams order
```

Strings use iPlug's length-prefixed `IByteChunk` representation. The 13
parameters and their order are recorded in `DSP_CHARACTERIZATION.md`. The
writer serializes only paths committed after a successful audio-boundary
activation. A requested, failed, cancelled, stale, or merely ready path is not
serialized.

No provider reference, model bytes, IR bytes, hash, secret, OAuth value, URL,
or state-schema addition is present.

## Readers

Reader dispatch remains:

1. `###Amphibia###` plus a known/newer compatible version layout.
2. `###NeuralAmpModeler###` plus the inherited version migrations.
3. Headerless inherited NAM state through the pre-0.7.9 fallback.

The inherited 0.7.9, 0.7.10, 0.7.12, and 0.7.14 parameter migrations remain.
`Slim` and calibration defaults continue to be synthesized for older layouts.
Unknown newer version strings continue to use the latest compatible inherited
payload reader because the payload has not changed.

Milestone 2 adds bounds/error handling around state strings, parameter reads,
and version conversion. A corrupt, negative-length, truncated, or otherwise
unreadable chunk returns `-1` to the host rather than throwing through the host
callback. Current active DSP is not modified on such a failure.

## Restoration timing

Parameters restore synchronously and `OnParamReset()` runs first. Non-empty
model and IR paths are then submitted independently to the same asynchronous
workers as file dialogs and previous/next navigation. Project loading therefore
does not wait for Core construction, WAV decode, resampling, or convolution
preparation.

Until successful activation, a new instance uses passthrough for the missing
stage; an existing instance retains its prior compatible active stage. Missing,
invalid, or stale restored files fail without replacing that stage. UI objects
are not accessed from restoration or worker code; a later `OnIdle()`/UI open
reflects current status.

Empty restored paths retain the inherited behavior: no new load is submitted.
Milestone 3 may define explicit empty-reference clearing as part of its richer
state schema only with a compatibility test and migration decision.
