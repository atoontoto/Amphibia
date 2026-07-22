# Amphibia Local Library

Milestone 3 adds an optional, completely local managed library. It does not replace the inherited local browser.

## Source modes

- `ReferencedFile` and `ReferencedFolder` keep user-owned NAM and WAV files in place. Amphibia never copies, moves, renames, or deletes them. The original non-recursive directory menu and previous/next wrapping are unchanged.
- `ManagedImport` copies explicitly selected files or recursively selected folder entries.
- `ManagedArchiveImport` extracts only confirmed ZIP candidates.

Single `.nam` drops on the model slot and single `.wav` drops on the IR slot are referenced loads. Multi-file, folder, or ZIP drops are scanned on the separate import worker and stop at a review confirmation before any copy or extraction. The Settings page provides read-only path/statistics plus Open folder, Verify, Remove unused, Clear staging, Import files, Import folder, and Import ZIP actions. Multiple selection is also available by dropping several files.

## Location and layout

Windows uses `%LOCALAPPDATA%\Amphibia\Library`; macOS uses `~/Library/Application Support/Amphibia/Library`, obtained through iPlug2's application-support abstraction.

```text
Library/
  objects/nam/<first-two-hash-chars>/<sha256>.nam
  objects/ir/<first-two-hash-chars>/<sha256>.wav
  packs/
  metadata/index-v1.json
  metadata/index-v1.json.bak
  metadata/schema-version
  metadata/write.lock
  staging/task-<random>/
  quarantine/
  logs/
```

Object names never use an archive filename. Display names and nested relative paths are metadata. SHA-256 is calculated with Windows CNG or macOS CommonCrypto in bounded streaming buffers. A copied/extracted object is inspected, hashed, copied to staging, inspected and hashed again, then published with same-filesystem rename. Identical bytes share one physical object while pack-entry associations remain separate.

## Loading and state

Every managed NAM and IR resolves to a regular file under the configured root and enters `_RequestModel()` or `_RequestIR()`. Milestone 2 remains responsible for bounded inspection, preparation, newest-request selection, generation checks, block-boundary activation, and non-audio reclamation. `last_used_at` changes only after an Active result.

State 1.1 retains the complete 1.0 prefix and appends optional managed SHA-256 references. Missing or corrupt managed content fails through the normal asynchronous loader and preserves active DSP. Referenced paths remain usable without the library.

## Cleanup and limitations

Startup removes only randomized children of Amphibia's own staging directory. Verify reports missing, invalid, hash-mismatched, outside-root, inconsistent-association, and orphaned files. Remove unused derives liveness from pack associations, honors pinned objects and hashes supplied by active plugin state, commits metadata first, and may leave a reportable orphan after a crash.

The default path is read-only in Milestone 3; relocation is deliberately not partially implemented. Settings shows the three most recently imported/activated display names, but there is no searchable managed tree or one-click recent-item audition list. Imported pack grouping is metadata rather than a separate previous/next navigator. No network access, telemetry, remote images, or provider concepts are present.
