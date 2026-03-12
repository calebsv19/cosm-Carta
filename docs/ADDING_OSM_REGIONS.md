# Adding OSM Regions

This guide explains how to add a new region from an OSM download so MapForge can build and run it safely.

## 1) Download The Correct File Type
- MapForge region build currently reads **plain OSM XML** input.
- Use files with `.osm` extension (for example `puyallup.osm`).
- Do not pass `.pbf`, `.osm.pbf`, `.gz`, or `.bz2` directly to `make region`.

## 2) Name Files And Region IDs
- OSM source file naming recommendation:
  - lowercase
  - letters/numbers plus `_` or `-`
  - example: `puyallup.osm`
- Region name (`REGION=...`) must be filesystem-safe. Use:
  - letters/numbers plus `_`, `-`, or `.`
  - example: `puyallup`
- Keep OSM filename and region name aligned when possible to reduce operator mistakes.

## 3) Place The OSM File
- Recommended location in this repo:
  - `map_forge/data/osm_sources/<name>.osm`
- Example:
  - `map_forge/data/osm_sources/puyallup.osm`

## 4) Build Region Tiles
From `map_forge/`:

```bash
make region REGION=puyallup OSM=/absolute/path/to/puyallup.osm MIN_Z=10 MAX_Z=13
```

Notes:
- `MIN_Z`/`MAX_Z` control how much data is generated and disk usage.
- The region pack is written under:
  - `map_forge/data/regions/puyallup/`

## 5) Optional: Build Route Graph
If you want routing for that region:

```bash
make route REGION=puyallup OSM=/absolute/path/to/puyallup.osm
```

## 6) Make Region Selectable In App
- No code edit is required.
- Runtime region choices are auto-discovered from the region root directory:
  - default: `data/regions/`
  - override: `MAPFORGE_REGIONS_DIR`
- Any folder with a valid `tiles/` subtree is included in the in-app region cycle (`F3`).

## 7) Validate Output
- Confirm pack exists:
  - `data/regions/<region>/meta.json`
  - `data/regions/<region>/tiles/...`
- Run app:

```bash
make run
```

## 8) Common Failures
- `Failed to open OSM file`: check absolute path and file extension.
- Empty or partial render: check zoom range and confirm OSM extract actually covers your area.
- Region not switchable in app: ensure the region directory exists under the active region root and includes a `tiles/` subtree.
