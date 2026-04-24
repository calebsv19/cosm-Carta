# Program Shared-Lib Connection Gaps

Last updated: 2026-04-02
Purpose: canonical per-program list of shared-lib connection gaps and next integrations.

Use this with:
- `../11_version_compat_matrix.md` for minimum supported versions.
- `../../SHARED_LIBS_CURRENT_STATE.md` for current adoption snapshot.

## Gap Legend
- `Missing`: not wired in build/runtime/tooling where it would provide clear value.
- `Partial`: wired, but only in additive or narrow paths; broader consolidation still open.
- `Stabilize`: wired broadly; remaining work is hardening/cleanup/documentation.

## Per-Program Gap List

### `datalab`
Current shared profile:
- Strong on `core_base/core_io/core_data/core_pack` and `kit_viz`.
- First incremental `kit_graph_timeseries` adoption is now in place (shared stride guidance for trace decimation).

Gaps:
- `Partial`: continue `kit_graph_timeseries` migration (shared view math, hover inspection, then one shared draw path).
- `Missing`: `core_trace` trace-import/view path standardization.
- `Missing`: `core_theme/core_font` if/when DataLab UI theming should align with app-wide presets.
- `Partial`: normalize profile loaders so all external data lanes map through one shared parsing contract.

### `daw`
Current shared profile:
- `core_base`, `core_io`, `core_time`, `core_queue`, `core_sched`, `core_jobs`, `core_wake`, `core_kernel`, `core_theme`, `core_font`, `kit_viz` adopted.
- `core_data` + `core_pack` mainly additive/diagnostics.

Gaps:
- `Stabilize`: no mandatory shared-lib gap remains for the current DAW rollout plan.
- `Stabilize`: Slice 3 complete (data contract hardening) - DAW dataset metadata now includes additive `schema_family`/`schema_variant` keys and deterministic contract coverage for canonical `daw_selection_v1` table.
- `Stabilize`: Slice 2 complete (pack/data contract parity guard) - deterministic `daw_pack_contract_parity_test` now verifies `DAWH/WMIN/WMAX/MRKS/JSON` chunk presence plus canonical `core_dataset` schema keys (`daw_timeline_v1`, `dataset_schema`, `dataset_contract_version`).
- `Stabilize`: Slice 4 complete (trace diagnostics lane) - deterministic `daw_trace_export_contract_test` now verifies canonical transport/scheduler timing lanes and `trace_start/trace_end` markers exported through shared `core_trace`.
- `Stabilize`: Slice 5 complete (workers lane adoption) - async diagnostics trace export now uses shared `core_workers` with deterministic completion/contract coverage (`daw_trace_export_async_contract_test`), while preserving runtime-loop behavior.

### `fisiCs`
Current shared profile:
- `sys_shims` is strong.
- `core_base/core_io/core_data/core_pack` are partial/additive.

Gaps:
- `Partial`: expand core usage beyond diagnostics/utility flows into more compiler/runtime helper paths.
- `Partial`: stabilize diagnostics schema contracts and pack artifacts with reader validation.
- `Missing`: execution-core adoption (only if/when runtime-loop orchestration enters scope).

### `ide`
Current shared profile:
- Full execution-core stack adopted (`time/queue/sched/jobs/workers/wake/kernel`).
- `core_base/core_io/core_data/core_pack` and `core_theme/core_font` are adopted.

Gaps:
- `Partial`: complete migration of remaining ad-hoc file/diagnostics paths into shared `core_io`/`core_data` patterns.
- `Partial`: move from export-first `core_pack` diagnostics into broader standardized snapshot/restore contracts (if needed).
- `Stabilize`: continue execution-core hardening tests (idle policies, wake behavior, shutdown boundaries).

### `line_drawing`
Current shared profile:
- `core_base`, `core_scene`, `core_math`, `core_time`, `core_theme`, `core_font` adopted.
- `core_io/core_data/core_pack/core_trace` are additive/partial.

Gaps:
- `Stabilize`: runtime import policy locked to JSON-only (`.pack` remains diagnostics-tooling only).
- `Stabilize`: `core_data` schema parity with 3D is now locked for shared metadata + shared `anchors_v1`/`walls_v1` tables; 3D-only fields are additive via `anchors_3d_ext_v1`.
- `Stabilize`: `core_pack` diagnostics contract parity with 3D is now locked (shared chunk sequence + shared base `LDAN` layout + additive `LDA3` extension).
- `Stabilize`: `core_trace` tooling consistency now aligned with 3D sibling (shared targets, CLI, and output lane contract).
- `Stabilize`: low-risk `core_io` cleanup completed for theme preset persistence (`core_io_path_exists` + `core_io_read_all`/`core_io_write_all`); remaining directory/create helpers stay app-local for now.
- `Missing`: execution-core adoption beyond `core_time` where background/task orchestration appears.

### `line_drawing3d`
Current shared profile:
- Mirrors `line_drawing` shared adoption shape.

Gaps:
- `Stabilize`: follows 2D runtime policy lock (JSON-only runtime import; `.pack` tooling-only).
- `Stabilize`: `core_data`/`core_pack`/`core_trace` parity with 2D is now locked (shared dataset contract, shared pack contract, shared trace tooling).
- `Missing`: evaluate `core_space` only if cross-app 3D placement parity becomes a real requirement.

### `map_forge`
Current shared profile:
- `core_base/core_io/core_space/core_time/core_queue/core_sched/core_jobs/core_workers/core_wake/core_kernel/core_theme/core_font` adopted.
- `kit_runtime_diag` adopted for runtime perf diagnostics stage timing and input counter totals.
- `core_data/core_pack/core_trace` partial/additive.

Gaps:
- `Stabilize`: Slice 1 execution-core completion in tile-loader lane now integrated (`core_sched/core_jobs/core_kernel`) with additive behavior.
- `Stabilize`: Slice 2 execution-core queue migration complete in `app_tile_pipeline` Vulkan asset ready-handoff (shared `core_queue` with additive eviction/retry behavior retained).
- `Stabilize`: Slice 3 execution-core queue migration complete in `app_tile_pipeline` Vulkan polygon prep in/out handoff queues (shared `core_queue` with additive worker policy retained).
- `Stabilize`: Slice 4 diagnostics contract guard complete - deterministic `test_build_safety.sh` assertions now lock required `meta.dataset.json` schema/table keys, and deterministic `map_trace_contract_test` locks shared trace pack chunk presence (`TRHD/TRSM/TREV`) plus canonical runtime lane/marker vocabulary (including `trace_start/trace_end` lifecycle markers).
- `Stabilize`: Slice 5 trace pack parity guard complete - deterministic `map_trace_contract_test` now locks exact shared `core_pack` chunk count/order (`TRHD` -> `TRSM` -> `TREV`) and deterministic payload sizes for sample/marker chunks.
- `Stabilize`: runtime diagnostics math/counter consolidation now uses shared `kit_runtime_diag` helpers; app-specific routing/render semantics remain local.
- `Partial`: consolidate map diagnostics into stronger `core_data` contracts and route optional diagnostics archives through `core_pack`.
- `Partial`: expand standardized trace-lane usage (`core_trace`) from tooling-level into clearer runtime diagnostics surfaces.

### `physics_sim`
Current shared profile:
- Strong on `core_base/core_io/core_pack/core_scene`, plus `kit_viz`, theme/font.
- `core_data` and `core_trace` partial.

Gaps:
- `Partial`: deepen `core_data` model breadth beyond current export tables into broader sim-domain datasets.
- `Partial`: further align `core_pack` payload semantics with canonical `core_data` schema.
- `Partial`: standardize `core_trace` lanes/contracts beyond tooling-centric usage.
- `Stabilize`: Slice 1 complete (tooling hardening) - `physics_trace_tool` manifest/path IO now uses shared `core_io` helpers with no lane/output behavior change.
- `Stabilize`: Slice 2 complete (trace contract guard) - deterministic `manifest_to_trace` smoke test now verifies canonical lanes/markers.
- `Stabilize`: Slice 3 complete (data contract hardening) - VF2D dataset sidecars now include additive `schema_family`/`schema_variant` metadata keys with test coverage.
- `Stabilize`: Slice 4 complete (pack/data parity guard) - deterministic parity test now verifies VF2D `.pack` chunk contract and sidecar dataset schema together.
- `Missing`: evaluate `core_time`/execution-core adoption where loop scheduling/work dispatch patterns justify it.

### `ray_tracing`
Current shared profile:
- Broad shared adoption: `core_base/core_io/core_scene/core_space/core_time`, theme/font, `kit_viz`.
- `core_data/core_pack/core_trace` are partly additive/tooling-oriented.
- `core_scene_compile` pre-`TP-S3` baseline wiring is now in place for authoring->runtime handoff preflight.

Gaps:
- `Partial`: raise `core_data` usage from render-metrics export slice into broader analyzable runtime datasets.
- `Partial`: lock `core_pack` export/import schemas around that data model for cross-app reuse.
- `Partial`: promote `core_trace` from tooling-first to clearer runtime contract lanes where useful.
- `Stabilize`: Slice 1 complete (io hardening) - `fluid_import` now uses shared `core_io` for file-exists and manifest file reads in low-risk paths.
- `Stabilize`: Slice 2 complete (data contract hardening) - render metrics dataset now includes additive `schema_family`/`schema_variant` metadata with test coverage.
- `Stabilize`: Slice 3 complete (io cleanup) - shared theme preset persistence now uses shared `core_io` helpers in `ui/shared_theme_font_adapter`.
- `Stabilize`: trace tooling source lane restored - `ray_trace_tool` now builds and `manifest_to_trace` now exports valid trace packs through shared `core_trace`.
- `Stabilize`: deterministic trace contract smoke assertions are now in place (`make -C ray_tracing test-manifest-to-trace-export`) for canonical lanes/markers.
- `Stabilize`: deterministic `core_pack` parity guard is now in place (`make -C ray_tracing test-fluid-pack-contract-parity`) for `VFHD/DENS/VELX/VELY` import contract parity.
- `Stabilize`: pre-`TP-S3` runtime-scene preflight lane is in place (`import/runtime_scene_bridge`) with contract tests against trio fixtures (`scene_runtime_v1` accept, authoring-variant reject).
- `Missing`: execution-core adoption beyond `core_time` if worker/job/scheduler behavior should be standardized with IDE/MapForge.

## Cross-System Priority Order (next)
1. Complete `core_data` + `core_pack` consolidation in `map_forge`, `ray_tracing`, `physics_sim` (DAW is now stabilize-only for this lane).
2. Expand standardized runtime `core_trace` lanes in `map_forge` and `physics_sim` where tooling-first usage still dominates.
3. Evaluate high-value execution-core adoption candidates in `ray_tracing` (`queue/sched/jobs/workers/wake/kernel`) beyond `core_time`.
4. Keep `line_drawing` + `line_drawing3d` in stabilize mode and only migrate additional IO helpers when shared directory APIs are available.
5. Expand `fisiCs` shared-core usage only where it improves compiler/runtime clarity without disrupting shim-focused flows.

## Maintenance Rule
When any app materially changes shared-lib usage:
- Update this doc first (gap state + next steps).
- Update `11_version_compat_matrix.md` if minimum required versions changed.
- Update `SHARED_LIBS_CURRENT_STATE.md` if adoption level changed.
