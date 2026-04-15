#include "app/region_loader.h"

#include "core/log.h"
#include "core_io.h"

#include <json-c/json.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static bool json_get_double_obj(struct json_object *obj, const char *key, double *out_value) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value)) {
        return false;
    }
    if (!json_object_is_type(value, json_type_double) &&
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out_value = json_object_get_double(value);
    return true;
}

static bool json_get_u32_obj(struct json_object *obj, const char *key, uint32_t *out_value) {
    struct json_object *value = NULL;
    int64_t v = 0;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value)) {
        return false;
    }
    if (!json_object_is_type(value, json_type_int)) {
        return false;
    }
    v = json_object_get_int64(value);
    if (v < 0 || v > 0xFFFFFFFFLL) {
        return false;
    }
    *out_value = (uint32_t)v;
    return true;
}

static bool json_get_u64_obj(struct json_object *obj, const char *key, uint64_t *out_value) {
    struct json_object *value = NULL;
    int64_t v = 0;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value)) {
        return false;
    }
    if (!json_object_is_type(value, json_type_int)) {
        return false;
    }
    v = json_object_get_int64(value);
    if (v < 0) {
        return false;
    }
    *out_value = (uint64_t)v;
    return true;
}

static bool json_get_string_obj(struct json_object *obj,
                                const char *key,
                                char *out_value,
                                size_t out_size) {
    struct json_object *value = NULL;
    const char *raw = NULL;
    int n = 0;
    if (!obj || !key || !out_value || out_size == 0u) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value)) {
        return false;
    }
    if (!json_object_is_type(value, json_type_string)) {
        return false;
    }
    raw = json_object_get_string(value);
    if (!raw || raw[0] == '\0') {
        return false;
    }
    n = snprintf(out_value, out_size, "%s", raw);
    if (n < 0 || (size_t)n >= out_size) {
        out_value[0] = '\0';
        return false;
    }
    return true;
}

static const char *k_region_rollup_band_names[REGION_ROLLUP_BAND_COUNT] = {
    "default",
    "coarse",
    "mid",
    "fine"
};

static const char *k_region_rollup_layer_names[REGION_ROLLUP_LAYER_COUNT] = {
    "artery",
    "local",
    "water",
    "park",
    "landuse",
    "building",
    "contour"
};

static bool region_join_region_path(const RegionInfo *info,
                                    const char *subpath,
                                    char *out_path,
                                    size_t out_size) {
    int n = 0;
    if (!info || !subpath || !out_path || out_size == 0u) {
        return false;
    }
    if (subpath[0] == '/') {
        n = snprintf(out_path, out_size, "%s", subpath);
    } else {
        n = snprintf(out_path, out_size, "%s/%s", info->region_dir, subpath);
    }
    if (n < 0 || (size_t)n >= out_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

bool region_load_meta(const RegionInfo *info, RegionInfo *out_info) {
    if (!info || !out_info) {
        return false;
    }

    *out_info = *info;
    if (!region_resolve_paths(out_info)) {
        return false;
    }

    char path[512];
    if (!region_meta_path(out_info, path, sizeof(path))) {
        return false;
    }

    struct json_object *root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) {
            json_object_put(root);
        }
        return false;
    }

    double min_lat = 0.0;
    double max_lat = 0.0;
    double min_lon = 0.0;
    double max_lon = 0.0;
    bool got_min_lat = false;
    bool got_max_lat = false;
    bool got_min_lon = false;
    bool got_max_lon = false;
    uint32_t tile_min_z = 0;
    uint32_t tile_max_z = 0;
    uint32_t tile_extent = 0;
    bool got_tile_min_z = false;
    bool got_tile_max_z = false;
    bool got_tile_extent = false;
    bool roads_pyramid_enabled = false;
    bool buildings_pyramid_enabled = false;
    TileStorageKind storage_kind = TILE_STORAGE_FILESYSTEM_TREE;
    char tiles_root_path[MAPFORGE_REGION_PATH_CAPACITY];
    char archive_path[MAPFORGE_REGION_PATH_CAPACITY];
    bool has_tiles_root_path = false;
    bool has_archive_path = false;
    tiles_root_path[0] = '\0';
    archive_path[0] = '\0';

    struct json_object *bounds = NULL;
    if (json_object_object_get_ex(root, "bounds", &bounds) &&
        json_object_is_type(bounds, json_type_object)) {
        got_min_lat = json_get_double_obj(bounds, "min_lat", &min_lat);
        got_max_lat = json_get_double_obj(bounds, "max_lat", &max_lat);
        got_min_lon = json_get_double_obj(bounds, "min_lon", &min_lon);
        got_max_lon = json_get_double_obj(bounds, "max_lon", &max_lon);
    }

    struct json_object *tile = NULL;
    if (json_object_object_get_ex(root, "tile", &tile) &&
        json_object_is_type(tile, json_type_object)) {
        got_tile_min_z = json_get_u32_obj(tile, "min_z", &tile_min_z);
        got_tile_max_z = json_get_u32_obj(tile, "max_z", &tile_max_z);
        got_tile_extent = json_get_u32_obj(tile, "extent", &tile_extent);
    }

    struct json_object *tile_pyramid = NULL;
    if (json_object_object_get_ex(root, "tile_pyramid", &tile_pyramid) &&
        json_object_is_type(tile_pyramid, json_type_object)) {
        struct json_object *roads = NULL;
        if (json_object_object_get_ex(tile_pyramid, "roads", &roads) &&
            json_object_is_type(roads, json_type_object)) {
            struct json_object *enabled = NULL;
            if (json_object_object_get_ex(roads, "enabled", &enabled) &&
                json_object_is_type(enabled, json_type_boolean)) {
                roads_pyramid_enabled = json_object_get_boolean(enabled);
            }
        }
        struct json_object *buildings = NULL;
        if (json_object_object_get_ex(tile_pyramid, "buildings", &buildings) &&
            json_object_is_type(buildings, json_type_object)) {
            struct json_object *enabled = NULL;
            if (json_object_object_get_ex(buildings, "enabled", &enabled) &&
                json_object_is_type(enabled, json_type_boolean)) {
                buildings_pyramid_enabled = json_object_get_boolean(enabled);
            }
        }
    }

    struct json_object *tile_store = NULL;
    if (json_object_object_get_ex(root, "tile_store", &tile_store) &&
        json_object_is_type(tile_store, json_type_object)) {
        char kind_buf[64];
        if (json_get_string_obj(tile_store, "kind", kind_buf, sizeof(kind_buf))) {
            storage_kind = tile_storage_kind_from_string(kind_buf);
        }
        char root_buf[MAPFORGE_REGION_PATH_CAPACITY];
        if (json_get_string_obj(tile_store, "root", root_buf, sizeof(root_buf)) &&
            region_join_region_path(out_info, root_buf, tiles_root_path, sizeof(tiles_root_path))) {
            has_tiles_root_path = true;
        }
        char archive_buf[MAPFORGE_REGION_PATH_CAPACITY];
        if (json_get_string_obj(tile_store, "archive_path", archive_buf, sizeof(archive_buf)) &&
            region_join_region_path(out_info, archive_buf, archive_path, sizeof(archive_path))) {
            has_archive_path = true;
        }
    }

    out_info->has_archive_rollups = false;
    out_info->archive_rollup_total_rows = 0u;
    out_info->archive_rollup_total_bytes = 0u;
    memset(out_info->archive_rollup_rows, 0, sizeof(out_info->archive_rollup_rows));
    memset(out_info->archive_rollup_bytes, 0, sizeof(out_info->archive_rollup_bytes));
    struct json_object *output_stats = NULL;
    if (json_object_object_get_ex(root, "output_stats", &output_stats) &&
        json_object_is_type(output_stats, json_type_object)) {
        struct json_object *archive_rollups = NULL;
        if (json_object_object_get_ex(output_stats, "archive_rollups", &archive_rollups) &&
            json_object_is_type(archive_rollups, json_type_object)) {
            uint64_t rows_total = 0u;
            uint64_t bytes_total = 0u;
            bool has_any_band_layer_entry = false;
            (void)json_get_u64_obj(archive_rollups, "rows", &rows_total);
            (void)json_get_u64_obj(archive_rollups, "bytes", &bytes_total);

            struct json_object *bands = NULL;
            if (json_object_object_get_ex(archive_rollups, "bands", &bands) &&
                json_object_is_type(bands, json_type_object)) {
                for (int b = 0; b < REGION_ROLLUP_BAND_COUNT; ++b) {
                    struct json_object *band_obj = NULL;
                    if (!json_object_object_get_ex(bands, k_region_rollup_band_names[b], &band_obj) ||
                        !json_object_is_type(band_obj, json_type_object)) {
                        continue;
                    }
                    for (int l = 0; l < REGION_ROLLUP_LAYER_COUNT; ++l) {
                        struct json_object *layer_obj = NULL;
                        if (!json_object_object_get_ex(band_obj, k_region_rollup_layer_names[l], &layer_obj) ||
                            !json_object_is_type(layer_obj, json_type_object)) {
                            continue;
                        }
                        uint64_t row_count = 0u;
                        uint64_t byte_count = 0u;
                        bool got_row_count = json_get_u64_obj(layer_obj, "rows", &row_count);
                        bool got_byte_count = json_get_u64_obj(layer_obj, "bytes", &byte_count);
                        if (got_row_count || got_byte_count) {
                            has_any_band_layer_entry = true;
                            out_info->archive_rollup_rows[b][l] = row_count;
                            out_info->archive_rollup_bytes[b][l] = byte_count;
                        }
                    }
                }
            }

            if (has_any_band_layer_entry || rows_total > 0u || bytes_total > 0u) {
                out_info->has_archive_rollups = true;
                out_info->archive_rollup_total_rows = rows_total;
                out_info->archive_rollup_total_bytes = bytes_total;
                if (out_info->archive_rollup_total_rows == 0u || out_info->archive_rollup_total_bytes == 0u) {
                    for (int b = 0; b < REGION_ROLLUP_BAND_COUNT; ++b) {
                        for (int l = 0; l < REGION_ROLLUP_LAYER_COUNT; ++l) {
                            out_info->archive_rollup_total_rows += out_info->archive_rollup_rows[b][l];
                            out_info->archive_rollup_total_bytes += out_info->archive_rollup_bytes[b][l];
                        }
                    }
                }
            }
        }
    }

    if (got_min_lat && got_max_lat && got_min_lon && got_max_lon) {
        out_info->center_lat = (min_lat + max_lat) * 0.5;
        out_info->center_lon = (min_lon + max_lon) * 0.5;
        out_info->has_center = true;
        out_info->min_lat = min_lat;
        out_info->max_lat = max_lat;
        out_info->min_lon = min_lon;
        out_info->max_lon = max_lon;
        out_info->has_bounds = true;
    }

    if (got_tile_min_z && got_tile_max_z) {
        if (tile_min_z > tile_max_z) {
            uint32_t swap = tile_min_z;
            tile_min_z = tile_max_z;
            tile_max_z = swap;
        }
        if (tile_min_z > 30u) {
            tile_min_z = 30u;
        }
        if (tile_max_z > 30u) {
            tile_max_z = 30u;
        }
        out_info->tile_min_zoom = (uint16_t)tile_min_z;
        out_info->tile_max_zoom = (uint16_t)tile_max_z;
        out_info->has_tile_range = true;
        if (tile_min_z == tile_max_z) {
            log_info("region '%s' has single tile zoom level z=%u; stepped tile pyramid behavior is disabled",
                     out_info->name,
                     tile_min_z);
        }
    }
    if (got_tile_extent && tile_extent > 0u) {
        out_info->tile_extent = tile_extent;
    }
    out_info->has_tile_pyramid_roads = roads_pyramid_enabled;
    out_info->has_tile_pyramid_buildings = buildings_pyramid_enabled;
    if (!has_tiles_root_path) {
        snprintf(tiles_root_path, sizeof(tiles_root_path), "%s", out_info->tiles_dir);
    }
    if (storage_kind == TILE_STORAGE_ARCHIVE_INDEXED) {
        tile_source_config_set_archive(&out_info->tile_source,
                                       tiles_root_path,
                                       has_archive_path ? archive_path : "");
    } else {
        tile_source_config_set_filesystem(&out_info->tile_source, tiles_root_path);
    }
    snprintf(out_info->tiles_dir, sizeof(out_info->tiles_dir), "%s", tiles_root_path);
    if (has_archive_path) {
        snprintf(out_info->tile_archive_path, sizeof(out_info->tile_archive_path), "%s", archive_path);
    } else {
        out_info->tile_archive_path[0] = '\0';
    }
    out_info->has_tile_archive = has_archive_path;

    json_object_put(root);
    return out_info->has_bounds || out_info->has_tile_range;
}

static bool region_loader_path_is_dir(const char *path) {
    struct stat st;
    if (!path || path[0] == '\0') {
        return false;
    }
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

static bool region_loader_path_is_file(const char *path) {
    struct stat st;
    if (!path || path[0] == '\0') {
        return false;
    }
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}

static void region_loader_summary_set(RegionPackageValidationResult *result, const char *text) {
    if (!result) {
        return;
    }
    if (!text) {
        result->summary[0] = '\0';
        return;
    }
    snprintf(result->summary, sizeof(result->summary), "%s", text);
}

bool region_validate_package(const RegionInfo *info, RegionPackageValidationResult *out_result) {
    RegionInfo loaded = {0};
    char meta_path[MAPFORGE_REGION_PATH_CAPACITY];
    if (!info || !out_result) {
        return false;
    }

    memset(out_result, 0, sizeof(*out_result));
    region_loader_summary_set(out_result, "invalid validator input");

    loaded = *info;
    if (!region_resolve_paths(&loaded)) {
        region_loader_summary_set(out_result, "failed to resolve region paths");
        return false;
    }
    if (!region_meta_path(&loaded, meta_path, sizeof(meta_path))) {
        region_loader_summary_set(out_result, "failed to build meta.json path");
        return false;
    }
    out_result->has_meta = region_loader_path_is_file(meta_path);
    if (!out_result->has_meta) {
        region_loader_summary_set(out_result, "missing meta.json");
        return false;
    }
    if (!region_load_meta(&loaded, &loaded)) {
        region_loader_summary_set(out_result, "meta.json parse failed or missing bounds/tile range");
        return false;
    }

    out_result->archive_storage = loaded.tile_source.storage_kind == TILE_STORAGE_ARCHIVE_INDEXED;
    out_result->archive_reader_supported = tile_source_archive_reader_supported();
    out_result->has_tiles_root = region_loader_path_is_dir(loaded.tiles_dir);
    out_result->has_archive_path = loaded.tile_archive_path[0] != '\0';
    out_result->has_archive_file = out_result->has_archive_path && region_loader_path_is_file(loaded.tile_archive_path);
    out_result->has_graph = region_has_graph(&loaded);

    if (!out_result->archive_storage) {
        if (!out_result->has_tiles_root) {
            region_loader_summary_set(out_result, "tile_store.filesystem_tree requires a readable tile_store.root directory");
            return false;
        }
        out_result->ok = true;
        region_loader_summary_set(out_result, out_result->has_graph
            ? "filesystem package valid"
            : "filesystem package valid (graph missing; routing disabled)");
        return true;
    }

    if (!out_result->has_archive_path) {
        region_loader_summary_set(out_result, "tile_store.archive_indexed requires tile_store.archive_path");
        return false;
    }
    if (!out_result->has_archive_file) {
        region_loader_summary_set(out_result, "tile_store.archive_path does not exist");
        return false;
    }
    if (!out_result->archive_reader_supported) {
        if (!out_result->has_tiles_root) {
            region_loader_summary_set(out_result, "archive reader unavailable; provide extracted tile_store.root fallback");
            return false;
        }
        out_result->archive_fallback_tree = true;
        out_result->ok = true;
        region_loader_summary_set(out_result, "archive metadata valid; runtime will use extracted tree fallback");
        return true;
    }

    out_result->archive_fallback_tree = out_result->has_tiles_root;
    out_result->ok = true;
    region_loader_summary_set(out_result, out_result->has_graph
        ? "archive package valid"
        : "archive package valid (graph missing; routing disabled)");
    return true;
}

void region_log_archive_rollup_summary(const RegionInfo *info, const char *context) {
    if (!info || !info->has_archive_rollups) {
        return;
    }
    uint64_t fine_roads = info->archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_ARTERY] +
        info->archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_LOCAL];
    uint64_t fine_polygons = info->archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_WATER] +
        info->archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_PARK] +
        info->archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_LANDUSE] +
        info->archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_BUILDING];
    uint64_t coarse_rows = 0u;
    for (int layer = 0; layer < REGION_ROLLUP_LAYER_COUNT; ++layer) {
        coarse_rows += info->archive_rollup_rows[REGION_ROLLUP_BAND_COARSE][layer];
    }

    log_info("region_archive_rollup context=%s region=%s rows=%llu bytes=%llu fine(roads=%llu polys=%llu) coarse_rows=%llu",
             context ? context : "runtime",
             info->name ? info->name : "unknown",
             (unsigned long long)info->archive_rollup_total_rows,
             (unsigned long long)info->archive_rollup_total_bytes,
             (unsigned long long)fine_roads,
             (unsigned long long)fine_polygons,
             (unsigned long long)coarse_rows);
}
