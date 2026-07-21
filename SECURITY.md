# Security policy

## Supported versions

Amphibia has no supported release yet. Only the current development branch receives security fixes.

## Reporting

Do not include credentials, private models, personal data, or exploit details in a public issue. Once the canonical Amphibia GitHub repository exists, use its private Security Advisory form. A separate security contact has not yet been established; this is a release blocker, not an invitation to publish sensitive details.

For non-sensitive hardening reports, open an issue with affected commit, platform/format, reproduction steps, impact, and any proposed mitigation.

## Scope

Important areas include malformed `.nam`/WAV handling, real-time safety, state deserialization, installer paths, dependency provenance, future HTTP/OAuth boundaries, credential storage, archive handling, and downloaded-content validation. See `docs/THREAT_MODEL.md` for the current design review.
