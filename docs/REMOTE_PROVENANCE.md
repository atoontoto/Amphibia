# Remote content provenance

Status: Milestone 4 schema-2 contract

Remote bytes are physical library objects identified only by their local SHA-256. Provider identity is a separate association. This lets the same bytes arrive from a local file, two provider models, or several tones without duplicating storage or losing attribution.

## Stored association

`metadata/index-v2.json` records a provider association containing:

- provider kind and raw string tone/model IDs;
- tone and exact model titles;
- creator user ID, username, and optional display/profile information;
- provider-supplied license ID/name without permissive inference;
- canonical source page without query or fragment;
- format, gear role, and NAM architecture, including unknown values;
- verification/import timestamps and metadata version; and
- the lowercase SHA-256 of the promoted managed object.

The complete `model_url`, signature/query parameters, bearer token, OAuth material, credential identifier/payload, request body, and callback URI are never stored. Provider IDs are not filenames and do not choose the object path.

## Promotion flow

```text
explicit individual model_url
  -> randomized managed .part staging
  -> streamed declared/actual byte limits and free-space policy
  -> flush/close and Milestone 2 NAM/WAV inspection
  -> Milestone 3 SHA-256 and content-addressed promotion
  -> atomic schema-2 provider association commit
  -> optional later explicit _RequestModel()/_RequestIR()
```

The coordinator never activates DSP. Failed, cancelled, truncated, oversized, invalid, or stale downloads remove their staging part on a best-effort basis and create no association. Successful duplicate bytes reuse the object and may add another association.

## Migration and lifecycle

Schema 1 opens read-only, converts in memory with an empty provider-association list, and atomically publishes `index-v2.json`. Existing hashes, paths, objects, packs, entries, revisions, and classifications are preserved; `index-v1.json` remains rollback evidence. A failed schema-2 write leaves schema 1 authoritative. Future schemas are rejected without modification.

Disconnect deletes the OS credential and cached account identity, not downloaded objects or their attribution. Removing local downloaded content is a separate explicit destructive operation. State 1.1 continues to store only the managed SHA-256; it can resolve local bytes offline with no credential or provider request.

## Presentation requirement

Milestone 5 surfaces the retained creator, license, provider identity, and source link in the exact-variant surface. The repository build uses neutral text because approved production brand assets are unavailable. Unknown or missing license metadata is shown as unknown/missing, never rewritten as unrestricted.

## Exact cache identity

Cache recognition requires the exact tuple `(tone3000, tone_id, model_id)` and returns the associated local SHA-256. Filename, title, or URL similarity never counts as a match. An explicit download refreshes exact-model metadata so a stale or substituted ID/format/architecture fails before bytes are accepted.

The panel presents bounded plain-text tone title, creator username, description, gear, format, tags/license when available, source action, exact variant name, architecture, declared bytes when documented, grouping path, compatibility, download state, and active state. Provider model IDs remain a diagnostic identity rather than a marketing label. Remote image/avatar URLs are preserved but not fetched by the initial implementation.
