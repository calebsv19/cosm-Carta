# core_sim_trace

Optional `core_sim` to `core_trace` adapter.

## Scope
- Map `CoreSimFrameRecord` control-plane outcomes into standard trace lanes.
- Emit frame markers for tick/render/clamp/single-step/failure reasons.
- Keep `core_sim` dependency-free by living as a sibling adapter module.

## Boundary
`core_sim_trace` owns only simulation control-plane trace vocabulary.

It does not own:
- app entity/world snapshots
- solver state
- replay execution
- UI or HUD rendering
- `core_data` or `core_pack` table/chunk schemas

Apps should add domain-specific trace lanes beside these shared lanes, not by
expanding the generic `core_sim` trace vocabulary.

## Contract
- `core_sim_trace` is an optional sibling adapter layered on top of finalized `core_trace` session semantics and `core_sim` frame records. It does not widen base `core_sim` ownership.
- The adapter always emits seven scalar lanes in fixed order: `frame`, `dt`, `ticks`, `passes`, `reason`, `accum`, `sim_adv`.
- Optional markers are emitted afterward on the shared `core_sim.event` lane: first the `"frame"` marker when enabled, then one marker per known reason bit in fixed shared order.
- Unknown `reason_bits` are preserved only in the scalar `core_sim.reason` sample. They do not emit extra markers.
- Emission is sequential, not atomic. Capacity overflow follows `core_trace` bounded-retention behavior, so older samples or markers may be evicted while the emit still succeeds. If a later write fails because of finalization or another invalid-argument path, earlier writes remain in the trace session.
- This module currently has no separate `ROADMAP.md`; this README is the truth doc for the current hardening pass.

## Build

```sh
make -C shared/core/core_sim_trace
```

## Test

```sh
make -C shared/core/core_sim_trace test
```

## Change Notes
- `0.1.1`: patch hardening for adapter edge behavior: README now truth-locks fixed lane/marker order, unknown-reason policy, finalized-session propagation, and partial-emission semantics, and tests now cover null args, finalized sessions, unknown reason bits, and small-capacity partial writes.
- `0.1.0`: initial frame-record to trace mapping helper.
