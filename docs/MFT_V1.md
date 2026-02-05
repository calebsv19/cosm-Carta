# MFT v1 (Roads Only)

This is the minimal MapForge Tile (MFT) format used in Phase 1. Each file stores one slippy tile of polyline road features.

## Coordinate Space
- Projection: Web Mercator meters (EPSG:3857).
- Tile-local coordinates are quantized to an integer grid.
- Tile extent: `4096` units per side.
- Conversion from meters to tile-local:
  - Compute tile origin in meters.
  - `u = (x_meters - tile_origin_x) / tile_size_meters`
  - `v = (tile_origin_y - y_meters) / tile_size_meters`
  - `x_q = round(u * 4096)`
  - `y_q = round(v * 4096)`

## File Layout (Little Endian)

Header:
- `char[4] magic` = "MFT1"
- `uint16 version` = 1
- `uint16 tile_z`
- `uint32 tile_x`
- `uint32 tile_y`
- `uint32 polyline_count`

Polyline records (repeated `polyline_count` times):
- `uint8 road_class`
- `uint32 point_count`
- `uint16 points[point_count][2]` (x_q, y_q)

## Road Class Values
- 0: motorway
- 1: trunk
- 2: primary
- 3: secondary
- 4: tertiary
- 5: residential
- 6: service
- 7: path (footway/cycleway/pedestrian)

## Notes
- Tile-local Y is inverted (north is smaller Y in tile space).
- Quantization assumes a uniform grid per tile; all coordinates are clamped to [0, 4096].
- Later versions can add per-feature metadata or polygon layers.
