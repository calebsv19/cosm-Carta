# Version Compatibility Matrix

Minimum supported shared-module versions per app.
Last updated: 2026-04-02

| App | core_base | core_io | core_data | core_pack | core_scene | core_space | core_trace | core_math | core_theme | core_font | core_time | core_queue | core_sched | core_jobs | core_workers | core_wake | core_kernel | kit_viz | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| physics_sim | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 1.0.0 | N/A | 2.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 1.0.0 | Strong core spine + kit_viz; trace/data paths are additive/tooling-oriented. |
| daw | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | 1.0.0 | N/A | 2.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.1 | 1.0.0 | 1.0.0 | Runtime/theme/font + mainthread execution-core path (`queue/sched/jobs/wake/kernel`) adopted; Slice 2 pack/data parity guard, Slice 3 additive data-contract hardening, Slice 4 shared `core_trace` diagnostics lane, and Slice 5 shared `core_workers` async diagnostics lane gate are active. |
| datalab | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 1.0.0 | Focused on base/io/data/pack + kit_viz ingestion/render paths. |
| ray_tracing | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 2.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | 1.0.0 | Uses scene/space/time + trace tooling; data/pack are partly additive diagnostics/import helpers. Pre-`TP-S3` runtime contract preflight lane is wired, and shared `core_scene_compile` baseline is available in build/test surface. |
| line_drawing | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 1.0.0 | 1.0.0 | 2.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 2D shape tooling paths include additive data/pack/trace usage. |
| line_drawing3d | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 1.0.0 | 1.0.0 | 2.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 3D variant mirrors 2D shared-core shape with additive data/pack/trace usage. |
| mapforge | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 1.0.0 | 1.0.0 | N/A | 2.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.1 | 1.0.0 | N/A | Tile-loader execution-core lane is integrated; Slice 2 routes vk-asset ready handoff through shared `core_queue`, Slice 3 routes `vk_poly_prep` in/out queues through shared `core_queue`, Slice 4 adds deterministic diagnostics contract gates for `meta.dataset.json` and trace lanes/chunks, Slice 5 hardens strict trace pack parity (exact chunk count/order and deterministic payload sizes), and runtime perf diagnostics now require `kit_runtime_diag >= 0.1.0`. |
| ide | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | 2.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.1 | 1.0.0 | N/A | Full execution-core stack integrated in current loop path. |
| fisiCs | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | Core usage remains partial/additive; `sys_shims` adoption is the dominant standardization path. |

## Update Rules
- Update this matrix whenever an app starts using a new shared module.
- Update minimum versions whenever an app relies on newly added module behavior.
- Keep `N/A` for modules not yet linked by that app.
- For shared patch bumps in active deps (for example `core_wake` `1.0.1`), update dependent app minimums only when they require that patch behavior.
