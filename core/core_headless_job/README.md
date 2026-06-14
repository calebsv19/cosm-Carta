# core_headless_job

Shared outer headless job/report contract for cross-program worker bundles.

## Scope
- Shared schema-family/schema-variant vocabulary for outer job envelopes
- Shared typed structs for:
  - tool identity
  - payload references
  - output roots
  - metadata
  - artifact records
  - top-level job envelope
  - top-level report summary
- JSON-free validation helpers for bundle/report semantics

## Boundaries
- No JSON parsing or writing
- No filesystem layout creation
- No scheduler, queue, or worker dispatch ownership
- No program-specific scene-schema ownership
- No status polling, process control, or artifact upload policy

## Current Contract Notes
- `core_headless_job` owns only the shared outer protocol meaning.
- Inner scene/world payload semantics remain program-owned and are referenced by
  `schema_family`, `schema_variant`, and `path`.
- Run-config semantics also remain program-owned; the shared boundary validates
  only presence and path/schema identity.
- The current surface is intentionally additive and minimal so `ray_tracing`
  and `physics_sim` can adopt it without forcing immediate scheduler or parser
  consolidation.
- Empty IDs, names, schema identifiers, and required paths are rejected at the
  shared boundary.
- Artifact records must always declare a type and path.
- Report state/stage strings are required, but the library does not yet impose
  a closed enum vocabulary.

## Status
- Initial bootstrap contract (`v0.1.0`) for the first unified VPS
  bundle/report rollout.
