# core_headless_job

Shared outer job, workflow, event, result, artifact, and worker-capability
semantics for cross-program compute.

## Scope
- Backward-compatible `headless_bundle_v1` and `headless_report_v1` vocabulary
- Additive platform-v1 vocabulary for:
  - accepted job envelopes
  - append-only job events
  - terminal attempt results
  - content/provenance artifact manifests
  - topologically ordered workflows
  - worker capability snapshots
- Closed job-state, event-kind, and terminal-outcome vocabularies
- Transition validation, including recoverable attempt requeue
- Shared typed structs for:
  - tool identity
  - payload references
  - output roots
  - metadata
  - artifact records
  - top-level job envelope
  - top-level report summary
- Separate typed structs for platform jobs, events, results, artifacts,
  workflows, capabilities, attempts, workers, and leases
- JSON-free validation helpers for bundle/report semantics
- Canonical JSON examples with a deterministic fixture-conformance test

The normative platform contract is
[`PLATFORM_CONTRACT_V1.md`](PLATFORM_CONTRACT_V1.md).

## Boundaries
- No JSON parsing or writing
- No filesystem layout creation
- No scheduler, queue, or worker dispatch ownership
- No coordinator persistence or API transport ownership
- No program-specific scene-schema ownership
- No status polling, process control, or artifact upload policy
- No authentication, tenancy, deployment, or application retry-policy choice

## Current Contract Notes
- `core_headless_job` owns only the shared outer protocol meaning.
- Inner scene/world payload semantics remain program-owned and are referenced by
  `schema_family`, `schema_variant`, and `path`.
- Run-config semantics also remain program-owned; the shared boundary validates
  only presence and path/schema identity.
- The platform-v1 surface is additive so current `ray_tracing`, `physics_sim`,
  and other v0.1 callers remain source-compatible.
- Empty IDs, names, schema identifiers, and required paths are rejected at the
  shared boundary.
- Artifact records must always declare a type and path.
- Legacy report state/stage strings remain open for compatibility.
- Platform-v1 paths are safe relative paths, timestamps are UTC RFC 3339,
  artifact content uses lowercase SHA-256, workflows are deterministic
  topological order, and terminal attempts carry canonical outcomes.
- Retries create attempts; claim and lease identity never replaces job
  identity.

## Status
- `v0.2.0`: additive compute-platform contract freeze plus fixtures and
  standalone validation.
- `v0.1.0`: initial unified VPS bundle/report bootstrap, retained for
  compatibility.
