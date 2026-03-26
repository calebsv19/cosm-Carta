# MapForge Contributor Module Map

Use this map to find where to make changes safely.

## Core App Runtime

- `src/app/app.c`
  - app bootstrap/shutdown orchestration
  - top-level runtime loop composition

- `src/app/app_runtime_events.c`
  - per-frame SDL/event polling entrypoints

- `src/app/app_runtime_controls.c`
  - global key controls and mode toggles

- `src/app/app_runtime_update.c`
  - update-stage integration and most state mutation

- `src/app/app_runtime_render.c`
  - render-stage draw orchestration

## Route Domain

- `src/app/app_route.c`
  - route graph load, worker thread loop, snap index, result poll/apply

- `src/app/app_route_service.c`
  - route alternative selection/toggle behavior
  - UI/controller-safe route mutation entrypoints

- `src/route/*`
  - graph/path algorithms and render support

## Tile/Vulkan Domain

- `src/app/app_tile_pipeline.c`
  - tile queue update, Vulkan prep/asset worker flows, queue metrics

- `src/app/app_tile_render.c`
  - visible tile draw paths by layer/band

- `src/map/*`
  - tile formats, loaders, cache, road/polygon rendering, zoom policies

## UI/HUD Domain

- `src/app/app_ui.c`
  - header/layer controls, debug overlay rendering/click behavior

- `src/app/app_playback.c`
  - route panel model + rendering, playback marker/time controls

## “Where To Edit” Quick Guide

Add keybind/global control:

- `src/app/app_runtime_controls.c`
- update `docs/KEYBINDS.md`

Change route panel interactions:

- controller behavior in `src/app/app_playback.c`
- route mutations in `src/app/app_route_service.c`

Change async worker semantics:

- `src/app/app_worker_contract.c`
- queue code in `src/app/app_tile_pipeline.c` / `src/app/app_route.c`

Change layer visibility/fade behavior:

- `src/map/layer_policy.c`
- `src/map/zoom_fade.c`
- header interactions in `src/app/app_ui.c`

## Safe Refactor Guardrails

1. Preserve update-before-render ordering.
2. Keep draw path state mutations minimal.
3. Keep worker generation checks centralized.
4. Prefer app-local extraction before shared-core promotion.
5. Run non-GUI verification (`make test`) before handing off.
