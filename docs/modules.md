# Shared Modules Map

## Core Modules
- `core_base`: low-level shared primitives and utilities.
- `core_io`: shared file/byte IO boundary.
- `core_data`: canonical in-memory data model.
- `core_pack`: versioned binary container and interchange.
- `core_scene`: scene bundle/load contract boundary.
- `core_space`: coordinate-space conversion and placement contracts.
- `core_viewport2d`: shared 2D viewport/camera transforms for fit, pan, and anchor-preserving zoom.
- `core_trace`: trace/event timeline capture contracts.
- `core_math`: generic deterministic math primitives.
- `core_sim`: simulation control-plane cadence, ordered pass semantics, and UI-free frame diagnostics helpers.
- `core_sim_trace`: optional `core_sim` to `core_trace` control-plane trace adapter.
- `core_theme`: semantic theme token model.
- `core_font`: font role/preset contract model.
- `core_time`: monotonic time reads.
- `core_queue`: queue ownership and bounded queue primitives.
- `core_sched`: deadline/scheduling primitives.
- `core_jobs`: main-thread budgeted job execution.
- `core_workers`: fixed worker-pool runtime.
- `core_wake`: wait/signal wake bridge.
- `core_kernel`: runtime phase orchestration.
- `core_memdb`: shared durable memory DB/storage layer.

## Kit Modules
- `kit_viz`: common visualization API helpers.
- `kit_render`: renderer abstraction and backend bridge surface.
- `kit_ui`: shared immediate UI toolkit surface.
- `kit_graph_timeseries`: time-series graph widgets/helpers.
- `kit_graph_struct`: structural graph layout/interaction helpers.

## Support Modules
- `sys_shims`: system include compatibility overlays.
- `shape`: shape import/export and shared shape geometry helpers.
- `vk_renderer`: Vulkan renderer backend implementation.
- `timer_hud`: shared frame timing and HUD utilities.

## Showcase and Program Hosts
- `showcase/kit_visual_showcase`: visual kit integration demo.
- `mem_console`: memory DB console program host.
