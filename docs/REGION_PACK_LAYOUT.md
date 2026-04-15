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
  - `data/regions/<region_name>/tiles/bands/coarse/<z>/<x>/<y>.building.mft`
  - `data/regions/<region_name>/tiles/bands/mid/<z>/<x>/<y>.building.mft`
  - `data/regions/<region_name>/tiles/bands/fine/<z>/<x>/<y>.building.mft`
- `data/regions/<region_name>/meta.json`
- Optional archive payload (when build uses archive emit lane):
  - `data/regions/<region_name>/tiles.mbtiles` (or custom `--archive-path` value)

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
    "tile_store": {
        "kind": "filesystem_tree",
        "root": "tiles"
    },
    "tile_pyramid": {
        "roads": {
            "enabled": true
        },
        "buildings": {
            "enabled": true
        }
    }
}
```

`tile_store.kind` values:
- `filesystem_tree` (current runtime path): reads from `tile_store.root` (default `tiles`).
- `archive_indexed` (runtime-supported):
  - archive fetch path is enabled for all current layer kinds (`road_artery`, `road_local`, `contour`, `water`, `park`, `landuse`, `building`) via SQLite-backed archive reads.
  - when archive reads miss, runtime falls back to `tile_store.root` if present.

Archive metadata keys:
- `tile_store.archive_path` (for example `tiles.pmtiles` or `tiles.mbtiles`; SQLite-backed archive read path expects tile blobs in `mapforge_tiles`/`tiles` rows)

## Notes
- Runtime loader checks banded road path first, then falls back to legacy split-layer tile path.
- Runtime loader checks banded layer path first, then falls back to legacy split-layer tile path.
- Region metadata now carries an explicit tile storage contract (`tile_store`) so runtime can migrate to archive-backed readers without changing region catalog/discovery shape.
- Region open path now validates package contract/artifacts before activation. CLI validator:
  - `make -C map_forge region-validate`
  - `make -C map_forge region-validate REGION=<name>`
  - `make -C map_forge region-validate STRICT=1` (treat archive->tree fallback as failure)
- Region build archive emit lane:
  - direct CLI: `build/tools/mapforge_region ... --emit-archive [--archive-path tiles.mbtiles]`
  - make lane: `make -C map_forge region REGION=<name> OSM=<file> EMIT_ARCHIVE=1 [ARCHIVE_PATH=tiles.mbtiles]`
  - metrics gate lane: `make -C map_forge metrics-rollup-gate` (fixture build + rollup validation)
- Build diagnostics now include archive storage rollups by `band x layer` in:
  - `meta.json` -> `output_stats.archive_rollups`
  - `meta.dataset.json` -> `map_forge_archive_rollups_v1`
- The app only reads region packs; it never mutates them.
