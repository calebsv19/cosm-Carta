# CodeWork Compute Platform Contract v1

Status: additive contract baseline

Module version: `core_headless_job` 0.2.x

## Authority

This document is the normative semantic and wire-shape reference for the
CodeWork compute-platform v1 contract family. The public C types and validators
in `include/core_headless_job_platform.h` mirror this contract without taking
ownership of JSON parsing or writing. Files under `fixtures/platform_v1/` are
canonical examples, not a second schema authority.

The existing `codework_job / headless_bundle_v1` and
`codework_job_report / headless_report_v1` contracts remain supported. This
contract is an additive successor surface; current applications do not need to
change until their adapters migrate.

## Ownership boundary

This contract owns:

- stable job, workflow, event, result, artifact, and worker-capability meaning
- canonical job-state and terminal-outcome vocabulary
- attempt, worker, claim/lease identity fields
- content identity and provenance references
- dependency and artifact-binding meaning between workflow stages
- idempotency and retry-policy fields

This contract does not own:

- JSON parser/writer implementation
- coordinator persistence or API transport
- scheduler algorithms or worker dispatch
- process launch, filesystem roots, upload, publication, or remote transport
- application payload schemas or algorithms
- authentication, tenancy, deployment topology, or service configuration
- RayTracing checkpoint/recovery internals
- CosmOS package, instance, codec, ledger, or QEMU lifecycle

## Common rules

- Schema identity is always the exact `schema_family` plus `schema_variant`
  pair.
- All identifiers are non-empty opaque strings. Consumers must not infer
  hierarchy or authorization from identifier spelling.
- Timestamps are UTC RFC 3339 strings ending in `Z`.
- Contract paths are relative, forward-slash paths. Absolute paths, empty
  segments, `.` segments, `..` segments, and backslashes are invalid.
- Content digests use lowercase SHA-256. Artifact manifests carry
  `digest_algorithm: "sha256"` and a 64-character hexadecimal digest.
- An executable digest is encoded as `sha256:<64 lowercase hex characters>`.
- Arrays contain at most 1024 records in the shared C validation surface.
- Storage location is not artifact identity. Transport or publication may
  change paths without changing `artifact_id` or its content digest.
- Retries create distinct attempts and preserve earlier events/results.
- A claim and lease identify execution authority for one attempt; they do not
  replace job or workflow identity.

## Schema registry

| Meaning | Schema family | Schema variant |
|---|---|---|
| accepted executable job | `codework_job` | `job_envelope_v1` |
| append-only job event | `codework_job_event` | `job_event_v1` |
| terminal attempt result | `codework_job_result` | `job_result_v1` |
| content/provenance record | `codework_artifact` | `artifact_manifest_v1` |
| ordered DAG declaration | `codework_workflow` | `workflow_manifest_v1` |
| worker inventory snapshot | `codework_worker` | `worker_capabilities_v1` |

## Job envelope

Required fields:

- `schema_family`, `schema_variant`
- `job_id`
- `idempotency_key`
- `program`
- `adapter`
  - `name`
  - `version`
  - `target_os`
  - `target_arch`
- `payload`
  - `schema_family`
  - `schema_variant`
  - `path`
- `input_artifacts`: zero or more `{artifact_id, role}` records
- `required_capabilities`: zero or more `{name, version_constraint}` records
- `output_contract`: `{schema_family, schema_variant}`
- `resources`
  - `min_cpu_cores`
  - `min_memory_bytes`
  - `min_gpu_count`
  - `max_runtime_seconds`
- `retry`
  - `max_attempts`, at least one
- `parent_jobs`: zero or more job ids
- `created_by`
- `created_at`

Resource zero values mean no semantic minimum was declared. A deployment may
still enforce its own bounded maximums.

`idempotency_key` is scoped by the accepting coordinator. Repeating the same
accepted request under that scope must return the same job identity or reject
semantic drift; it must not create silent duplicate work.

## Job states and events

Canonical states:

```text
submitted
validating
queued
claimed
preparing
running
collecting
completed
failed
cancelled
```

Normal forward path:

```text
submitted -> validating -> queued -> claimed -> preparing -> running
          -> collecting -> completed
```

Rules:

- `failed` and `cancelled` may be entered from any non-terminal state.
- A terminal state has no outgoing transition.
- A repeated state is not a transition.
- `claimed`, `preparing`, `running`, or `collecting` may return to `queued`
  after a recoverable attempt/lease failure. The failed attempt evidence must
  remain append-only.
- `HTTP 202`, submit acceptance, or queue presence never means completion.
- RayTracing `resumable` and `recovery_required` remain structured
  attempt/adapter conditions, not new generic job states.
- CosmOS controller states remain backend-local and map to platform events.

Event kinds:

- `state_transition`
- `progress`
- `diagnostic`
- `claim`
- `lease`
- `artifact`

Every event requires:

- schema identity
- `event_id`
- `job_id`
- monotonically increasing positive `sequence` within the job event stream
- `occurred_at`
- `kind`
- current `state`

`attempt_id`, `worker_id`, and `lease_id` are optional until the event is tied
to an execution attempt. Claim and lease events require all three. Progress is
a finite value in `[0, 1]`. A state-transition event carries
`previous_state`; JSON uses `null` for the first transition into `submitted`.

## Artifact manifest

Required fields:

- schema identity
- `artifact_id`
- `logical_name`
- `type`
- optional `media_type`
- optional paired `data_contract` schema family/variant
- `digest_algorithm`
- `digest`
- `size_bytes`
- `producer_job_id`
- `producer_attempt_id`
- `parent_artifacts`
- `constituents`

A constituent record contains a safe relative `path`, its SHA-256 digest, and
its byte size. A multi-file artifact's top-level digest identifies the
canonical manifest or deterministic bundle selected by its producer contract;
it is not derived implicitly by this core module.

Transport manifests, project-local receipts, and Visualizer READY manifests
remain adapters/consumers. They should reference canonical artifact identity
instead of redefining it.

## Job result

Required fields:

- schema identity
- `job_id`
- `attempt_id`
- `outcome`: `completed`, `failed`, or `cancelled`
- `finished_at`
- `executor` tool identity
- `output_artifacts`
- `failure_code`
- `failure_message`

Completed results carry empty failure strings. Failed and cancelled results
carry both a stable failure code and operator-readable message. A result is
attempt-scoped; the coordinator derives final job state from retained attempts
and retry policy.

## Workflow manifest

Required fields:

- schema identity
- `workflow_id`
- `idempotency_key`
- `stages`
- `created_by`
- `created_at`

Each stage declares:

- `stage_id`
- `job_template_id`
- `dependencies`
- `bindings`

Each binding maps an `input_role` to a prior `source_stage_id` and one
`source_artifact_role`.

Stages are stored in deterministic topological order. Every dependency and
binding source must name an earlier stage. Stage ids are unique. This rule
keeps the v1 validator dependency-free while still rejecting missing edges,
self-dependencies, forward references, and cycles.

The first reference workflow is:

```text
PhysicsSim cache generation -> RayTracing render
```

Scene-project validation is coordinator ingress validation for the first
slice, not a third application executor. LineDrawing produces the already
valid input project outside this initial execution graph.

## Worker capabilities

Required fields:

- schema identity
- `worker_id`
- `target_os`
- `target_arch`
- one or more `adapters`
- zero or more semantic `capabilities`
- available `resources`
- `observed_at`

Each adapter declares its name, version, and exact executable digest. A
capability declares a stable name and optional version. CPU cores and memory
must be positive; GPU count may be zero.

Heartbeat freshness, capacity reservations, deployment labels, endpoint
addresses, credentials, and package-install policy are coordinator/deployment
records, not part of this semantic snapshot.

## Compatibility and migration

- v0.1 bundle/report readers remain valid and unchanged.
- New coordinators may translate legacy app/VPS payloads into these contracts
  while retaining the original bytes as provenance.
- App runners keep app-local status vocabularies behind adapters.
- No application minimum shared version changes until that application
  actually consumes this additive surface.
- Remote activation requires a separately verified canonical coordinator
  checkout and deployed-version readback.
- CosmOS admission waits for its native host-worker boundary freeze and a
  separately tested translation adapter.

## First implementation acceptance

The collapsed local runtime may begin only after:

1. the standalone C contract tests pass
2. all canonical JSON fixtures pass their conformance test
3. legacy v0.1 tests remain green
4. the active simulation-program plan records this contract as the selected
   platform boundary
5. no unresolved field has to be guessed by the runtime implementation
