# Privacy

The current Milestone 3 code does not implement Amphibia telemetry, analytics, accounts, OAuth, online catalog access, HTTP, or TONE3000 networking. It loads model and impulse-response files selected locally by the user. The inherited standalone host may store local audio/MIDI device preferences under the Amphibia application namespace.

Audio is processed locally. Amphibia does not intentionally transmit audio, model files, impulse responses, filenames, or usage data.

When the user explicitly imports local content, Amphibia stores managed copies, lowercase SHA-256 identifiers, normalized pack-relative paths, display names, file/format properties, pack associations, import/last-use times, and optional manual classifications under the platform application-data directory. Referenced-file mode does not copy the source. Source folders and ZIPs are read-only and are never deleted by library cleanup.

Local diagnostics are limited to standard development output in the current implementation; no persistent task log is emitted. Code is structured to avoid recording model weights, audio samples, file contents, credentials, or unrelated directory listings. The managed `logs/` directory is reserved and empty. Users can clear safe staging remnants and remove unassociated managed objects from Settings; unknown or corrupt data is reported rather than uploaded or silently repaired.

This policy describes the current source baseline, not planned features. Before any online provider is enabled, this document must be updated with exact endpoints, data fields, credential storage, retention/deletion behavior, and third-party policies. Operating systems, DAWs, audio drivers, and separately obtained content providers have their own practices.
