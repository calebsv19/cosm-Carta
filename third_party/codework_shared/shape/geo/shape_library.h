#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "geo/shape_asset.h"

typedef struct {
    ShapeAsset *assets;
    size_t      count;
} ShapeAssetLibrary;

// Load all *.asset.json files from a directory into a library.
// Returns true if directory exists and parsing succeeded for at least one asset.
bool shape_library_load_dir(const char *dir_path, ShapeAssetLibrary *out_lib);

const ShapeAsset *shape_library_get(const ShapeAssetLibrary *lib, size_t index);
const ShapeAsset *shape_library_find(const ShapeAssetLibrary *lib, const char *name);

void shape_library_free(ShapeAssetLibrary *lib);
