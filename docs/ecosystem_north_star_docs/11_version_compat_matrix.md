# Version Compatibility Matrix

Minimum supported shared-module versions per app.
Last updated: 2026-02-27

| App | core_base | core_io | core_data | core_pack | core_scene | core_space | core_trace | core_math | core_theme | core_font | core_time | core_queue | core_sched | core_jobs | core_workers | core_wake | core_kernel | kit_viz | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| physics_sim | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 1.0.0 | N/A | 2.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 1.0.0 | Strong core spine + kit_viz; trace/data paths are additive/tooling-oriented. |
| daw | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | 2.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | 1.0.0 | Runtime/theme/font + core_time adopted; data/pack primarily diagnostics/export paths. |
| datalab | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 1.0.0 | Focused on base/io/data/pack + kit_viz ingestion/render paths. |
| ray_tracing | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 2.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | 1.0.0 | Uses scene/space/time + trace tooling; data/pack are partly additive diagnostics/import helpers. |
| line_drawing | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 1.0.0 | 1.0.0 | 2.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 2D shape tooling paths include additive data/pack/trace usage. |
| line_drawing3d | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 1.0.0 | 1.0.0 | 2.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 3D variant mirrors 2D shared-core shape with additive data/pack/trace usage. |
| mapforge | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 1.0.0 | 1.0.0 | N/A | 2.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | 1.0.0 | 1.0.1 | N/A | N/A | Now adopts queue/workers/wake/time for tile-loader pipeline; sched/jobs/kernel not yet integrated. |
| ide | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | 2.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.1 | 1.0.0 | N/A | Full execution-core stack integrated in current loop path. |
| fisiCs | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | Core usage remains partial/additive; `sys_shims` adoption is the dominant standardization path. |

## Update Rules
- Update this matrix whenever an app starts using a new shared module.
- Update minimum versions whenever an app relies on newly added module behavior.
- Keep `N/A` for modules not yet linked by that app.
- For shared patch bumps in active deps (for example `core_wake` `1.0.1`), update dependent app minimums only when they require that patch behavior.
