#include "app/app_internal.h"

#include "map/mercator.h"

#include <math.h>

static const uint16_t kDefaultMinZoom = 10;
static const uint16_t kDefaultMaxZoom = 18;

float app_clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

uint16_t app_zoom_to_tile_level(float zoom, const RegionInfo *region) {
    int level = (int)floorf(zoom + 0.5f);
    uint16_t min_zoom = kDefaultMinZoom;
    uint16_t max_zoom = kDefaultMaxZoom;
    if (region && region->has_tile_range) {
        min_zoom = region->tile_min_zoom;
        max_zoom = region->tile_max_zoom;
        if (min_zoom > max_zoom) {
            uint16_t swap = min_zoom;
            min_zoom = max_zoom;
            max_zoom = swap;
        }
    }
    if (level < (int)min_zoom) {
        level = (int)min_zoom;
    }
    if (level > (int)max_zoom) {
        level = (int)max_zoom;
    }
    return (uint16_t)level;
}

static float app_zoom_for_bounds(const Camera *camera, const RegionInfo *region, int screen_w, int screen_h, float padding) {
    if (!camera || !region || !region->has_bounds || screen_w <= 0 || screen_h <= 0) {
        return camera ? camera->zoom : 14.0f;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){region->min_lat, region->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){region->max_lat, region->max_lon});

    double width = max_m.x - min_m.x;
    double height = max_m.y - min_m.y;
    if (width <= 0.0 || height <= 0.0) {
        return camera->zoom;
    }

    double target_w = (double)screen_w / (width * (double)padding);
    double target_h = (double)screen_h / (height * (double)padding);
    double ppm = target_w < target_h ? target_w : target_h;
    if (ppm <= 0.0) {
        return camera->zoom;
    }

    double world_size = mercator_world_size_meters();
    float min_zoom = (region && region->has_tile_range) ? (float)region->tile_min_zoom : 10.0f;
    float max_zoom = (region && region->has_tile_range) ? (float)region->tile_max_zoom : 18.0f;
    if (min_zoom > max_zoom) {
        float swap = min_zoom;
        min_zoom = max_zoom;
        max_zoom = swap;
    }

    double zoom = log2(ppm * world_size / 256.0);
    return app_clampf((float)zoom, min_zoom, max_zoom);
}

void app_center_camera_on_region(Camera *camera, const RegionInfo *region, int screen_w, int screen_h) {
    if (!camera || !region || !region->has_bounds) {
        return;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){region->min_lat, region->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){region->max_lat, region->max_lon});

    camera->x = (float)((min_m.x + max_m.x) * 0.5);
    camera->y = (float)((min_m.y + max_m.y) * 0.5);
    camera->zoom = app_zoom_for_bounds(camera, region, screen_w, screen_h, 1.15f);
    camera->x_target = camera->x;
    camera->y_target = camera->y;
    camera->zoom_target = camera->zoom;
}

float app_building_zoom_bias_for_region(const RegionInfo *region) {
    if (!region || !region->has_bounds) {
        return 0.0f;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){region->min_lat, region->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){region->max_lat, region->max_lon});

    double width = max_m.x - min_m.x;
    double height = max_m.y - min_m.y;
    if (width <= 0.0 || height <= 0.0) {
        return 0.0f;
    }

    double span = width > height ? width : height;
    double base = 30000.0;
    double ratio = span / base;
    if (ratio < 1.0) {
        ratio = 1.0;
    }

    float bias = (float)log2(ratio) * 0.6f;
    if (bias < 0.0f) {
        bias = 0.0f;
    }
    if (bias > 1.5f) {
        bias = 1.5f;
    }
    return bias;
}

float app_road_zoom_bias_for_region(const RegionInfo *region) {
    if (!region || !region->has_bounds) {
        return 0.0f;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){region->min_lat, region->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){region->max_lat, region->max_lon});

    double width = max_m.x - min_m.x;
    double height = max_m.y - min_m.y;
    if (width <= 0.0 || height <= 0.0) {
        return 0.0f;
    }

    double span = width > height ? width : height;
    double base = 30000.0;
    double ratio = span / base;
    if (ratio < 1.0) {
        ratio = 1.0;
    }

    float bias = (float)log2(ratio) * 0.5f;
    if (bias < 0.0f) {
        bias = 0.0f;
    }
    if (bias > 2.0f) {
        bias = 2.0f;
    }
    return bias;
}
