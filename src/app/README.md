# app Module

This module owns `AppState` and coordinates all runtime subsystems each frame.
Its core job is to keep rendering, tile loading, and routing synchronized around user input.

## Files

- `app.c`: App initialization/shutdown and primary frame loop integration.
- `app_playback.c`: Route playback clock/state and playback marker rendering.
- `app_route.c`: Route graph load, nearest-node snapping, and route interaction helpers.
- `app_runtime_controls.c`: Global command/key handlers (including font/theme controls).
- `app_runtime_dispatch.c`: Frame dispatch seam across input/update/render phases.
- `app_runtime_events.c`: Input intake/normalize/route/invalidate top-level flow.
- `app_runtime_input_policy.c`: Text-entry shortcut gating policy helpers for normalize phase.
- `app_runtime_render.c`: Render-stage orchestration with explicit derive/submit phase contracts and frame-present boundaries.
- `app_runtime_update.c`: Update-stage orchestration for camera/state/queue/route sequencing.
- `app_tile_pipeline.c`: Tile queueing, load/integrate budgets, and layer readiness orchestration.
- `app_tile_presenter.c`: Tile presentation policy helpers (band blend + hold continuity + fallback tile resolution + road/polygon draw-path decisions + frame invariant validation).
- `app_tile_render.c`: Visible-tile rendering path and Vulkan asset-budgeted draw helpers.
- `app_ui.c`: Header/status UI rendering, hit-tests, and UI text/chip composition.
- `app_view.c`: View/camera helpers, zoom-to-tile mapping, region-fit behavior.
- `region.c`: Built-in region registry (runtime-selectable region identities/paths).
- `region_loader.c`: `meta.json` loader for region bounds/tile range/pyramid flags.

## How It Connects

- Uses `camera`, `map`, `render`, `route`, and `ui` modules every frame.
- Provides the top-level behavior users actually experience: pan/zoom, layer readiness, route editing/playback, and overlay diagnostics.
- Acts as the integration boundary between offline-built region packs and live runtime rendering.
