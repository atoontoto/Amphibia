# Standalone audio configuration

Audio-device configuration belongs only to the standalone application. A DAW
owns VST3 devices, routing, sample rate, buffer size, and callback lifecycle.

On Windows Amphibia enumerates RtAudio DirectSound and ASIO backends. ASIO is
preferred when a usable driver exists; Amphibia does not bundle or download a
driver. Device lists retain backend-scoped IDs even when two devices have the
same display name. The wizard exposes an optional mono guitar input (including
an explicit output-only choice), the output device and stereo pair, the
intersection of reported sample rates, and reported/fixed buffer choices.

## Safe changes

Every Apply is a transaction:

1. validate backend, device IDs, channels, rate, and buffer;
2. stop any test tone;
3. attempt the requested stream;
4. commit only when the callback and output are active;
5. otherwise reopen the previous known-good configuration; and
6. if neither opens, close audio and offer an explicit no-audio continuation.

The selected configuration is written only when setup finishes. The legacy INI
remains the iPlug APP compatibility store; `settings-v1.json` is authoritative
for setup state and the Amphibia-owned configuration projection.

## Input and output test

The output test never starts from focus, page entry, or device selection. It
requires a warning confirmation and explicit button press. The 440 Hz signal is
limited to a 0.05 linear peak, fades in/out, runs for at most two seconds by
default (hard maximum five), does not stack, and stops on page exit, Cancel,
device change, or host shutdown. Only its preconfigured oscillator runs in the
audio callback.

Input/output meters exchange lock-free peak values. They do not allocate,
record, store, log, serialize, or upload samples.

## Recovery

Use Audio Settings or `--audio-settings` when a saved device disappears. Refresh
re-enumerates devices. Unsupported rates/buffers and unavailable input/output
devices are reported before or during the transactional open. Safe mode can be
used when content restoration, rather than audio enumeration, is suspected.

Automated audio tests use fakes and do not count as physical-interface
validation. No real interface, speaker test, input meter, or unplug/replug
sequence was manually validated for this milestone.
