# MapForge Runtime Architecture

This document describes the runtime frame loop, state ownership, and data flow at a level intended for contributors.

## Runtime Frame Phases

Primary runtime orchestration lives in `src/app/app.c`, with phase slices extracted into dedicated modules:

1. `app_runtime_begin_frame(...)`
2. `app_runtime_handle_global_controls(...)`
3. `app_runtime_update_frame(...)`
4. `app_runtime_render_frame(...)`
5. trace/metrics emission and frame bookkeeping

## Update Stage Responsibilities

`src/app/app_runtime_update.c` owns mutation-heavy logic:

- camera/input updates
- route worker poll/submit flow
- tile queue update and result integration
- Vulkan poly/asset queue integration
- HUD/controller click handling
- route panel model snapshot update (`app_route_panel_model_update`)

The update stage is where state should change.

## Render Stage Responsibilities

`src/app/app_runtime_render.c` owns drawing only:

- tile/layer rendering
- route rendering
- hover/playback markers
- route panel/header/debug overlay draw
- present/window title update

Render stage should consume state prepared by update stage, not own game-state transitions.

## State Ownership Model

`AppState` is split into domain buckets (`include/app/app_internal.h`):

- `AppViewState`: camera/zoom/layer visual control state
- `AppTileState`: tile visibility/readiness/cache and Vulkan tile artifacts
- `AppRouteRuntimeState`: route graph, endpoints, alternatives, playback, hover/snap
- `AppWorkerState`: worker threads, queues, request generations, counters
- `AppUiState`: input + HUD model/layout/cache fields

## Route Service Boundary

Route selection/toggle behavior is centralized in `src/app/app_route_service.c`:

- `app_route_service_select_alternative`
- `app_route_service_toggle_alternative_visibility`

HUD click handlers call this service instead of mutating route internals inline.

## Practical Rule

If a change introduces a new UI interaction:

1. apply state mutation in update/controller code
2. update any HUD model snapshot in update
3. keep draw path as a consumer of prepared state
