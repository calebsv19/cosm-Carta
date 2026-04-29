# Shared Libraries Current State (CodeWork)

Last updated: 2026-04-23
Scope: active build wiring and integration across `behavior_sim`, `datalab`, `daw`, `fisiCs`, `ide`, `line_drawing`, `line_drawing3d`, `map_forge`, `physics_sim`, `ray_tracing`.

## Core Library Versions (current)
- `core_base`: `1.0.0`
- `core_io`: `1.0.0`
- `core_data`: `1.0.0`
- `core_pack`: `1.1.0`
- `core_layout`: `0.1.0`
- `core_config`: `0.1.0`
- `core_action`: `0.1.0`
- `core_scene`: `1.1.0`
- `core_scene_compile`: `0.3.0`
- `core_space`: `1.0.0`
- `core_units`: `0.1.0`
- `core_object`: `0.1.0`
- `core_trace`: `1.0.0`
- `core_math`: `1.0.0`
- `core_pane`: `0.2.0`
- `core_theme`: `2.0.0`
- `core_font`: `1.0.1`
- `core_time`: `1.0.0`
- `core_queue`: `1.0.0`
- `core_sched`: `1.0.0`
- `core_jobs`: `1.0.0`
- `core_workers`: `1.0.0`
- `core_wake`: `1.0.1`
- `core_kernel`: `1.0.0`
- `core_memdb`: `0.24.10`

## New Bootstrap Modules
- `core_pane`: shared renderer-agnostic pane-tree foundation for split solve, constraint-aware ratio clamping, splitter hit metadata, and bounded drag updates (`v0.2.0` in current shared docs; `behavior_sim` currently consumes the vendored subtree surface available from its imported shared commit, which resolves to `v0.1.2`), now adopted by `physics_sim` and `behavior_sim` for editor-shell pane geometry while broader pane-host rollout remains bootstrap-stage elsewhere.
- `core_layout`: shared layout transaction-state scaffold (`v0.1.0`) for runtime/authoring mode lifecycle, draft/apply/cancel semantics, and rebuild intent signaling, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `core_config`: shared typed runtime configuration table scaffold (`v0.1.0`) for bounded key/value host settings, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `core_action`: shared action registry scaffold (`v0.1.0`) for action identity and trigger binding resolution, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `core_pane_module`: shared pane-module registry scaffold (`v0.1.0`) for stable module descriptors, capability/hook validation, and pane-leaf binding validation, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `core_units`: shared units conversion scaffold (`v0.1.0`) for canonical unit vocabulary and world-scale conversion helpers, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `core_object`: shared object contract scaffold (`v0.1.0`) for app-neutral identity/transform/dimensional-mode validation, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `core_scene`: shared scene contract module is now at `1.1.0` with additive typed scene-root/object helpers layered above the existing bundle/source resolver path: space-mode vocabulary, root metadata validation (`scene_id`, mode intent/default, unit kind, world scale), canonical object-kind vocabulary, and first primitive payload validation for `plane_primitive` / `rect_prism_primitive`.
- `core_scene_compile`: shared scene compile module is now at `0.3.0` with additive primitive-contract hardening layered above the existing authoring/runtime normalization path: canonical primitive payload validation for known primitive object kinds, deterministic preservation of object-local `primitive` payloads in runtime output, compile metadata emission, extension-preserving runtime handoff normalization, scene-aware semantic diff tooling (`scene_contract_diff`), and shared writeback merge-guard helpers (`core_scene_overlay_merge_shared.h`) consumed by trio runtime bridges.
- `kit_pane`: shared pane-shell presentation scaffold (`v0.1.0`) for chrome/splitter/authoring visuals layered above `core_pane`, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `kit_workspace_authoring`: shared host-agnostic authoring interaction scaffold is now at `v0.4.1` with shared overlay UI + derive/submit seam helpers and host-attach contract guidance for theme/font state handoff (including top-level picker/shell theme parity expectations); current proving-host + pilot-host coverage is `workspace_sandbox` + `datalab`.
- `kit_runtime_diag`: shared runtime diagnostics scaffold (`v0.1.0`) for app-neutral stage-timing derivation and input totals accumulation helpers, with first adoption in `map_forge` runtime perf logging.
- `core_memdb`: shared SQLite-backed memory DB foundation remains stable with additive event-lane ops (schema v6 with scope fields, `mem_audit`, `mem_event`, link-graph constraints), built-in `v1 -> v2 -> v3 -> v4 -> v5 -> v6` migration on open, scoped `mem_cli add/query`, session-budget controls on writes, `batch-add` with retry/failure controls, bounded `neighbors` retrieval, `health`, `audit-list`, `event-list`, full-field replay drift verification via `event-replay-check`, deterministic rebuild parity via `event-replay-apply`, event-first writes across `add`/`pin`/`canonical`/`rollup`/`link-*`, and legacy parity seeding/upgrade via `event-backfill`
- `vk_renderer`: shared Vulkan renderer module is now explicitly versioned at `1.0.0` (`shared/vk_renderer/VERSION`) with opt-in-only capture behavior (no automatic `vk_frame.ppm` frame dumps).

## Program -> Shared Library Integration Map
Legend:
- `A` = adopted in build/runtime/tooling
- `P` = partial/additive/export-path only
- `-` = not wired

| Program | base | io | data | pack | scene | space | trace | math | theme | font | time | queue | sched | jobs | workers | wake | kernel | kit_viz | sys_shims |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| datalab | A | A | A | A | - | - | - | - | - | A | - | - | - | - | - | - | - | A | - |
| behavior_sim | - | - | - | - | - | - | - | - | - | A | - | - | - | - | - | - | - | - | - |
| daw | A | A | P | P | - | - | P | - | A | A | A | A | A | A | P | A | A | A | - |
| fisiCs | P | P | P | P | - | - | - | - | - | - | - | - | - | - | - | - | - | - | A |
| ide | A | A | A | A | - | - | - | - | A | A | A | A | A | A | A | A | A | - | - |
| line_drawing | A | P | P | P | P | - | P | P | A | A | A | - | - | - | - | - | - | - | - |
| line_drawing3d | A | P | P | P | P | - | P | P | A | A | A | - | - | - | - | - | - | - | - |
| map_forge | A | A | P | P | - | A | P | - | A | A | A | A | A | A | A | A | A | - | - |
| physics_sim | A | A | P | A | A | - | P | - | A | A | - | - | - | - | - | - | - | A | P |
| ray_tracing | A | A | P | P | A | A | P | - | A | A | A | - | - | - | - | - | - | A | - |

## Execution Core Adoption Snapshot
- Fully adopted (all 7 execution cores):
  - `ide` (`core_time`, `core_queue`, `core_sched`, `core_jobs`, `core_workers`, `core_wake`, `core_kernel`)
- Partially adopted:
  - `map_forge` (`core_time`, `core_queue`, `core_sched`, `core_jobs`, `core_workers`, `core_wake`, `core_kernel`) via tile-loader lane; broader runtime still in progress
  - `daw` (`core_time`, `core_queue`, `core_sched`, `core_jobs`, `core_workers`, `core_wake`, `core_kernel`) via mainthread loop path plus diagnostics async export lane
  - `line_drawing`, `line_drawing3d` (`core_time`)
  - `ray_tracing` (`core_time`)
- Not yet adopted:
  - `datalab`, `physics_sim`, `fisiCs`

## Notes on Current Reality
- `map_forge` tile-loader lane is now on shared execution core (`queue/sched/jobs/workers/wake/kernel`) with additive migration.
- `map_forge` Slice 2 adds shared `core_queue` handoff usage in `app_tile_pipeline` (`vk_asset` ready queue), while preserving existing worker/stage behavior.
- `map_forge` Slice 3 adds shared `core_queue` handoff usage in `app_tile_pipeline` (`vk_poly_prep` in/out queues), while preserving existing worker lifecycle and prep behavior.
- `map_forge` Slice 4 diagnostics contract guard is complete: deterministic `test_build_safety.sh` assertions now lock required `meta.dataset.json` schema/table keys, and deterministic `map_trace_contract_test` now locks shared trace pack chunk contract (`TRHD/TRSM/TREV`) plus canonical runtime lane/marker vocabulary (including lifecycle `trace_start/trace_end`).
- `map_forge` Slice 5 trace pack parity guard is complete: deterministic `map_trace_contract_test` now locks strict shared trace pack parity with exact chunk count/order and deterministic payload sizes for `TRSM`/`TREV`.
- `daw` mainthread loop now uses shared execution-core modules (`core_queue/core_sched/core_jobs/core_wake/core_kernel`) with additive behavior; `core_workers` is now partially adopted in diagnostics async export lanes.
- `daw` Slice 2 pack/data guard is complete: deterministic `daw_pack_contract_parity_test` now validates canonical `core_pack` chunk contract and embedded `core_data` dataset schema keys.
- `daw` Slice 3 data contract hardening is complete: additive dataset metadata keys (`schema_family`, `schema_variant`) and canonical `daw_selection_v1` table are now locked by deterministic parity coverage.
- `daw` Slice 4 trace diagnostics lane is complete: deterministic `daw_trace_export_contract_test` validates canonical `core_trace` lanes (`frame_dt`, `transport_frame`, scheduler timing/count lanes) and `trace_start/trace_end` markers.
- `daw` Slice 5 workers lane is complete: async diagnostics trace export now uses shared `core_workers` with deterministic completion/contract coverage (`daw_trace_export_async_contract_test`).
- `daw` has no mandatory shared-lib connection gap in the current rollout plan; remaining shared work is optional consolidation/hardening.
- `core_wake` is at `1.0.1` (standards-safe timeout constant update), and downstream checks pass.
- `core_data` + `core_pack` remain mostly additive/export-path integrations in several apps (`daw`, `ray_tracing`, `map_forge`, `ide`, `fisiCs`) rather than full runtime-domain ownership.
- `datalab` now adopts shared `core_font` for overlay text rendering (runtime zoom now scales real TTF fonts instead of bitmap glyph text).
- Shared theme/font adapters are now default-on in the current UI app set (`daw`, `ide`, `line_drawing`, `line_drawing3d`, `map_forge`, `ray_tracing`, `physics_sim`), with app-local persistence added for runtime preset selection.
- `core_font` is now at `1.0.1` with shared-asset fallback paths aligned to real font files in `shared/assets/fonts`, reducing bitmap fallback in kit Vulkan text rendering.
- `kit_render` is now at `0.14.0` with one shared Vulkan text runtime path: the additive `kit_render_external_text.*` helpers remain the public bridge surface for non-`kit_ui` hosts, the internal `KIT_RENDER_CMD_TEXT` backend path consumes that same extracted SDL_ttf/cache runtime instead of keeping a duplicate internal raster-font cache implementation, and wrapped UTF-8 draw support now lives in that shared external runtime as well.
- `physics_sim` now partially adopts `kit_render` for both policy and runtime in its font migration: menu, editor, HUD, timer HUD, and structural font-open lanes resolve shared role/tier/zoom/render-scale policy through `kit_render`, the active UTF-8 raster/upload/cache path lives in shared `kit_render_external_text.*`, and the shared Vulkan command-text path now uses that same runtime so bridge hosts and full command-buffer hosts no longer diverge inside `kit_render`; the host also now consumes shared code through a vendored `third_party/codework_shared` subtree instead of direct live `../shared` linkage.
- `ray_tracing` now partially adopts `kit_render` for both build/runtime in its font migration: the host Makefile wires shared `kit_render`, app-local bridge/helper files resolve shared role/tier/zoom/render-scale policy and font-source registration through the shared runtime, active helper/menu/timer-HUD UTF-8 measure/draw lanes route through `kit_render_external_text.*`, wrapped helper labels now use that same shared runtime path, the old local `text_font_quality` helper is retired, and the host now consumes those shared modules through a vendored `third_party/codework_shared` subtree instead of direct live `../shared` linkage; remaining work is limited to final thin-wrapper decisions and any visual tuning.
- `kit_ui` is now at `0.8.0` with additive theme-scale style sync (`kit_ui_style_apply_theme_scale`), allowing UI density to track active shared theme presets at runtime.
- `kit_graph_timeseries` is now at `0.2.1`, and DataLab has started incremental adoption via shared stride guidance.
- `kit_graph_struct` is now at `0.8.0`.
- `core_memdb` is now at `0.24.10` with additive event-dual-write, full-field replay-check, deterministic projection rebuild/apply, and event-first projection apply in-transaction across all mutation lanes (`add`, `pin`, `canonical`, `rollup`, `link-add`, `link-update`, `link-remove`): schema target v6, built-in v1->v2->v3->v4->v5->v6 migration on open, append-only `mem_event` table + indexes (`event_id`, `ts_ns`, `event_type`, `session_id`), `mem_cli event-list` for bounded event inspection, `mem_cli event-replay-check` for full-field parity checks between replayed event projection and live rows, `mem_cli event-replay-apply` for source->target projection rebuild + parity verification, snapshot-backed payload emission on key write commands, and `mem_cli event-backfill` for upgrading legacy minimal events into replay-complete history while preserving existing scoped retrieval, `mem_audit` coverage, session budgets, bounded neighbors, link constraints, dedupe-aware add behavior, and transactional rollup.
- `mem_console` (top-level program host) includes an optional (`--kernel-bridge`) evaluation scaffold over `core_sched` + `core_jobs` + `core_wake` + `core_kernel`; this is additive host validation and is not yet a production-app matrix requirement.
- `mem_console` (top-level program host) serializes theme/font UI prefs through `core_pack` (`<db_path>.ui.pack`) as app-local persistence.
- `mem_console` (top-level program host) now evaluates shared `core_pane` (`0.1.0`) for split-pane solve + draggable splitter interaction in the 15C lane, with pane-ratio persistence wired through app-local prefs packs.
- `core_pane` is now at `0.2.0` with additive graph validation diagnostics (`core_pane_validate_graph`, validation-code strings) layered on top of the earlier deterministic drag and invalid-graph hardening coverage.
- `physics_sim` now adopts `core_pane` for its `PS4D-2B` editor shell: left/center/right pane rectangles are solved through an app-local pane-host wrapper while pane purpose and editor behavior remain app-local.
- `core_pack` is now at `1.1.0` with additive truthful volumetric PhysicsSim export conversion (`core_pack_convert_vf3d`) and a frozen `VF3H/DENS/VELX/VELY/VELZ/PRES/SOLI` profile layered alongside the unchanged legacy planar `VFHD` profile.
- `behavior_sim` now adopts shared `core_font` for shell labels and shared `core_pane` for top-level shell pane geometry through its vendored shared subtree import; current pane-host use stays bounded to `viewport`, `side_panel`, and `hud_strip` leaves while editor/debug meaning remains app-local.
- `kit_pane` is now scaffolded at `0.1.0` with baseline pane chrome/splitter rendering helpers and null-backend unit tests; adoption into app hosts is deferred behind workspace sandbox gates.
- `kit_runtime_diag` is now at `0.1.0` with baseline stage timing + input totals helpers, and `map_forge` now adopts it for runtime perf diagnostics math/counter accumulation while keeping routing/render behavior app-local.
- `workspace_sandbox` runtime now owns `core_action` registry/binding storage and exposes trigger-resolve + action-execute APIs, with the visual harness delegating key routing through that shared runtime path.
- `workspace_sandbox` runtime now uses shared `core_layout` as source-of-truth for mode transitions, pending-change tracking, revision counters, and rebuild acknowledgment semantics.
- `workspace_sandbox` runtime now uses shared `core_config` for split-authoring defaults (`split.default_ratio`, `split.min_size_a`, `split.min_size_b`) instead of hardcoded split creation constants.
- `workspace_sandbox` `SX2` lane now routes local authoring-engine adapter helpers through shared `kit_workspace_authoring` (`v0.1.0`) for entry-chord checks, trigger mapping, callback-driven action execution wrappers, and text-size helper adapters.
- `workspace_sandbox` visual run path now auto-loads/saves session preset state (`workspace_sandbox/data/last_session.pack`) for close/reopen pane-layout persistence during iterative authoring.
- `workspace_sandbox` P12 lane has started: authoring-mode divider hit-test + drag now routes through runtime APIs backed by `core_pane` splitter primitives.
- `workspace_sandbox` P12 lane now includes first structural reorder operation (`workspace.reorder_next`, key `R`) routed through runtime-owned `core_action` dispatch.
- `workspace_sandbox` P12 lane now supports explicit two-pane swap interaction in authoring mode (`click` then `Shift+click`) via runtime API `workspace_sandbox_swap_panes`.
- `workspace_sandbox` corner/seam lane C1 is complete: runtime now exposes derived seam/junction graph build API (`workspace_sandbox_build_derived_graph`), and harness corner visualization consumes that deterministic graph path.
- `workspace_sandbox` corner/seam lane C2 is complete: splitter drag now supports immediate corner-to-corner snapping (16px radius) with topology-preferred candidate ranking and snap-target glow in the harness.
- `workspace_sandbox` corner/seam lane C3 has started with first rewrite slice: `Alt` orthogonal-drag threshold (`24px`) arms `promote_center_cross`, with hard-cancel and console reason on invalid rewrites.
- future-work note: `shared/docs/memory_db_autonomous_maintenance/` (Plan 01-03) is planning-only and not yet counted as implemented runtime/adoption state in this snapshot.
- detailed per-program connective-tissue snapshot now lives in `shared/docs/ecosystem_north_star_docs/44_program_library_connective_tissue_state.md`.
