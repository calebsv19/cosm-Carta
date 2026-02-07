#include "map/map_space.h"

static const float kTileExtent = 4096.0f;
static const int kTileGridSize = 4097;

void map_tile_transform_init(TileCoord coord, MapTileTransform *out_transform) {
    CoreSpaceDesc desc;
    CoreResult r;
    float cell_size = 1.0f;

    if (!out_transform) {
        return;
    }
    out_transform->coord = coord;
    out_transform->tile_size_m = tile_size_meters(coord.z);
    out_transform->origin = tile_origin_meters(coord);

    cell_size = (float)(out_transform->tile_size_m / (double)kTileExtent);
    r = core_space_desc_default_from_grid(kTileGridSize,
                                          kTileGridSize,
                                          (float)out_transform->origin.x,
                                          (float)(out_transform->origin.y - out_transform->tile_size_m),
                                          cell_size,
                                          &desc);
    if (r.code != CORE_OK) {
        /* Defensive fallback to avoid exposing uninitialized state. */
        core_space_desc_default_from_grid(kTileGridSize, kTileGridSize, 0.0f, 0.0f, 1.0f, &desc);
    }
    out_transform->space_desc = desc;
}

void map_tile_local_to_world(const MapTileTransform *transform,
                             float tile_local_x,
                             float tile_local_y,
                             float *out_world_x,
                             float *out_world_y) {
    if (!transform) {
        return;
    }

    float ux = tile_local_x / kTileExtent;
    float uy = tile_local_y / kTileExtent;
    float inv_uy = 1.0f - uy;

    if (out_world_x) {
        *out_world_x = core_space_unit_to_world_x(&transform->space_desc, ux);
    }
    if (out_world_y) {
        *out_world_y = core_space_unit_to_world_y(&transform->space_desc, inv_uy);
    }
}

void map_world_to_screen(const Camera *camera,
                         int screen_w,
                         int screen_h,
                         float world_x,
                         float world_y,
                         float *out_screen_x,
                         float *out_screen_y) {
    camera_world_to_screen(camera, world_x, world_y, screen_w, screen_h, out_screen_x, out_screen_y);
}

void map_tile_local_to_screen(const MapTileTransform *transform,
                              const Camera *camera,
                              int screen_w,
                              int screen_h,
                              float tile_local_x,
                              float tile_local_y,
                              float *out_screen_x,
                              float *out_screen_y) {
    float world_x = 0.0f;
    float world_y = 0.0f;
    map_tile_local_to_world(transform, tile_local_x, tile_local_y, &world_x, &world_y);
    map_world_to_screen(camera, screen_w, screen_h, world_x, world_y, out_screen_x, out_screen_y);
}

bool map_tile_affine_from_camera(const Camera *camera,
                                 int screen_w,
                                 int screen_h,
                                 TileCoord coord,
                                 MapTileAffine *out_affine) {
    if (!camera || !out_affine || screen_w <= 0 || screen_h <= 0) {
        return false;
    }

    MapTileTransform transform;
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;

    map_tile_transform_init(coord, &transform);
    map_tile_local_to_screen(&transform, camera, screen_w, screen_h, 0.0f, 0.0f, &x0, &y0);
    map_tile_local_to_screen(&transform, camera, screen_w, screen_h, kTileExtent, 0.0f, &x1, &y1);
    map_tile_local_to_screen(&transform, camera, screen_w, screen_h, 0.0f, kTileExtent, &x2, &y2);

    out_affine->m00 = (x1 - x0) / kTileExtent;
    out_affine->m01 = (x2 - x0) / kTileExtent;
    out_affine->m02 = x0;
    out_affine->m10 = (y1 - y0) / kTileExtent;
    out_affine->m11 = (y2 - y0) / kTileExtent;
    out_affine->m12 = y0;
    return true;
}
