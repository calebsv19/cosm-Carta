# core_sim

Shared simulation control-plane foundation.

## Scope
- Fixed-step accumulator policy
- Pause/play/single-step control state
- Max ticks per frame clamp
- Ordered simulation pass execution
- Deterministic per-frame outcome reporting
- Public pass-order validation, pass-outcome initialization, status names, and frame reason bits for host adapters
- UI-free frame summary, reason-name, and stage-timing helpers for diagnostics adapters

## Boundary
`core_sim` owns simulation orchestration semantics only.

It does not own:
- physics equations
- entity/world storage
- scenario formats
- rendering or UI
- platform input
- worker/job/scheduler ownership

Apps provide domain callbacks for simulation passes. `core_sim` decides when and in what order those callbacks run.

## Contract
- `core_sim` is a dependency-light control-plane only. It does not own time providers, schedulers, jobs, workers, wake behavior, trace capture, or retained scene/entity storage.
- `core_sim_loop_advance(...)` accepts only finite non-negative frame dt input. Invalid state, request, policy, or pass-order input returns an invalid frame outcome instead of mutating the loop through host callbacks.
- `core_sim_step_policy_valid(...)` requires a finite positive `fixed_dt_seconds` and a non-zero `max_ticks_per_frame`.
- Paused frames execute no ticks unless `single_step_requested` is armed. A paused single-step consumes exactly one tick and then clears the request.
- Active frames execute deterministic fixed-step ticks in declared pass order until the accumulator falls below `fixed_dt_seconds` or the max-tick clamp is reached.
- Artifact/header/summary/record/stage-timing helpers are pure reporting helpers layered on top of completed outcomes. They do not imply pack, trace, scheduler, or host-runtime ownership.
- This module currently has no separate `ROADMAP.md`; this README is the truth doc for the current hardening pass.

## Dependencies
- C standard library only for v0.4.0

Future adapters may layer on:
- `core_time`
- `core_kernel`
- `core_sched`
- `core_jobs`
- `core_workers`
- `core_queue`
- `core_wake`
- `core_trace`

## Status
Bootstrap implementation truth-locked with standalone unit tests and four proving hosts (`v0.4.2`):
- `gravity_orbit_sim` fixed-step runtime-loop adapter
- `behavior_sim` ordered pass-execution adapter
- `physics_sim` scene-level substep pass network plus 3D solver first-pass shell adapter
- `ray_tracing` progressive/render runtime-frame adapter

## Build

```sh
make -C shared/core/core_sim
```

## Test

```sh
make -C shared/core/core_sim test
```

## Change Notes
- `0.4.2`: patch-level pedantic cleanup: the internal FNV-1a hash constants now use typed static constants instead of oversized enum values, removing standards-warning noise without changing runtime behavior.
- `0.4.1`: patch hardening for control-plane edge behavior: finite policy/frame-dt validation is now explicit, stage-timing helpers reject non-finite mark data, and the README now truth-locks helper/lifecycle boundaries and paused/single-step semantics.
- `0.4.0`: additive Step 3 artifact-record helpers: public version string,
  deterministic pass-order hashing, artifact run-header initialization, and
  frame-record extraction from completed frame outcomes. These helpers keep
  trace/data/pack sinks optional and dependency-free.
- `0.3.0`: additive host-adapter examples plus UI-free diagnostics helpers:
  reason-name extraction, frame outcome summaries, and stage timing derivation.
  Optional `core_trace` / `core_data` / `core_pack` adapter contracts are
  documented without adding hard module dependencies.
- Roadmap boundary, 2026-05-03: `core_sim` is now proven across fixed-step,
  entity/group pass-order, solver/substep, and progressive/render-frame host
  shapes. Near-term work should focus on examples, diagnostics vocabulary, and
  optional trace/snapshot/replay adapters, not on moving solvers, entity storage,
  renderer policy, or scenario schemas into this module.
- `0.2.0`: additive adapter hardening with public status names, pass-order validation, pass-outcome initialization, and frame reason bits; `behavior_sim` now uses `core_sim` for ordered stub-pass execution.
- `0.1.0`: initial fixed-step loop state, pause/play/single-step controls, ordered pass execution, max-tick clamp, and frame outcome contract.
- `physics_sim` consumes `core_sim >= 0.2.0` through its vendored shared subtree for a scene-level substep pass network and the 3D solver first-pass shell; solver math, mode hooks, emitter/backend/object operations, scene time, and render/HUD meaning remain app-local callbacks.
- `ray_tracing` consumes `core_sim >= 0.2.0` through its vendored shared subtree for one-tick runtime-frame pass routing (`events`, `update`, `route`, `submit`, `loop_conditions`); progressive accumulation, scene/camera math, renderer submission, and window policy remain app-local.

## References
- `docs/HOST_ADAPTER_EXAMPLES.md`
- `docs/OPTIONAL_TRACE_DATA_PACK_ADAPTERS.md`
- `docs/TRACE_DATA_PACK_STEP3_PLAN.md`
