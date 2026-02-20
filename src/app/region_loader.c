#include "app/region_loader.h"

#include "core/log.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdint.h>

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

bool region_load_meta(const RegionInfo *info, RegionInfo *out_info) {
    if (!info || !out_info) {
        return false;
    }

    *out_info = *info;

    char path[512];
    snprintf(path, sizeof(path), "data/regions/%s/meta.json", info->name);

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

    json_object_put(root);
    return out_info->has_bounds || out_info->has_tile_range;
}
