# Phase 2 Plan: OSM Region Pack Toolchain (Roads)

Status key: [ ] pending, [x] completed

1) [x] Define region pack directory layout
- data/regions/<name>/tiles/<z>/<x>/<y>.mft
- data/regions/<name>/meta.json (bounds, source, build info)
- Keep runtime as read-only

2) [x] OSM ingestion (roads only)
- Parse ways + tags needed for highway classification
- Extract node coordinates for each way
- Handle oneway flags minimally for later graph build

3) [x] Road classification rules
- Map OSM highway tags to RoadClass values
- Define default handling for missing/unknown tags

4) [x] Tile slicing + clipping
- Convert lat/lon to Mercator meters
- Clip polylines to tile bounds
- Quantize to 0..4096 tile-local coordinates

5) [x] MFT writer
- Serialize MFT v1 header + polyline records
- Ensure stable ordering for determinism
- Emit one file per tile

6) [x] Tool binary + CLI
- tools/mapforge_region tool (C99)
- Args: REGION=..., OSM=..., OUT=...
- Emits region pack directory

7) [x] Makefile integration
- make tools builds tool binaries
- make region REGION=... OSM=... builds region pack

8) [x] Phase 2 acceptance check
- Build Seattle region pack from OSM extract
- Load tiles in app and render roads
- Repeat build gives identical output
