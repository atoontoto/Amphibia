# Amphibia state format

Status: Milestone 3 compatibility record

## Version 1.0 compatibility prefix

Milestone 3 preserves the complete Milestone 1/2 payload as the prefix of every current state:

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

The compatibility prefix contains no provider reference, model bytes, IR bytes,
secret, OAuth value, URL, or schema addition. Managed identities exist only in
the bounded optional 1.1 tail documented below.

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

For a 1.0 state, empty restored paths retain inherited behavior: no new load is
submitted. A 1.1 optional tail can explicitly classify a slot as `clear` while
leaving the frozen prefix readable.
# Milestone 3 managed local references

The current Amphibia state-layout version is `1.1.0`. The frozen header remains `###Amphibia###`; the legacy `###NeuralAmpModeler###` and headerless readers remain unchanged.

Version 1.1 writes the complete 1.0 payload first, in the same order: header, layout version, active NAM path, active IR path, and serialized parameters. It then appends two length-prefixed strings:

```text
###AmphibiaLocalReferences###
{"schema":1,"model":{"kind":"managed","sha256":"..."},"ir":{"kind":"referenced"}}
```

Each slot is `managed`, `referenced`, or `clear`. Managed values contain only a lowercase SHA-256 identity; referenced paths remain in the inherited prefix. The JSON tail is limited to 64 KiB and unknown fields are ignored under schema 1. Readers that stop after parameters can ignore the tail. Existing Amphibia 1.0 and legacy NAM state has no tail and follows the existing path restoration route.

On 1.1 restore, parameters and inherited paths are decoded first. A managed hash, when present, is resolved under the managed root and submitted asynchronously through `_RequestModel()` or `_RequestIR()`, superseding the provisional path request. A `clear` kind submits the matching asynchronous clear request. A missing hash/object produces no synchronous mutation; normal async failure behavior preserves active DSP. Because the extension is optional, an invalid, oversized, or truncated tail is ignored after the valid 1.0 compatibility prefix has restored; corruption in the required prefix still returns `-1`.
