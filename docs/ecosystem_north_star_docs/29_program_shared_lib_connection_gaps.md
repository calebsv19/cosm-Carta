# Program Shared-Lib Connection Gaps

Last updated: 2026-03-09
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
- `core_base`, `core_io`, `core_time`, `core_theme`, `core_font`, `kit_viz` adopted.
- `core_data` + `core_pack` mainly additive/diagnostics.

Gaps:
- `Partial`: deepen `core_data` from diagnostics exports into richer canonical session/timeline model usage.
- `Partial`: align `core_pack` payloads to the `core_data` table contracts used by DAW diagnostics.
- `Missing`: optional `core_trace` diagnostics lane export for transport/scheduler timing.
- `Missing`: execution-core migration beyond `core_time` (`queue/sched/jobs/workers/wake/kernel`) where runtime loop paths benefit.

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
- `Partial`: decide and lock runtime import policy (JSON-only vs JSON+pack).
- `Partial`: unify tooling contracts for `core_data` + `core_pack` outputs with 3D sibling.
- `Partial`: harden `core_trace` tooling consistency and documented output lanes.
- `Missing`: execution-core adoption beyond `core_time` where background/task orchestration appears.

### `line_drawing3d`
Current shared profile:
- Mirrors `line_drawing` shared adoption shape.

Gaps:
- `Partial`: same runtime import contract decision as 2D.
- `Partial`: maintain schema/tooling parity with 2D (`core_data/core_pack/core_trace`).
- `Missing`: evaluate `core_space` only if cross-app 3D placement parity becomes a real requirement.

### `map_forge`
Current shared profile:
- `core_base/core_io/core_space/core_time/core_queue/core_workers/core_wake/core_theme/core_font` adopted.
- `core_data/core_pack/core_trace` partial/additive.

Gaps:
- `Missing`: execution-core completion (`core_sched`, `core_jobs`, `core_kernel`) if moving toward full standardized runtime loop.
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
- `Missing`: evaluate `core_time`/execution-core adoption where loop scheduling/work dispatch patterns justify it.

### `ray_tracing`
Current shared profile:
- Broad shared adoption: `core_base/core_io/core_scene/core_space/core_time`, theme/font, `kit_viz`.
- `core_data/core_pack/core_trace` are partly additive/tooling-oriented.

Gaps:
- `Partial`: raise `core_data` usage from render-metrics export slice into broader analyzable runtime datasets.
- `Partial`: lock `core_pack` export/import schemas around that data model for cross-app reuse.
- `Partial`: promote `core_trace` from tooling-first to clearer runtime contract lanes where useful.
- `Missing`: execution-core adoption beyond `core_time` if worker/job/scheduler behavior should be standardized with IDE/MapForge.

## Cross-System Priority Order (next)
1. Complete `core_data` + `core_pack` consolidation in `daw`, `physics_sim`, `ray_tracing`, `map_forge`.
2. Complete execution-core rollout in `map_forge` (`sched/jobs/kernel`) and evaluate high-value candidates in `daw`/`ray_tracing`.
3. Lock `line_drawing` + `line_drawing3d` import/tooling contract parity.
4. Expand `fisiCs` shared-core usage only where it improves compiler/runtime clarity without disrupting shim-focused flows.

## Maintenance Rule
When any app materially changes shared-lib usage:
- Update this doc first (gap state + next steps).
- Update `11_version_compat_matrix.md` if minimum required versions changed.
- Update `SHARED_LIBS_CURRENT_STATE.md` if adoption level changed.
