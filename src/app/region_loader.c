#include "app/region_loader.h"

#include "core/log.h"

#include <stdio.h>
#include <string.h>

static bool extract_double(const char *line, const char *key, double *out_value) {
    if (!line || !key || !out_value) {
        return false;
    }

    const char *pos = strstr(line, key);
    if (!pos) {
        return false;
    }

    const char *colon = strchr(pos, ':');
    if (!colon) {
        return false;
    }

    double value = 0.0;
    if (sscanf(colon + 1, " %lf", &value) != 1) {
        return false;
    }

    *out_value = value;
    return true;
}

bool region_load_meta(const RegionInfo *info, RegionInfo *out_info) {
    if (!info || !out_info) {
        return false;
    }

    *out_info = *info;

    char path[512];
    snprintf(path, sizeof(path), "data/regions/%s/meta.json", info->name);

    FILE *file = fopen(path, "r");
    if (!file) {
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

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (!got_min_lat && extract_double(line, "\"min_lat\"", &min_lat)) {
            got_min_lat = true;
        }
        if (!got_max_lat && extract_double(line, "\"max_lat\"", &max_lat)) {
            got_max_lat = true;
        }
        if (!got_min_lon && extract_double(line, "\"min_lon\"", &min_lon)) {
            got_min_lon = true;
        }
        if (!got_max_lon && extract_double(line, "\"max_lon\"", &max_lon)) {
            got_max_lon = true;
        }
    }

    fclose(file);

    if (got_min_lat && got_max_lat && got_min_lon && got_max_lon) {
        out_info->center_lat = (min_lat + max_lat) * 0.5;
        out_info->center_lon = (min_lon + max_lon) * 0.5;
        out_info->has_center = true;
        out_info->min_lat = min_lat;
        out_info->max_lat = max_lat;
        out_info->min_lon = min_lon;
        out_info->max_lon = max_lon;
        out_info->has_bounds = true;
        return true;
    }

    return false;
}
