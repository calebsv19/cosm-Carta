# core_sim Optional Trace/Data/Pack Adapter Plan

Status: dependency-free adapter contract plan for `core_sim v0.4.0`.

`core_sim` should not require `core_trace`, `core_data`, or `core_pack`.
Instead, hosts can use `core_sim` summaries and timing helpers as stable source
records, then map them into optional artifact lanes.

## Source Records

The stable source records are:
- `CoreSimFrameOutcome`
- `CoreSimFrameSummary`
- `CoreSimArtifactRunHeader`
- `CoreSimFrameRecord`
- pass ids and pass names from `CoreSimPassDescriptor`
- `CoreSimStageMark`
- `CoreSimStageTiming`

These are UI-free and domain-free. They should be enough for trace, dataset,
snapshot, and replay scaffolds without forcing artifact dependencies into the
core loop ABI.

## Optional core_trace Mapping

Recommended lanes:
- `core_sim.frame_dt`
- `core_sim.tick_count`
- `core_sim.pass_count`
- `core_sim.accumulator_remaining`
- `core_sim.sim_time_advanced`
- `core_sim.reason_bits`
- `core_sim.stage_duration.<stage_name>`

Recommended markers:
- `core_sim.frame_begin`
- `core_sim.tick_executed`
- `core_sim.max_tick_clamp_hit`
- `core_sim.single_step_consumed`
- `core_sim.pass_failed`
- `core_sim.frame_end`

Rule: trace adapters may record pass names and durations, but pass meaning stays
owned by the app adapter.

## Optional core_data Mapping

Recommended table: `core_sim_frame_summary_v1`.

Suggested columns:
- `frame_index`
- `status`
- `ticks_executed`
- `passes_executed`
- `reason_bits`
- `render_requested`
- `max_tick_clamp_hit`
- `single_step_consumed`
- `sim_time_advanced_seconds`
- `accumulator_remaining_seconds`
- `failed_pass_id`
- `failed_pass_name`

Recommended table: `core_sim_stage_timing_v1`.

Suggested columns:
- `frame_index`
- `stage_name`
- `start_seconds`
- `end_seconds`
- `duration_seconds`

Rule: app-specific metrics should go into app-owned tables that reference
`frame_index` or `tick_index`, not into generic `core_sim` tables.

## Optional core_pack Mapping

Recommended chunks:
- `CSIM`: simulation frame summary metadata
- `CSTG`: stage timing rows
- `CSPA`: pass metadata rows

These chunk names are planning placeholders, not frozen pack contracts. Freeze
them only when a real host needs interchange or replay artifacts.

## Replay Boundary

`core_sim` can help replay verify control-plane determinism:
- same frame dt stream
- same pause/single-step commands
- same pass order
- same ticks/passes/reason bits

`core_sim` cannot replay app state by itself. Domain snapshots still need
app-owned state serialization or a later proven `core_data` / `core_pack`
contract.

## Next Adapter Slice

`shared/core/core_sim_trace v0.1.0` is now the first optional adapter slice. It
maps `CoreSimFrameRecord` values into standard `core_sim.*` trace lanes and
frame/reason markers.

Hosts should use it for shared control-plane traces, then keep app-domain
snapshots, solver metrics, entity state, and durable `core_data` / `core_pack`
artifacts app-owned until those shapes are proven by multiple hosts.
