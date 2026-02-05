# Region Pack Layout (Phase 2)

Each region pack is a directory under `data/regions/<region_name>/` and is treated as read-only at runtime.

## Layout
- `data/regions/<region_name>/tiles/<z>/<x>/<y>.mft`
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
    }
}
```

## Notes
- The toolchain writes one `.mft` file per tile.
- The app only reads region packs; it never mutates them.
