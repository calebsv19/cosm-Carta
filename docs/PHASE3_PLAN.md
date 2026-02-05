# Phase 3 Plan: Routing v1

Status key: [ ] pending, [x] completed

1) [x] Define graph v1 format
- Graph file layout (nodes, edges, adjacency offsets)
- Store per-edge length + speed
- Document in docs/

2) [x] Graph builder tool (offline)
- Parse OSM ways + oneway
- Build node list + edge list
- Write graph file to data/regions/<name>/graph/graph.bin

3) [x] Runtime graph loader
- Load graph file into memory
- Provide node lookup and edge traversal

4) [x] Snap endpoints
- Convert screen click to world coordinates
- Find nearest graph node

5) [x] A* routing
- Shortest distance profile
- Fastest time profile using speed proxy
- Return polyline of node coordinates

6) [x] Route rendering
- Draw route polyline on top of roads
- Distinct color + width

7) [x] UI and stats
- Left click sets A, right click sets B
- Key toggle for profile (shortest/fastest)
- Show route distance + time in title

8) [x] Phase 3 acceptance check
- A/B endpoints set and snapped
- Route renders for at least one region
- Profile toggle changes route cost

# Phase 3.5 Plan: Zoom-Tiered Rendering

Status key: [ ] pending, [x] completed

1) [x] Define zoom tiers + road visibility
- Far/mid/near tiers with class filters

2) [x] Decide render mode per road class + tier
- Quad strips only for thick roads at close zoom
- Footpaths remain polylines

3) [x] Centralize style + width scaling
- One resolver for color/width/mode
- Extra thinning for far zoom

4) [x] Split draw pipeline by mode
- Polyline path for thin roads
- Quad strip path for thick roads

5) [x] Add tier selection helper
- Shared helper for tier lookup

6) [x] Optional debug tier display
- Show current tier for validation

7) [ ] Validate tier behavior
- Far: majors only, thin lines
- Mid: add secondaries
- Near: add locals + paths

8) [x] Log completion in this doc
