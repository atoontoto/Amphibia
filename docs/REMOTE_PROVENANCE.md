# Remote provider provenance

Status: Milestone 4 durable metadata contract

Remote bytes enter the same content-addressed managed library as local NAM and
IR imports. A provider ID never selects a physical filename or object path.
The validated bytes receive Amphibia's SHA-256, and identical bytes converge
on the existing Milestone 3 object while retaining independent associations.

Each TONE3000 association stores the provider, tone ID, model ID, tone/model
titles, creator ID and username, provider license ID/name, source page URL,
format, gear role, architecture, download and verification times, metadata
version, object SHA-256, and the original provider association key. Unknown
gear, architecture, and license values remain unknown/preserved; Amphibia does
not invent a more permissive license or claim creator ownership.

Complete model download URLs, query signatures, Authorization headers,
access/refresh tokens, authorization codes, PKCE state/verifiers, account
email, and credential-store payloads are forbidden from library metadata.
The promotion API rejects source-page URLs containing query strings or
fragments. Automated tests scan the serialized index for token and signed-URL
canaries.

## Promotion sequence

1. The provider adapter validates the exact tone/model identities and URL.
2. The transport exclusively creates a randomized `payload.part` under the
   managed library staging root and streams with actual-byte limits.
3. After flush/close, the coordinator renames the file to the format-selected
   `.nam` or `.wav` staging name and performs Milestone 2 inspection.
4. Milestone 3 SHA-256 is computed before promotion.
5. `ManagedLibrary::PromoteProviderFile()` revalidates, rehashes its bounded
   copy, publishes/deduplicates the content-addressed object, and atomically
   commits the provider pack/model association in schema 2.
6. Temporary staging is removed. No DSP activation is requested.

Account disconnect removes the secure credential and cached account identity;
it does not silently delete user-requested local objects. Creator/license
provenance remains attached to retained content. Content deletion is a
separate explicit operation.
