#ifndef MAPFORGE_MAP_SPACE_H
#define MAPFORGE_MAP_SPACE_H

#include "camera/camera.h"
#include "core_space.h"
#include "map/mercator.h"
#include "map/tile_math.h"

#include <stdbool.h>

typedef struct MapTileTransform {
    TileCoord coord;
    MercatorMeters origin;
    double tile_size_m;
    CoreSpaceDesc space_desc;
} MapTileTransform;

typedef struct MapTileAffine {
    float m00;
    float m01;
    float m02;
    float m10;
    float m11;
    float m12;
} MapTileAffine;

void map_tile_transform_init(TileCoord coord, MapTileTransform *out_transform);

void map_tile_local_to_world(const MapTileTransform *transform,
                             float tile_local_x,
                             float tile_local_y,
                             float *out_world_x,
                             float *out_world_y);

void map_world_to_screen(const Camera *camera,
                         int screen_w,
                         int screen_h,
                         float world_x,
                         float world_y,
                         float *out_screen_x,
                         float *out_screen_y);

void map_tile_local_to_screen(const MapTileTransform *transform,
                              const Camera *camera,
                              int screen_w,
                              int screen_h,
                              float tile_local_x,
                              float tile_local_y,
                              float *out_screen_x,
                              float *out_screen_y);

bool map_tile_affine_from_camera(const Camera *camera,
                                 int screen_w,
                                 int screen_h,
                                 TileCoord coord,
                                 MapTileAffine *out_affine);

#endif
