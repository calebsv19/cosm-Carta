# Shared Shape Stack

Canonical ShapeLib + ShapeAsset sources live here and are forwarded into the three projects.

- Library: `ShapeLib/` (Bezier model, flattening, JSON), `geo/` (ShapeAsset JSON/raster), `tests/` (sanity helper).
- Default asset root: `shared/assets/shapes` (override with `SHAPE_ASSET_DIR`).
- Common env: `SHAPE_ASSET_DIR=/abs/path/to/shared/assets/shapes` before launching any app or converter.

## Converting line_drawing exports
1) Build a converter:
   - `cd ray_tracing && make cli-tools` (preferred), or `cd physics_sim && make shape_asset_tool`.
2) Run the sync helper (defaults to `line_drawing/export -> shared/assets/shapes`):
   ```sh
   SHAPE_ASSET_DIR=shared/assets/shapes shared/shape/sync_exports.sh
   ```
   Pass a custom export dir as the first arg if needed. A manifest is written to `manifest.json` in the output dir.
3) (Optional) Build a sanity tool and rerun the sync; it will auto-run and report bounds/raster success.

Each project reads `SHAPE_ASSET_DIR` if set; otherwise it falls back to its legacy local `config/objects` or `Configs/objects`.
