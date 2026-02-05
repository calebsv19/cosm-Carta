# Phase 1 Plan: Minimal Map Viewer (Roads Only)

Status key: [ ] pending, [x] completed

1) [x] Confirm MFT v1 tile layout + quantization strategy
- Define tile-local coordinate ranges and quantization bits
- Define polyline encoding (counts, points, road class)
- Write a short format note in docs/

2) [x] Tile math + coordinate utilities
- Slippy tile math helpers (z/x/y, world meters)
- Web Mercator conversions (lat/lon ↔ meters)
- Screen/world transforms (camera zoom + center)

3) [x] Tile manager + LRU cache
- Tile key struct (z/x/y, layer)
- LRU cache for decoded tiles
- Predictable memory cap + eviction

4) [x] MFT tile loader (runtime)
- Read MFT header + polyline batches
- Decode into contiguous buffers
- Minimal error handling + logging

5) [x] Road renderer
- Batch polyline draw by class
- Zoom-based line widths
- Basic style palette (class → color)

6) [x] Integration into app loop
- Request visible tiles based on camera
- Load tiles async or synchronous stub
- Render visible tile set

7) [x] Debug overlay upgrades
- Show FPS, tile count, cache size (window title)
- Toggle overlay on key (F1)

8) [x] Phase 1 acceptance check
- Pan/zoom smooth
- Roads render from a small test tile (data/regions/seattle/tiles/12/2048/2048.mft)
- No stalls on tile load (even if synchronous)
