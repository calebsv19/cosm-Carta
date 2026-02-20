# map Module

This module owns map-domain logic: coordinate systems, tile formats, tile IO/cache, and layer-specific render policies.

## Files

- `layer_policy.c`: Centralized per-layer policy (activation, budgets, zoom bands, Vulkan-specific gates).
- `map_space.c`: Tile-local to world/screen transform helpers.
- `mercator.c`: Lat/lon <-> Web Mercator conversions and world-size constants.
- `mft_loader.c`: Decoder for MFT tile files (roads/polygons/variants).
- `polygon_cache.c`: Preprocessing/caching helpers for polygon data.
- `polygon_renderer.c`: Polygon layer draw logic (water/park/landuse/building behavior).
- `polygon_triangulator.c`: Polygon triangulation helper for fill generation.
- `road_renderer.c`: Road polyline draw logic and class/tier-aware filtering stats.
- `tile_loader.c`: Background threaded tile loader with request/result queues.
- `tile_manager.c`: In-memory LRU tile cache and cache lookup/insert logic.
- `tile_math.c`: Slippy-tile coordinate math and tile origin/size helpers.
- `zoom_fade.c`: Zoom tier and fade alpha helpers.

## How It Connects

- Consumes region-pack files created by offline tooling and exposes decoded tile data to the app/render path.
- Provides the math and policies that keep large-region rendering stable and predictable.
- Serves as the boundary between raw tile payloads and draw-ready structures.

