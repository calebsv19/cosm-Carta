# Program Library Connective Tissue State

Last updated: 2026-03-11

Purpose:
- Provide a single current-state reference for shared-library connections across active programs.
- Distinguish what is already integrated, what is partial, and what should migrate next.

Source of truth used for this snapshot:
- Program build wiring in `*/Makefile`.
- Shared module versions in `shared/core/*/VERSION` and `shared/kit/*/VERSION`.
- Active migration docs in `11_version_compat_matrix.md`, `29_program_shared_lib_connection_gaps.md`, and `../SHARED_LIBS_CURRENT_STATE.md`.

## Current Shared Module Versions

Core:
- `core_base`: `1.0.0`
- `core_io`: `1.0.0`
- `core_data`: `1.0.0`
- `core_pack`: `1.0.0`
- `core_scene`: `1.0.0`
- `core_space`: `1.0.0`
- `core_trace`: `1.0.0`
- `core_math`: `1.0.0`
- `core_theme`: `2.0.0`
- `core_font`: `1.0.1`
- `core_time`: `1.0.0`
- `core_queue`: `1.0.0`
- `core_sched`: `1.0.0`
- `core_jobs`: `1.0.0`
- `core_workers`: `1.0.0`
- `core_wake`: `1.0.1`
- `core_kernel`: `1.0.0`
- `core_pane`: `0.1.1`
- `core_memdb`: `0.24.10`

Kits:
- `kit_viz`: `1.0.0`
- `kit_render`: `0.10.0`
- `kit_ui`: `0.8.0`
- `kit_graph_timeseries`: `0.2.1`
- `kit_graph_struct`: `0.8.0`
- `kit_pane`: `0.1.0`

## Program Integration Snapshot

Legend:
- `A`: adopted in build/runtime path
- `P`: partial/additive/narrow-slice usage
- `-`: not integrated

| Program | base | io | data | pack | scene | space | trace | math | theme | font | time | queue | sched | jobs | workers | wake | kernel | memdb | pane | kit_viz | kit_render | kit_ui | kit_graph_ts | kit_graph_struct | sys_shims |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| datalab | A | A | A | A | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | A | - | - | P | - | - |
| daw | A | A | P | P | - | - | P | - | A | A | A | A | A | A | P | A | A | - | - | A | - | - | - | - | - |
| ide | A | A | A | A | - | - | - | - | A | A | A | A | A | A | A | A | A | - | - | - | - | - | - | - | - |
| line_drawing | A | P | P | P | P | - | P | P | A | A | A | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| line_drawing3d | A | P | P | P | P | - | P | P | A | A | A | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| map_forge | A | A | P | P | - | A | P | - | A | A | A | A | A | A | A | A | A | - | - | - | - | - | - | - | - |
| physics_sim | A | A | P | A | A | - | P | - | A | A | - | - | - | - | - | - | - | - | - | A | - | - | - | - | P |
| ray_tracing | A | A | P | P | A | A | P | - | A | A | A | - | - | - | - | - | - | - | - | A | - | - | - | - | - |
| fisiCs | P | P | P | P | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | A |
| mem_console (host) | A | - | - | A | - | - | - | - | A | A | A | A | A | A | A | A | A | A | A | - | A | A | - | A | - |
| workspace_sandbox (host) | A | - | - | A | - | - | - | - | - | - | - | - | - | - | - | - | - | - | A | - | - | - | - | - | - |

## Program Migration State And Next Connections

### `datalab`
Implemented now:
- strong `core_base/core_io/core_data/core_pack` integration
- `kit_viz` integration
- first `kit_graph_timeseries` adoption slice (`recommended_stride`) in trace rendering

Next migration slices:
1. route trace viewport math through `kit_graph_ts_compute_view` and `kit_graph_ts_zoom_view`
2. route hover inspection through `kit_graph_ts_hover_inspect`
3. migrate one concrete trace draw lane to `kit_graph_ts_draw_plot`
4. optional follow-on: align UI controls with `core_theme/core_font`

### `daw`
Implemented now:
- strong runtime/theme/font spine with `core_time` + `core_theme/core_font`
- mainthread execution-core path now adopted for `core_queue/core_sched/core_jobs/core_wake/core_kernel`
- `kit_viz` adapters integrated
- Slice 2 contract guard complete: deterministic `daw_pack_contract_parity_test` validates shared `core_pack` chunk contract and embedded `core_data` dataset schema keys
- Slice 3 data contract hardening complete: embedded dataset metadata now includes additive `schema_family`/`schema_variant`, and canonical `daw_selection_v1` table is locked by deterministic test coverage
- Slice 4 trace diagnostics lane complete: deterministic `daw_trace_export_contract_test` validates shared `core_trace` export lanes for transport/scheduler timing plus canonical `trace_start/trace_end` markers
- Slice 5 workers-lane adoption complete: async diagnostics trace export now runs through shared `core_workers` with deterministic completion/contract coverage (`daw_trace_export_async_contract_test`)

Next:
1. optional follow-on: evaluate broader runtime-loop worker migration beyond diagnostics lanes

### `ide`
Implemented now:
- full execution-core stack (`time/queue/sched/jobs/workers/wake/kernel`)
- shared theme/font and base/data/pack/io paths

Next:
1. continue ad-hoc diagnostics/file-path cleanup into shared contracts
2. execution-core hardening and shutdown/idle behavior tests

### `line_drawing` + `line_drawing3d`
Implemented now:
- shared base/scene/math/time + theme/font
- partial data/pack/trace tooling integration
- low-risk `core_io` cleanup completed for theme preset persistence (shared `core_io` read/write/path helpers)

Next:
1. import contract parity locked: JSON-only runtime import (`.pack` diagnostics-tooling only)
2. standardize data/pack output schemas between 2D/3D apps (data and pack parity now aligned)
3. defer remaining directory/export helper IO migration until shared `core_io` directory/create APIs are available
4. evaluate execution-core only when asynchronous/background orchestration becomes material

### `map_forge`
Implemented now:
- shared space + execution-core lane now includes `time/queue/sched/jobs/workers/wake/kernel`
- execution-core Slice 2 ready-lane migration complete: `app_tile_pipeline` Vulkan asset worker ready-handoff now uses shared `core_queue` (`CoreQueueMutex`) with additive queue semantics
- execution-core Slice 3 queue migration complete: `app_tile_pipeline` Vulkan polygon prep in/out handoff now uses shared `core_queue` (`CoreQueueMutex`) with additive worker policy
- diagnostics contract Slice 4 complete: deterministic `test_build_safety.sh` assertions now lock required `meta.dataset.json` schema/table keys, and deterministic `map_trace_contract_test` now locks shared trace pack chunk contract (`TRHD/TRSM/TREV`) plus canonical runtime lane/marker vocabulary (including lifecycle `trace_start/trace_end`)
- diagnostics contract Slice 5 complete: deterministic `map_trace_contract_test` now locks strict shared trace pack parity (exact chunk count/order and deterministic payload sizes for `TRSM`/`TREV`)
- theme/font integrated

Next:
1. stabilize/expand execution-core usage beyond current tile-loader + vk-asset-ready + vk-poly-prep queue lanes where app-local pthread/cond queues still exist
2. deepen `core_data/core_pack/core_trace` beyond additive diagnostics lanes

### `physics_sim`
Implemented now:
- strong base/io/pack/scene + theme/font + `kit_viz`
- partial data/trace + `sys_shims`
- trace tooling Slice 1 hardening complete: `physics_trace_tool` now uses shared `core_io` for manifest/path IO
- trace tooling Slice 2 guard complete: deterministic `manifest_to_trace` smoke checks lane/marker contract
- data tooling Slice 3 hardening complete: VF2D dataset sidecars include additive `schema_family`/`schema_variant` metadata keys
- pack/data Slice 4 guard complete: deterministic parity test validates VF2D `.pack` chunks against dataset sidecar schema

Next:
1. deepen `core_data` usage for broader simulation datasets
2. align pack payload semantics to those datasets
3. standardize trace lanes for shared analysis consumption
4. evaluate execution-core adoption when scheduler/worker patterns justify it

### `ray_tracing`
Implemented now:
- broad shared adoption (`base/io/scene/space/time`, theme/font, `kit_viz`)
- partial data/pack/trace
- Slice 1 complete: `fluid_import` low-risk file/manifest helpers now use shared `core_io`
- Slice 2 complete: render metrics dataset includes additive `schema_family`/`schema_variant` metadata keys
- Slice 3 complete: shared theme preset persistence adapter now uses shared `core_io` path/read/write helpers

Next:
1. raise `core_data` usage from export slices to richer runtime datasets
2. lock pack schemas around that model for cross-app interoperability
3. trace tooling source lane restored: `ray_trace_tool` + `manifest_to_trace` now build/run on shared fixture input
4. deterministic trace contract smoke checks now verify emitted lanes/markers (`test-manifest-to-trace-export`)
5. deterministic `core_pack` import parity guard now verifies `VFHD/DENS/VELX/VELY` contract (`test-fluid-pack-contract-parity`)
6. promote trace lanes from tooling-first to standardized runtime contracts
7. evaluate execution-core adoption beyond `core_time` only where useful

### `fisiCs`
Implemented now:
- strong `sys_shims` lane
- partial base/io/data/pack usage

Next:
1. deepen core usage only where it improves compiler/runtime clarity
2. keep shim conformance as dominant standardization priority

### `mem_console` (host)
Implemented now:
- strong adoption of `core_memdb`, `core_pane`, execution-core stack, and `kit_render/kit_ui/kit_graph_struct`
- this is the most advanced integration host for new core/kit surface area

Next:
1. continue as integration proving ground for `core_pane` + graph/kit evolution
2. keep migration outputs feeding back into reusable kit/core contracts

## Cross-System Priority Queue (Current)

1. Complete the next two DataLab `kit_graph_timeseries` adoption slices (view math + hover), then first draw-path migration.
2. Consolidate `core_data` + `core_pack` schema usage in `daw`, `physics_sim`, `ray_tracing`, `map_forge`.
3. Evaluate/expand `core_workers` adoption in apps with partial execution-core rollout (`daw`, `map_forge`, `ray_tracing`) where runtime policy convergence is still desired.
4. Keep `mem_console` as the forward integration host for `core_pane` + visual kit evolution, but avoid leaking host-specific policy into core/kit APIs.

## Maintenance Rule

When shared-library wiring changes in any program:
1. update this file
2. update `29_program_shared_lib_connection_gaps.md`
3. update `11_version_compat_matrix.md` if minimum required versions changed
4. update `../SHARED_LIBS_CURRENT_STATE.md` when module versions or adoption-level notes change
