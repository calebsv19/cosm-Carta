# MapForge Summary

## Purpose
MapForge is an offline-first map viewer and route explorer for macOS (C99 + SDL). It ingests OpenStreetMap extracts via an offline build step to produce local region packs, then runs entirely from disk with smooth pan/zoom, road rendering, and local routing.

## Core Goals
- Offline-first: no server dependency at runtime.
- Focused scope: Seattle/Washington region first, then expand.
- Clear separation: tooling builds region packs; runtime app consumes them.
- Fast, responsive UX: smooth camera, fast tile loading, stable FPS.
- Extensible architecture: renderer abstraction (SDL now, Vulkan later) and routing profiles.

## What Users Can Do (V1)
- Open the app and view road data for a selected region.
- Pan and zoom smoothly with the slippy map model.
- Click to set A/B endpoints; drag to adjust.
- Compute routes locally (shortest or fastest by speed proxy).
- See route stats (distance, time proxy).

## Non-Goals for V1
- Live traffic or online services.
- Address search or geocoding.
- Planet-scale data or lane-level guidance.
- Satellite imagery or perfect labeling.

## Core Technical Choices
- Projection: Web Mercator + slippy tiles.
- Coordinate types: Lat/Lon for I/O, Mercator meters for geometry, screen pixels for rendering.
- Data model split: render tiles (vector features) and routing graph tiles.
- Tile format: custom binary MFT v1, quantized tile-local geometry.

## Data Pipeline (Offline Build)
- Parse OSM ways tagged highway.
- Convert geometry to Mercator meters.
- Slice and clip features into tiles.
- Build routing graph with directed edges and speed estimates.
- Output a region pack directory with render tiles and graph tiles.

## Runtime Subsystems
- App loop: input, camera update, tile cache, routing, render.
- Camera: world-to-screen transforms with zoom (float).
- Tile manager: load/evict tiles with LRU cache.
- Renderer API: draw polyline batches by class.
- Router: A* with distance or time cost profile.

## Long-Term Direction (North Star)
- Expand layers: buildings, water, parks, POIs.
- Add playback mode and advanced routing profiles.
- Integrate elevation and grade-aware routing.
- Optional Vulkan backend for GPU scalability.

## Guiding Invariants
- Region pack assets are read-only.
- Runtime caches own decoded tile memory.
- Avoid per-feature malloc churn; use arenas.
- Keep rendering decoupled from OSM tags.
- Keep routing decoupled from render styling.
