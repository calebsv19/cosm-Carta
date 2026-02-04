# MapForge North Star Plan

This document lays out the long-term plan by layers and phases. It is intended as a durable reference for scope control, sequencing, and architectural intent.

## 1) North Star Vision
MapForge is an offline-first, high-performance map viewer and route explorer for macOS. It converts OSM extracts into local region packs and delivers smooth navigation, high-quality map rendering, and robust routing without server dependencies.

## 2) Strategic Layers (Long-Term Behavior Goals)

### Layer A: Data & Tooling
Goal: Deterministic, reproducible region pack builds from raw OSM data.
- A1. OSM ingestion: parse ways and tags with stable classification rules.
- A2. Tile slicing: consistent clipping, quantization, and tile boundaries.
- A3. Graph building: intersection detection and edge creation with correct oneway handling.
- A4. Spatial indexing: fast nearest-edge snapping with tile or grid indexes.
- A5. Packaging: stable region pack structure (later single-file pack).

### Layer B: Runtime Map Core
Goal: Fast, smooth map navigation with reliable tile lifecycle.
- B1. Camera: precise Mercator transforms; zoom across scales.
- B2. Tile manager: LRU cache, predictable memory usage, no stalls.
- B3. Feature decoding: contiguous buffers, minimal allocations, correct transforms.
- B4. Renderer abstraction: minimal API and backend interchangeability.

### Layer C: Routing Core
Goal: Local routing that is correct, fast, and profile-driven.
- C1. A* performance: robust heuristic, efficient graph loading.
- C2. Profiles: shortest vs fastest with clean weighting logic.
- C3. Snapping: stable endpoint snapping with clear user feedback.
- C4. Output: route polyline + stats with deterministic results.

### Layer D: UX & Interaction
Goal: Minimal, clear, and responsive user interaction.
- D1. Inputs: pan, zoom, click, drag endpoints.
- D2. UI panels: profile selector, stats, debug overlay.
- D3. Playback: route traversal visualization and timing.

### Layer E: Extended Map Features
Goal: Expand from roads to a full visual map experience.
- E1. Polygons: buildings, water, parks, landuse.
- E2. Labels: optional text, basic priority and zoom gating.
- E3. Styling: themeable per-layer and per-zoom.

### Layer F: Advanced Routing & Terrain
Goal: Support elevation-aware and mode-specific routing.
- F1. DEM ingestion and per-edge ascent/descent metrics.
- F2. Routing profiles: flat vs hilly vs minimize grade.
- F3. Surface and access tags: bike/ped/vehicle policies.

### Layer G: Rendering Backends
Goal: Maintain portability while enabling higher performance.
- G1. SDL renderer as baseline.
- G2. Vulkan renderer as a drop-in backend.
- G3. Shared render API and stable draw semantics.

## 3) Phase Plan (Execution Roadmap)

### Phase 0: Engine Skeleton
Outcome: Bootable app with basic rendering loop.
- SDL window + main loop.
- Camera pan/zoom controls.
- Renderer abstraction with SDL backend.
- Debug overlay (FPS, dt).

### Phase 1: Minimal Map Viewer (Roads Only)
Outcome: Real-time map viewing with road tiles.
- Tile math for slippy coordinates.
- Tile cache with LRU eviction.
- MFT loader and polyline renderer.
- Zoom-based line widths and class styling.

### Phase 2: OSM Region Pack Toolchain (Roads)
Outcome: Build Seattle region pack from OSM extract.
- OSM importer for highway ways.
- Tile slicing and MFT output.
- Makefile target for region build.

### Phase 3: Routing v1
Outcome: A/B endpoints and local routing.
- Graph builder tool.
- Runtime A* with shortest and fastest profiles.
- Snap endpoints + route polyline rendering.
- Route stats panel.

### Phase 4: UX Enhancements & Playback
Outcome: Usability improvements and route playback.
- Drag endpoints and improve interaction.
- Playback mode with time proxy.
- UI polish and debug overlays.

### Phase 5: Additional Map Layers
Outcome: Buildings, water, parks, landuse.
- Extend MFT to polygons.
- Polygon renderer and style system.
- Layer toggles or zoom gating.

### Phase 6: Elevation Modes
Outcome: Elevation-aware routing.
- DEM ingestion pipeline.
- Per-edge elevation metrics.
- New routing profiles and UI controls.

### Phase 7: Vulkan Backend (Optional)
Outcome: Renderer backend swap without feature loss.
- Vulkan implementation of renderer API.
- Performance tuning for large tile loads.

## 4) Success Criteria
- Tiles load smoothly with no hitching during pan/zoom.
- Routing returns deterministic results for identical inputs.
- Region pack builds are repeatable from same OSM extract.
- Clear separation between rendering, routing, and data tooling.
- Milestones demonstrate visible progress at each phase.

## 5) Risks & Mitigations
- Large OSM extracts: mitigate with region scope and tile caching.
- Memory bloat: use quantization and arenas for decoded features.
- Routing correctness: validate graph build with small test regions.
- Tooling complexity: keep MFT format stable until later phases.

## 6) Immediate Next Steps (If Starting Today)
- Implement Phase 0 skeleton.
- Confirm MFT v1 binary layout and quantization strategy.
- Build a dummy tile generator to validate render pipeline.
