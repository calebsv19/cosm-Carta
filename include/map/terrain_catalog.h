#ifndef MAPFORGE_TERRAIN_CATALOG_H
#define MAPFORGE_TERRAIN_CATALOG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TERRAIN_CATALOG_CAPACITY 9
#define TERRAIN_AREA_BUDGET (32u * 1024u * 1024u)
#define TERRAIN_ADDITIONAL_BUDGET (128u * 1024u * 1024u)
// Describes one verified background area independently of routes and personal history.
typedef struct TerrainArea {
    char id[64], name[96], raster_path[4096], vector_path[4096], source[256];
    double extent[4], resolution_m;
    size_t bytes;
    bool removable;
} TerrainArea;
// Retains one bounded catalog plus the immutable overview and an optimistic edit token.
typedef struct TerrainCatalog {
    TerrainArea areas[TERRAIN_CATALOG_CAPACITY];
    size_t count, additional_bytes;
    char region_root[4096], storage_root[4096];
    uint64_t token;
} TerrainCatalog;
// Loads a bounded catalog transactionally; an absent additional catalog keeps the base overview.
bool terrain_catalog_load(TerrainCatalog *catalog, const char *region_root, const char *storage_root,
                          bool base_only, char *error, size_t error_size);
// Selects the best intersecting area with deterministic coverage/resolution ties, or no coverage.
int terrain_catalog_select(const TerrainCatalog *catalog, double xmin, double ymin,
                           double xmax, double ymax);
// Removes only one explicitly confirmed owned patch after stale-token and exact-file validation.
bool terrain_catalog_remove(TerrainCatalog *catalog, size_t index, char *error, size_t error_size);
#endif
