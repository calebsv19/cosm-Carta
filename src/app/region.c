#include "app/region.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *k_default_region_root = "data/regions";

static const RegionInfo kRegions[] = {
    {"sample", "", "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false, false},
    {"seattle_small", "", "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false, false},
    {"seattle_medium", "", "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false, false},
    {"seattle_large", "", "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false, false},
    {"new_york", "", "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false, false},
    {"rome", "", "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false, false},
    {"poulsbo", "", "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false, false},
    {"puyallup", "", "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false, false},
    {"puget_sound", "", "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false, false},
    {"planet_122_416_47_494_122_214_47_83", "", "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false, false}
};

int region_count(void) {
    return (int)(sizeof(kRegions) / sizeof(kRegions[0]));
}

const RegionInfo *region_get(int index) {
    if (index < 0 || index >= region_count()) {
        return NULL;
    }

    return &kRegions[index];
}

const char *region_data_root(void) {
    const char *value = getenv("MAPFORGE_REGIONS_DIR");
    if (!value || value[0] == '\0') {
        return k_default_region_root;
    }
    return value;
}

bool region_resolve_paths(RegionInfo *info) {
    const char *root = NULL;
    int n = 0;
    if (!info || !info->name || info->name[0] == '\0') {
        return false;
    }
    root = region_data_root();
    n = snprintf(info->region_dir, sizeof(info->region_dir), "%s/%s", root, info->name);
    if (n < 0 || (size_t)n >= sizeof(info->region_dir)) {
        info->region_dir[0] = '\0';
        info->tiles_dir[0] = '\0';
        return false;
    }
    n = snprintf(info->tiles_dir, sizeof(info->tiles_dir), "%s/tiles", info->region_dir);
    if (n < 0 || (size_t)n >= sizeof(info->tiles_dir)) {
        info->tiles_dir[0] = '\0';
        return false;
    }
    return true;
}

bool region_meta_path(const RegionInfo *info, char *out_path, size_t out_size) {
    int n = 0;
    if (!info || !out_path || out_size == 0u || info->region_dir[0] == '\0') {
        return false;
    }
    n = snprintf(out_path, out_size, "%s/meta.json", info->region_dir);
    if (n < 0 || (size_t)n >= out_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

bool region_graph_path(const RegionInfo *info, char *out_path, size_t out_size) {
    int n = 0;
    if (!info || !out_path || out_size == 0u || info->region_dir[0] == '\0') {
        return false;
    }
    n = snprintf(out_path, out_size, "%s/graph/graph.bin", info->region_dir);
    if (n < 0 || (size_t)n >= out_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}
