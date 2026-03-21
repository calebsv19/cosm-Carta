#include "app/region.h"

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *k_default_region_root = "data/regions";
enum {
    REGION_CATALOG_MAX = 256,
    REGION_NAME_CAPACITY = 128
};

typedef struct RegionCatalogEntry {
    RegionInfo info;
    char name[REGION_NAME_CAPACITY];
} RegionCatalogEntry;

static RegionCatalogEntry g_region_catalog[REGION_CATALOG_MAX];
static int g_region_catalog_count = 0;

static bool path_is_dir_local(const char *path) {
    struct stat st;
    if (!path || path[0] == '\0') {
        return false;
    }
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

static int region_catalog_entry_cmp(const void *left, const void *right) {
    const RegionCatalogEntry *a = (const RegionCatalogEntry *)left;
    const RegionCatalogEntry *b = (const RegionCatalogEntry *)right;
    return strcmp(a->name, b->name);
}

static int region_catalog_rebuild(void) {
    const char *root = region_data_root();
    DIR *dir = NULL;
    struct dirent *entry = NULL;

    g_region_catalog_count = 0;
    if (!root || root[0] == '\0') {
        return 0;
    }

    dir = opendir(root);
    if (!dir) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (g_region_catalog_count >= REGION_CATALOG_MAX) {
            break;
        }

        RegionCatalogEntry *slot = &g_region_catalog[g_region_catalog_count];
        memset(slot, 0, sizeof(*slot));

        if (snprintf(slot->name, sizeof(slot->name), "%s", entry->d_name) < 0) {
            continue;
        }
        if (slot->name[0] == '\0') {
            continue;
        }

        if (snprintf(slot->info.region_dir, sizeof(slot->info.region_dir), "%s/%s", root, slot->name) < 0) {
            continue;
        }
        if (!path_is_dir_local(slot->info.region_dir)) {
            continue;
        }

        if (snprintf(slot->info.tiles_dir, sizeof(slot->info.tiles_dir), "%s/tiles", slot->info.region_dir) < 0) {
            continue;
        }
        if (!path_is_dir_local(slot->info.tiles_dir)) {
            continue;
        }

        slot->info.name = slot->name;
        slot->info.tile_min_zoom = 10u;
        slot->info.tile_max_zoom = 18u;
        slot->info.tile_extent = 4096u;
        slot->info.has_tile_pyramid_roads = false;
        slot->info.has_tile_pyramid_buildings = false;
        slot->info.has_center = false;
        slot->info.has_bounds = false;
        slot->info.has_tile_range = false;

        g_region_catalog_count += 1;
    }

    closedir(dir);
    if (g_region_catalog_count > 1) {
        qsort(g_region_catalog,
              (size_t)g_region_catalog_count,
              sizeof(g_region_catalog[0]),
              region_catalog_entry_cmp);
    }
    return g_region_catalog_count;
}

int region_count(void) {
    return region_catalog_rebuild();
}

const RegionInfo *region_get(int index) {
    int count = region_catalog_rebuild();
    if (index < 0 || index >= count) {
        return NULL;
    }
    return &g_region_catalog[index].info;
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

bool region_has_graph(const RegionInfo *info) {
    char path[MAPFORGE_REGION_PATH_CAPACITY];
    struct stat st;
    if (!region_graph_path(info, path, sizeof(path))) {
        return false;
    }
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}
