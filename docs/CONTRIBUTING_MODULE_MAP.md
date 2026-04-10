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

- `src/app/route/app_route.c`
  - route graph load, snap index, worker result poll/apply orchestration

- `src/app/route/app_route_service.c`
  - route alternative selection/toggle behavior
  - UI/controller-safe route mutation entrypoints

- `src/app/route/app_route_worker_lifecycle.c`
  - route worker init/shutdown lifecycle and recompute scheduling

- `src/app/route/app_route_*.c`
  - focused route helper seams (`anchor_query`, `hover`, `result_apply`, `graph_result`)

- `src/route/*`
  - graph/path algorithms and render support

## Tile/Vulkan Domain

- `src/app/app_tile_pipeline.c`
  - tile queue update, Vulkan prep/asset worker flows, queue metrics

- `src/app/app_tile_render.c`
  - visible tile draw paths by layer/band

- `src/render/vk_tile_cache.c`
  - Vulkan tile-cache residency, mesh build/release, and entry lifecycle

- `src/render/vk_tile_cache_policy.c`
  - cache slot selection/eviction policy and per-layer residency floor guards

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
- route mutations in `src/app/route/app_route_service.c`

Change async worker semantics:

- `src/app/app_worker_contract.c`
- queue code in `src/app/app_tile_pipeline.c`
- route worker scheduling/lifecycle in `src/app/route/app_route_worker_lifecycle.c`

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
