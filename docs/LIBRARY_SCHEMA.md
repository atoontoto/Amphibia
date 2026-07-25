# Managed Library Schema

The durable metadata format is schema version `2`. `metadata/schema-version` is human-readable; `metadata/index-v2.json` is authoritative after migration. A document with a future schema is rejected without modification. Malformed metadata is rejected, and an existing `.bak` remains available for manual recovery.

## Document model

The root stores `schema_version`, monotonically increasing `revision`, `objects`, `packs`, `pack_entries`, `provider_associations`, and `user_classifications`.

- Object key: lowercase 64-character SHA-256. Values store type, library-relative object path, byte size, original display name, source mode, optional NAM architecture/capture role or WAV properties, import/last-use times, validation version, and pinned state.
- Pack key: 128 bits from the OS cryptographic random source, encoded as lowercase hex. Values store source mode, optional source archive hash/path, display name, detected creator/license/description, and import time.
- Pack entry: pack key, object hash, normalized nested relative path, display name, type, and stable sort key. Associations, rather than a manually maintained reference count, determine liveness.
- User classification: object hash to explicit capture role. User data is separate from detected metadata.
- Provider association: provider/tone/model identity, tone and model titles,
  creator identity, unknown-preserving license fields, source page, format,
  gear, architecture, verification times, metadata version, and the Amphibia
  object SHA-256. These are associations, not physical object identities.

JSON objects are serialized with nlohmann/json. Metadata strings are plain text; HTML is neither stored as trusted markup nor rendered.

## Transactions

Writers take an in-process mutex and an OS file lock (`LockFileEx` or `flock`). A new snapshot is written to a random temporary file in `metadata`, flushed, and atomically replaces the index with a backup. Pack import publishes validated content-addressed objects before one metadata snapshot publishes the pack and all associations. Deduplicated existing objects are revalidated and never removed on rollback. Pack removal atomically removes its metadata associations; object cleanup is a separate association-derived operation.

Object publication uses a randomized staging directory, create-new semantics, no-follow/reparse checks, a verified second hash, and same-volume rename. If a crash happens after object publication but before metadata commit, verification reports an orphan. If it happens after metadata commit but before unused-file deletion, verification also reports an orphan. Neither case destroys user source content.

## Schema-1 migration

Opening a schema-1 library reads `index-v1.json` without modifying it, builds
an in-memory schema-2 snapshot with an empty provider-association collection,
and atomically publishes a separate `index-v2.json`. Existing objects, hashes,
paths, packs, entries, revisions, and user classifications are unchanged.
`index-v1.json` remains available as rollback evidence. If writing schema 2
fails, schema 1 remains authoritative on the next open. Once a valid schema-2
index exists it is preferred; future schemas are still rejected untouched.

## Verification

Verification checks schema parsing, object path containment, existence, size, bounded NAM/WAV inspection, optional SHA-256, pack/object association integrity, and unrecorded files beneath object roots. It runs on the import worker, supports cooperative cancellation, and does not repair automatically.
