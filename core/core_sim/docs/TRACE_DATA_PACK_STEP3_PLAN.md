# core_sim Step 3 Trace/Data/Pack Plan

Status: active implementation reference for `core_sim v0.4.0` adapter work.

Step 3 should turn `core_sim` frame outcomes into reusable artifacts without
moving artifact ownership into the base simulation loop. The base library should
stay a dependency-light control plane. Optional adapters can translate its
stable records into trace lanes, data tables, and pack chunks when a host has a
real diagnostics, replay, or batch-analysis need.

## Target
- Give simulation hosts a standard way to emit frame, tick, pass, and timing
  records.
- Keep `core_sim` focused on cadence, pass ordering, loop state, and outcome
  reporting.
- Let hosts enrich those records with app-owned domain summaries, snapshots, or
  replay inputs.
- Promote shared adapters only after one host proves the shape and a second host
  is likely to reuse it.

## Non-Goals
- `core_sim` should not own entity storage, solver state, renderer state, or
  scenario schemas.
- `core_sim` should not take hard dependencies on `core_trace`, `core_data`, or
  `core_pack`.
- Step 3 should not freeze a replay or `.pack` wire format before a host proves
  the required artifact shape.
- Step 3 should not turn UI diagnostics, HUD rendering, or editor behavior into
  core simulation behavior.

## Mental Model
Step 3 has three lanes:

- Control plane: `core_sim` owns frame dt intake, fixed-step accumulation,
  pause/single-step state, max-tick clamps, ordered pass execution, frame
  outcomes, reason bits, summaries, and stage timing.
- Domain plane: the host owns entities, world state, solver math, scenario
  meaning, render policy, app metrics, snapshots, and state digests.
- Artifact plane: optional adapters map control-plane and host-provided domain
  records into `core_trace`, `core_data`, or `core_pack`.

This keeps the core loop reusable while still giving complex simulations a path
to record, analyze, and replay what happened.

## V1 Record Shape
The first useful artifact contract should be record-oriented rather than
serializer-oriented.

Recommended run header:
- `run_id`
- `program_key`
- `host_adapter_id`
- `core_sim_version`
- `fixed_dt_seconds`
- `max_ticks_per_frame`
- `pass_order_hash`
- `artifact_schema_version`

Recommended frame summary:
- `frame_index`
- `input_dt_seconds`
- `ticks_executed`
- `passes_executed`
- `reason_bits`
- `status`
- `render_requested`
- `max_tick_clamp_hit`
- `single_step_consumed`
- `sim_time_advanced_seconds`
- `accumulator_remaining_seconds`
- `failed_pass_id`
- `failed_pass_name`

Recommended tick summary:
- `frame_index`
- `tick_index`
- `tick_dt_seconds`
- `sim_time_before_seconds`
- `sim_time_after_seconds`
- `passes_executed`
- `status`

Recommended pass summary:
- `frame_index`
- `tick_index`
- `pass_id`
- `pass_name`
- `status`
- `duration_seconds`
- `failure_code`

Recommended stage timing:
- `frame_index`
- `stage_name`
- `start_seconds`
- `end_seconds`
- `duration_seconds`

App-specific fields should live in app-owned records that reference
`frame_index`, `tick_index`, or `pass_id`.

## Adapter Layers
Step 3 should start app-local and promote only when reuse is proven.

1. App-local artifact writer
   - First host maps `CoreSimFrameSummary`, pass descriptors, and
     `CoreSimStageTiming` into a simple artifact.
   - The host owns file naming, output triggers, and domain summaries.
   - This proves whether the record shape is enough before introducing a new
     shared module.

2. Optional `core_sim_trace` helper
   - `shared/core/core_sim_trace v0.1.0` now exists as the first promoted
     sibling helper for reusable control-plane trace lane mapping.
   - It depends on `core_sim` and `core_trace`, not as a dependency inside
     `core_sim`.
   - It emits standard lane names and markers while leaving pass meaning
     app-owned.

3. Optional `core_sim_data` helper
   - Promote when hosts need analyzable frame/tick/pass tables.
   - This should build table schemas and rows from `core_sim` records plus
     host-supplied app records.
   - It should avoid generic entity/world tables until real host schemas prove a
     cross-app shape.

4. Optional `core_sim_pack` helper
   - Promote after trace/data records need interchange, replay, or batch
     archiving.
   - It should package run headers, frame summaries, stage timings, pass
     metadata, and optional app-owned snapshot chunks.
   - Chunk names should stay provisional until at least one replay or batch
     artifact is validated.

## Replay Behavior
Replay should be phased so the promise stays truthful.

Phase A: control-plane replay
- Feed the same dt stream and pause/single-step commands.
- Verify the same pass order, tick counts, pass counts, reason bits, and failure
  surfaces.
- This proves `core_sim` orchestration determinism only.

Phase B: host state replay
- Add host-owned snapshot capture and restore.
- Verify app-owned state digests or domain summaries at selected frames.
- This proves a specific simulation host can replay its domain state.

Phase C: batch artifact replay
- Store run header, control inputs, summaries, optional snapshots, and expected
  digests in a pack artifact.
- Add a verifier that can run the host headlessly and compare the recorded
  control/domain summaries.
- This is the point where chunk names and pack compatibility should be frozen.

## First Proving Host
`gravity_orbit_sim` is the first Step 3 proving host for the initial
implementation slice.

Reasoning:
- It is the smallest fixed-step host and can validate the control-plane artifact
  shape without multi-entity behavior policy or renderer timing noise.
- It already uses `core_sim` as the top-level runtime-loop shell.
- A dedicated probe can compile against the live shared `core_sim` source while
  the dirty vendored subtree is left untouched.
- Its first artifact only needs a run header and one deterministic frame record.

`behavior_sim` remains the best second proving host after the minimal
fixed-step record path is stable.

Reasoning:
- It is multi-entity and pass-oriented, so it stresses the intended shape better
  than a single focused loop.
- It already owns persistent `CoreSimLoopState` and ordered pass execution.
- Its headless output can validate frame summaries without renderer noise.
- Its domain summaries can stay small: entity counts, active groups, aggregate
  metrics, and behavior-state digests.

`physics_sim` should come after the record shape is stable because solver
substeps and 3D backend details add volume. `ray_tracing` should come later
because progressive accumulation and renderer-specific diagnostics make the
artifact meaning broader than pure simulation cadence.

## Setup Requirements
- Hosts need stable pass ids and pass names.
- Hosts need a monotonically increasing `frame_index` and optional `tick_index`.
- Hosts should emit artifacts after the frame outcome is finalized, not while
  passes are still mutating domain state.
- Hosts should provide optional app summary callbacks instead of letting
  adapters inspect app internals.
- Trace/data/pack sinks should be optional and disabled by default in runtime
  builds unless a test, CLI flag, or diagnostics mode enables them.

## Acceptance Criteria
- `shared/core/core_sim` remains dependency-light and does not include
  `core_trace`, `core_data`, or `core_pack` headers.
- A first host can emit deterministic frame summaries from a headless run.
- The artifact includes enough information to explain why a frame ticked,
  clamped, failed, rendered, or idled.
- App-owned domain records reference the core frame/tick/pass keys instead of
  extending generic `core_sim` tables.
- A control-plane replay check can compare recorded and live outcome streams.

## Work Slices
1. Finalize the Step 3 record vocabulary in docs and `core_sim v0.4.0` helpers.
2. Add a `gravity_orbit_sim` artifact probe for deterministic fixed-step frame
   records.
3. Add a control-plane replay verifier for the recorded dt/control stream.
4. Add an app-local `behavior_sim` artifact writer for headless frame summaries.
5. Use `core_sim_trace v0.1.0` as the shared control-plane trace adapter when
   `behavior_sim` and later hosts need headless frame analysis.
6. Add `core_data` row helpers after the frame/tick/pass table shape is proven.
7. Add `core_pack` chunks only after snapshots or batch replay require durable
   interchange.

Current implementation checkpoint:
- `core_sim v0.4.0` provides the dependency-free run-header and frame-record
  helpers.
- `gravity_orbit_sim` has a focused `test-core-sim-artifact` target that emits
  deterministic frame records and replays the recorded dt stream through a fresh
  loop/world.
- `gravity_orbit_sim` also has a headless runtime output mode,
  `--emit-core-sim-artifact --core-sim-artifact-frames=N`, covered by the
  default smoke contract. It emits the current seeded scenario/session stream
  before SDL startup and validates replay internally.
- `core_sim_trace v0.1.0` promotes the first reusable Step 3 adapter: mapping
  `CoreSimFrameRecord` values into standard `core_sim.*` trace lanes and
  frame/reason markers while preserving app-owned domain lanes.

## Risks
- Freezing pack chunks before replay needs are proven.
- Hiding app-domain semantics inside generic `core_sim` tables.
- Capturing too much per-frame data and making diagnostics too expensive for
  normal runs.
- Treating control-plane determinism as full domain replay determinism.
- Letting renderer/HUD needs leak into core artifact contracts.

## Boundary Decision
Step 3 remains an adapter plan plus proven implementation slices. Shared
promotion should create optional sibling helpers, not expand `core_sim` itself:

- `shared/core/core_sim`: base control-plane library
- `shared/core/core_sim_trace`: current optional `core_sim` to `core_trace`
  mapping helper
- possible later `shared/core/core_sim_data`: `core_sim` to `core_data` table
  helper
- possible later `shared/core/core_sim_pack`: `core_sim` replay/batch pack
  helper

This preserves the rule that core simulation orchestration remains small,
stable, and reusable while artifact adapters grow only where proven.
