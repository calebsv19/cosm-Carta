#include "shape_sanity.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#endif

#include "../geo/shape_library.h"
#include "../geo/shape_asset.h"

int shape_sanity_check(const char *asset_dir, FILE *out_log) {
    const char *dir = (asset_dir && asset_dir[0]) ? asset_dir : "config/objects";
    ShapeAssetLibrary lib = {0};
    if (!shape_library_load_dir(dir, &lib)) {
        if (out_log) fprintf(out_log, "[sanity] Failed to load assets from %s\n", dir);
        return 1;
    }
    const ShapeAsset *asset = shape_library_get(&lib, 0);
    if (!asset) {
        if (out_log) fprintf(out_log, "[sanity] No assets in %s\n", dir);
        shape_library_free(&lib);
        return 2;
    }

    ShapeAssetBounds b = {0};
    if (!shape_asset_bounds(asset, &b) || !b.valid) {
        if (out_log) fprintf(out_log, "[sanity] Invalid bounds\n");
        shape_library_free(&lib);
        return 3;
    }

    uint8_t mask[8 * 8];
    ShapeAssetRasterOptions opts = {
        .margin_cells = 1.0f,
        .stroke = 1.0f,
        .max_error = 0.5f,
        .position_x_norm = 0.5f,
        .position_y_norm = 0.5f,
        .rotation_deg = 0.0f,
        .scale = 1.0f,
        .center_fit = true
    };
    if (!shape_asset_rasterize(asset, 8, 8, &opts, mask)) {
        if (out_log) fprintf(out_log, "[sanity] Rasterize failed\n");
        shape_library_free(&lib);
        return 4;
    }

    size_t filled = 0;
    for (size_t i = 0; i < sizeof(mask); ++i) {
        filled += mask[i] ? 1 : 0;
    }

    // Round-trip save/load into memory buffer on disk.
    const char *temp_dir = "build";
#ifdef _WIN32
    _mkdir(temp_dir);
#else
    mkdir(temp_dir, 0755);
#endif
    const char *temp_path = "build/shape_sanity.asset.json";
    bool saved = shape_asset_save_file(asset, temp_path);
    ShapeAsset reloaded = {0};
    bool loaded = saved ? shape_asset_load_file(temp_path, &reloaded) : false;
    size_t reloaded_paths = reloaded.path_count;
    shape_asset_free(&reloaded);

    shape_library_free(&lib);

    if (out_log) {
        fprintf(out_log, "[sanity] dir=%s bounds=[%.2f %.2f %.2f %.2f] filled=%zu saved=%d loaded=%d paths=%zu\n",
                dir, b.min_x, b.min_y, b.max_x, b.max_y, filled, saved ? 1 : 0, loaded ? 1 : 0, reloaded_paths);
    }

    return (filled == 0 || !saved || !loaded) ? 5 : 0;
}
