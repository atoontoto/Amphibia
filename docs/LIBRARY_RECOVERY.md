# Local Library Recovery

Recovery is conservative and confined to Amphibia's managed root.

On initialization Amphibia creates the expected root tree, rejects a symlink/reparse-point root, takes the writer lock, loads schema 1, and removes abandoned direct children of `staging/`. Random staging names are never state or metadata, so this cleanup is safe. Unknown files elsewhere are left alone.

`Verify Library` runs outside the UI and audio threads. It checks recorded objects for containment, existence, expected size, format validity and SHA-256; validates pack associations; and counts unrecorded regular files under `objects/nam` and `objects/ir`. A cancelled verification publishes no repair.

Recovery categories are:

- Missing or corrupt indexed object: report it. Loading by hash fails normally and preserves active DSP.
- Orphaned content-addressed object: report it; do not automatically delete it.
- Abandoned staging: clear automatically at startup or through Clear staging.
- Invalid/future/corrupt metadata: reject it without replacement. Preserve `index-v1.json.bak` for diagnosis/manual restoration.
- Interrupted removal: metadata is authoritative; a remaining file is an orphan and is reported.
- Stale lock file: the file may remain, but the OS lock—not its existence—determines ownership.

`Remove unused` deletes only objects with no pack association, no pinned flag, and no caller-supplied active hash. It never traverses outside the managed object roots and never touches source files or archives. Repairing corrupt metadata, restoring backups, quarantine promotion, and relocation are deliberately manual/future operations.
