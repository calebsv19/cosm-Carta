# Region Pack Layout (Phase 2)

Each region pack is a directory under `data/regions/<region_name>/` and is treated as read-only at runtime.

## Layout
- Legacy split layer tiles:
  - `data/regions/<region_name>/tiles/<z>/<x>/<y>.artery.mft`
  - `data/regions/<region_name>/tiles/<z>/<x>/<y>.local.mft`
  - `data/regions/<region_name>/tiles/<z>/<x>/<y>.water.mft`
  - `data/regions/<region_name>/tiles/<z>/<x>/<y>.park.mft`
  - `data/regions/<region_name>/tiles/<z>/<x>/<y>.landuse.mft`
  - `data/regions/<region_name>/tiles/<z>/<x>/<y>.building.mft`
  - `data/regions/<region_name>/tiles/<z>/<x>/<y>.contour.mft`
- Road tile pyramid bands (runtime path priority for roads):
  - `data/regions/<region_name>/tiles/bands/coarse/<z>/<x>/<y>.artery.mft`
  - `data/regions/<region_name>/tiles/bands/mid/<z>/<x>/<y>.artery.mft`
  - `data/regions/<region_name>/tiles/bands/fine/<z>/<x>/<y>.artery.mft`
  - `data/regions/<region_name>/tiles/bands/coarse/<z>/<x>/<y>.local.mft`
  - `data/regions/<region_name>/tiles/bands/mid/<z>/<x>/<y>.local.mft`
  - `data/regions/<region_name>/tiles/bands/fine/<z>/<x>/<y>.local.mft`
- Polygon pyramid bands (runtime path priority when present):
  - `data/regions/<region_name>/tiles/bands/coarse/<z>/<x>/<y>.water.mft`
  - `data/regions/<region_name>/tiles/bands/mid/<z>/<x>/<y>.water.mft`
  - `data/regions/<region_name>/tiles/bands/fine/<z>/<x>/<y>.water.mft`
  - `data/regions/<region_name>/tiles/bands/coarse/<z>/<x>/<y>.park.mft`
  - `data/regions/<region_name>/tiles/bands/mid/<z>/<x>/<y>.park.mft`
  - `data/regions/<region_name>/tiles/bands/fine/<z>/<x>/<y>.park.mft`
  - `data/regions/<region_name>/tiles/bands/coarse/<z>/<x>/<y>.landuse.mft`
  - `data/regions/<region_name>/tiles/bands/mid/<z>/<x>/<y>.landuse.mft`
  - `data/regions/<region_name>/tiles/bands/fine/<z>/<x>/<y>.landuse.mft`
  - `data/regions/<region_name>/tiles/bands/fine/<z>/<x>/<y>.building.mft` (conservative rollout)
- `data/regions/<region_name>/meta.json`

## meta.json
Example:
```
{
    "region": "seattle",
    "source": "Seattle OSM extract",
    "created_utc": "2026-02-04T00:00:00Z",
    "bounds": {
        "min_lat": 47.48,
        "min_lon": -122.45,
        "max_lat": 47.75,
        "max_lon": -122.20
    },
    "tile": {
        "min_z": 10,
        "max_z": 14,
        "extent": 4096
    },
    "tile_pyramid": {
        "roads": {
            "enabled": true
        }
    }
}
```

## Notes
- Runtime loader checks banded road path first, then falls back to legacy split-layer tile path.
- Runtime loader checks banded layer path first, then falls back to legacy split-layer tile path.
- The app only reads region packs; it never mutates them.
