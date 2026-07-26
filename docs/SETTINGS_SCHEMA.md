# Amphibia settings schema

Windows stores Amphibia-owned settings at
`%LOCALAPPDATA%\Amphibia\settings-v1.json`. The document and nested setup state
are independently versioned at 1.

```json
{
  "schema_version": 1,
  "shared": {
    "nam_folder": "optional local path",
    "ir_folder": "optional local path"
  },
  "standalone": {
    "audio": {
      "backend_id": "asio",
      "backend_name": "ASIO",
      "input_device_id": "backend-scoped ID or empty",
      "input_device_name": "local display name",
      "output_device_id": "backend-scoped ID",
      "output_device_name": "local display name",
      "input_channel": 1,
      "output_first_channel": 1,
      "output_channel_count": 2,
      "sample_rate": 48000,
      "buffer_size": 256
    },
    "setup": {
      "schema_version": 1,
      "completion": "never_started | in_progress | completed | skipped",
      "audio_configured": true,
      "nam_folder_configured": false,
      "ir_folder_configured": false,
      "library_validated": false,
      "tone3000_configured": false,
      "last_completed_app_version": "0.1.0",
      "completed_at_unix_seconds": 0
    }
  }
}
```

Strings and numeric fields are bounded on read. Future document/setup schemas
fail closed and remain untouched. A corrupt primary uses a validated `.bak`
when available; otherwise the app uses safe in-memory defaults and reports the
failure. An active legacy `settings.ini` audio stream seeds a missing JSON
document for review; Finish performs the durable migration.

Writes use a random sibling temporary file, flush, replace, and validated
backup. A bounded cross-process lock protects reload/merge/write. This is
crash-resilient at the file replacement boundary, not a distributed database.

The serializer rejects sensitive provider field names including access/refresh
tokens, authorization codes, PKCE/code verifier material, signed/model URLs,
and client secrets. Credentials remain exclusively in the OS secure store. DAW
project state remains the separate state 1.1 format in `STATE_FORMAT.md`.
