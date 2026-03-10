# Shared Libraries Current State (CodeWork)

Last updated: 2026-03-09
Scope: active build wiring and integration across `datalab`, `daw`, `fisiCs`, `ide`, `line_drawing`, `line_drawing3d`, `map_forge`, `physics_sim`, `ray_tracing`.

## Core Library Versions (current)
- `core_base`: `1.0.0`
- `core_io`: `1.0.0`
- `core_data`: `1.0.0`
- `core_pack`: `1.0.0`
- `core_layout`: `0.1.0`
- `core_config`: `0.1.0`
- `core_action`: `0.1.0`
- `core_scene`: `1.0.0`
- `core_space`: `1.0.0`
- `core_trace`: `1.0.0`
- `core_math`: `1.0.0`
- `core_pane`: `0.1.1`
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
- `core_pane`: shared renderer-agnostic pane-tree foundation for split solve, constraint-aware ratio clamping, splitter hit metadata, and bounded drag updates (`v0.1.1`), currently in bootstrap validation and not yet marked adopted by matrix-tracked apps.
- `core_layout`: shared layout transaction-state scaffold (`v0.1.0`) for runtime/authoring mode lifecycle, draft/apply/cancel semantics, and rebuild intent signaling, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `core_config`: shared typed runtime configuration table scaffold (`v0.1.0`) for bounded key/value host settings, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `core_action`: shared action registry scaffold (`v0.1.0`) for action identity and trigger binding resolution, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `kit_pane`: shared pane-shell presentation scaffold (`v0.1.0`) for chrome/splitter/authoring visuals layered above `core_pane`, currently in bootstrap validation and not yet tracked as adopted by matrix apps.
- `core_memdb`: shared SQLite-backed memory DB foundation remains stable with additive event-lane ops (schema v6 with scope fields, `mem_audit`, `mem_event`, link-graph constraints), built-in `v1 -> v2 -> v3 -> v4 -> v5 -> v6` migration on open, scoped `mem_cli add/query`, session-budget controls on writes, `batch-add` with retry/failure controls, bounded `neighbors` retrieval, `health`, `audit-list`, `event-list`, full-field replay drift verification via `event-replay-check`, deterministic rebuild parity via `event-replay-apply`, event-first writes across `add`/`pin`/`canonical`/`rollup`/`link-*`, and legacy parity seeding/upgrade via `event-backfill`

## Program -> Shared Library Integration Map
Legend:
- `A` = adopted in build/runtime/tooling
- `P` = partial/additive/export-path only
- `-` = not wired

| Program | base | io | data | pack | scene | space | trace | math | theme | font | time | queue | sched | jobs | workers | wake | kernel | kit_viz | sys_shims |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| datalab | A | A | A | A | - | - | - | - | - | - | - | - | - | - | - | - | - | A | - |
| daw | A | A | P | P | - | - | - | - | A | A | A | - | - | - | - | - | - | A | - |
| fisiCs | P | P | P | P | - | - | - | - | - | - | - | - | - | - | - | - | - | - | A |
| ide | A | A | A | A | - | - | - | - | A | A | A | A | A | A | A | A | A | - | - |
| line_drawing | A | P | P | P | P | - | P | P | A | A | A | - | - | - | - | - | - | - | - |
| line_drawing3d | A | P | P | P | P | - | P | P | A | A | A | - | - | - | - | - | - | - | - |
| map_forge | A | A | P | P | - | A | P | - | A | A | A | A | - | - | A | A | - | - | - |
| physics_sim | A | A | P | A | A | - | P | - | A | A | - | - | - | - | - | - | - | A | P |
| ray_tracing | A | A | P | P | A | A | P | - | A | A | A | - | - | - | - | - | - | A | - |

## Execution Core Adoption Snapshot
- Fully adopted (all 7 execution cores):
  - `ide` (`core_time`, `core_queue`, `core_sched`, `core_jobs`, `core_workers`, `core_wake`, `core_kernel`)
- Partially adopted:
  - `map_forge` (`core_time`, `core_queue`, `core_workers`, `core_wake`)
  - `daw` (`core_time`)
  - `line_drawing`, `line_drawing3d` (`core_time`)
  - `ray_tracing` (`core_time`)
- Not yet adopted:
  - `datalab`, `physics_sim`, `fisiCs`

## Notes on Current Reality
- `map_forge` is now on shared queue/worker/wake primitives for tile loading.
- `core_wake` is at `1.0.1` (standards-safe timeout constant update), and downstream checks pass.
- `core_data` + `core_pack` remain mostly additive/export-path integrations in several apps (`daw`, `ray_tracing`, `map_forge`, `ide`, `fisiCs`) rather than full runtime-domain ownership.
- Shared theme/font adapters are now default-on in the current UI app set (`daw`, `ide`, `line_drawing`, `line_drawing3d`, `map_forge`, `ray_tracing`, `physics_sim`), with app-local persistence added for runtime preset selection.
- `core_font` is now at `1.0.1` with shared-asset fallback paths aligned to real font files in `shared/assets/fonts`, reducing bitmap fallback in kit Vulkan text rendering.
- `kit_render` is now at `0.10.0` with additive runtime theme/font preset setters and a TTF-first Vulkan text path (with bitmap fallback), allowing live preset switching plus substantially improved text legibility in kit-hosted Vulkan UIs.
- `kit_ui` is now at `0.8.0` with additive theme-scale style sync (`kit_ui_style_apply_theme_scale`), allowing UI density to track active shared theme presets at runtime.
- `kit_graph_timeseries` is now at `0.2.1`, and DataLab has started incremental adoption via shared stride guidance.
- `kit_graph_struct` is now at `0.8.0`.
- `core_memdb` is now at `0.24.10` with additive event-dual-write, full-field replay-check, deterministic projection rebuild/apply, and event-first projection apply in-transaction across all mutation lanes (`add`, `pin`, `canonical`, `rollup`, `link-add`, `link-update`, `link-remove`): schema target v6, built-in v1->v2->v3->v4->v5->v6 migration on open, append-only `mem_event` table + indexes (`event_id`, `ts_ns`, `event_type`, `session_id`), `mem_cli event-list` for bounded event inspection, `mem_cli event-replay-check` for full-field parity checks between replayed event projection and live rows, `mem_cli event-replay-apply` for source->target projection rebuild + parity verification, snapshot-backed payload emission on key write commands, and `mem_cli event-backfill` for upgrading legacy minimal events into replay-complete history while preserving existing scoped retrieval, `mem_audit` coverage, session budgets, bounded neighbors, link constraints, dedupe-aware add behavior, and transactional rollup.
- `mem_console` (top-level program host) includes an optional (`--kernel-bridge`) evaluation scaffold over `core_sched` + `core_jobs` + `core_wake` + `core_kernel`; this is additive host validation and is not yet a production-app matrix requirement.
- `mem_console` (top-level program host) serializes theme/font UI prefs through `core_pack` (`<db_path>.ui.pack`) as app-local persistence.
- `mem_console` (top-level program host) now evaluates shared `core_pane` (`0.1.0`) for split-pane solve + draggable splitter interaction in the 15C lane, with pane-ratio persistence wired through app-local prefs packs.
- `core_pane` is now at `0.1.1` with additive invalid-graph and non-finite input hardening plus deterministic edge-case fixture coverage.
- `kit_pane` is now scaffolded at `0.1.0` with baseline pane chrome/splitter rendering helpers and null-backend unit tests; adoption into app hosts is deferred behind workspace sandbox gates.
- `workspace_sandbox` validation harness now includes a thin `core_action` trigger-to-action routing proof (non-app-adoption gate path) for mode/edit/module key commands.
- future-work note: `shared/docs/memory_db_autonomous_maintenance/` (Plan 01-03) is planning-only and not yet counted as implemented runtime/adoption state in this snapshot.
- detailed per-program connective-tissue snapshot now lives in `shared/docs/ecosystem_north_star_docs/44_program_library_connective_tissue_state.md`.
