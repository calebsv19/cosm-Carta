#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ShapeLib/shape_core.h"

// Canonical, flattened geometry representation shared across programs.
// Paths contain only polylines (no handles) plus a closed flag.
typedef struct {
    float x;
    float y;
} ShapeAssetPoint;

typedef struct {
    bool               closed;
    size_t             point_count;
    ShapeAssetPoint   *points;
} ShapeAssetPath;

typedef struct {
    char             *name;  // optional; owned string
    size_t            path_count;
    ShapeAssetPath   *paths;
    int               schema; // version from JSON, default 1
} ShapeAsset;

typedef struct {
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    bool  valid;
} ShapeAssetBounds;

typedef struct {
    float margin_cells;    // padding (cells) around fitted shape when center_fit=true
    float stroke;          // line thickness in cells for open paths
    float max_error;       // flatten tolerance for conversions/raster hints (optional)
    float position_x_norm; // 0..1 target center (used when center_fit=false)
    float position_y_norm; // 0..1 target center (used when center_fit=false)
    float rotation_deg;    // rotation around shape bounds center
    float scale;           // uniform scale (used when center_fit=false; <=0 => 1)
    bool  center_fit;      // if true, auto-fit to grid with margin
} ShapeAssetRasterOptions;

// Load/save the canonical JSON format:
// {
//   "schema": 1,
//   "name": "asset_name",
//   "paths": [
//     { "closed": true, "points": [ { "x": 0, "y": 0 }, ... ] }
//   ]
// }
bool shape_asset_load_file(const char *path, ShapeAsset *out_asset);
bool shape_asset_save_file(const ShapeAsset *asset, const char *path);

// Compute bounds of the asset (in asset-local coordinates).
bool shape_asset_bounds(const ShapeAsset *asset, ShapeAssetBounds *out_bounds);

// Utilities for building an asset from editor-style ShapeLib shapes.
// Flattens Beziers to polylines using max_error and copies all paths.
bool shape_asset_from_shapelib_shape(const Shape *shape,
                                     float max_error,
                                     ShapeAsset *out_asset);

// Rasterize a ShapeAsset into a binary mask of size grid_w x grid_h.
bool shape_asset_rasterize(const ShapeAsset *asset,
                           int grid_w,
                           int grid_h,
                           const ShapeAssetRasterOptions *opts,
                           uint8_t *mask_out);

// Free all owned allocations inside an asset.
void shape_asset_free(ShapeAsset *asset);
