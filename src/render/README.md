# render Module

This module implements rendering backends and backend-specific retained-cache behavior.

## Files

- `sdl_renderer.c`: Renderer abstraction implementation, backend selection, and SDL/Vulkan frame primitives.
- `vk_tile_cache.c`: Vulkan retained per-tile mesh cache, LRU behavior, and mesh lifecycle helpers.

## How It Connects

- App/map/route/UI code call renderer APIs without hard-coding backend details.
- Vulkan retained cache reduces repeated CPU/GPU work for large tile sets.

