#include "camera/camera.h"
#include "map/map_space.h"
#include "map/mercator.h"
#include "map/tile_math.h"

#include <math.h>
#include <stdio.h>

static int nearly_equal(float a, float b, float eps) {
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= eps;
}

static int test_tile_world_roundtrip(void) {
    TileCoord coord = {12, 655, 1582};
    MapTileTransform transform;
    map_tile_transform_init(coord, &transform);

    float local_x = 1337.0f;
    float local_y = 2048.0f;
    float world_x = 0.0f;
    float world_y = 0.0f;
    map_tile_local_to_world(&transform, local_x, local_y, &world_x, &world_y);

    TileCoord resolved = tile_from_meters(coord.z, (MercatorMeters){world_x, world_y});
    if (resolved.x != coord.x || resolved.y != coord.y || resolved.z != coord.z) {
        printf("FAIL tile_world_roundtrip expected z/x/y=%u/%u/%u got %u/%u/%u\n",
               coord.z, coord.x, coord.y, resolved.z, resolved.x, resolved.y);
        return 1;
    }
    return 0;
}

static int test_tile_affine_matches_screen_projection(void) {
    Camera camera;
    camera_init(&camera);
    camera.x = -13618288.0f;
    camera.y = 6046761.0f;
    camera.zoom = 12.5f;

    TileCoord coord = {12, 655, 1582};
    MapTileTransform transform;
    map_tile_transform_init(coord, &transform);

    const int screen_w = 1280;
    const int screen_h = 720;
    const float local_x = 900.0f;
    const float local_y = 1200.0f;

    float sx_ref = 0.0f;
    float sy_ref = 0.0f;
    map_tile_local_to_screen(&transform,
                             &camera,
                             screen_w,
                             screen_h,
                             local_x,
                             local_y,
                             &sx_ref,
                             &sy_ref);

    MapTileAffine affine;
    if (!map_tile_affine_from_camera(&camera, screen_w, screen_h, coord, &affine)) {
        printf("FAIL tile_affine_from_camera returned false\n");
        return 1;
    }
    float sx_affine = affine.m00 * local_x + affine.m01 * local_y + affine.m02;
    float sy_affine = affine.m10 * local_x + affine.m11 * local_y + affine.m12;

    if (!nearly_equal(sx_ref, sx_affine, 0.01f) || !nearly_equal(sy_ref, sy_affine, 0.01f)) {
        printf("FAIL affine parity ref=(%.4f,%.4f) affine=(%.4f,%.4f)\n",
               sx_ref, sy_ref, sx_affine, sy_affine);
        return 1;
    }
    return 0;
}

static int test_screen_world_roundtrip(void) {
    Camera camera;
    camera_init(&camera);
    camera.x = -13618288.0f;
    camera.y = 6046761.0f;
    camera.zoom = 13.0f;

    const int screen_w = 1920;
    const int screen_h = 1080;
    const float world_x = -13618000.5f;
    const float world_y = 6046500.25f;

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&camera, world_x, world_y, screen_w, screen_h, &sx, &sy);

    float roundtrip_x = 0.0f;
    float roundtrip_y = 0.0f;
    camera_screen_to_world(&camera, sx, sy, screen_w, screen_h, &roundtrip_x, &roundtrip_y);

    if (!nearly_equal(world_x, roundtrip_x, 0.01f) || !nearly_equal(world_y, roundtrip_y, 0.01f)) {
        printf("FAIL screen_world_roundtrip in=(%.4f,%.4f) out=(%.4f,%.4f)\n",
               world_x, world_y, roundtrip_x, roundtrip_y);
        return 1;
    }
    return 0;
}

static int test_tile_wrap_and_clamp(void) {
    double world_size = mercator_world_size_meters();
    double half_world = world_size * 0.5;
    double base_x = -half_world + 12345.0;
    TileCoord a = tile_from_meters(12, (MercatorMeters){base_x, 0.0});
    TileCoord b = tile_from_meters(12, (MercatorMeters){base_x + world_size, 0.0});
    if (a.x != b.x) {
        printf("FAIL wrap periodicity mismatch a_x=%u b_x=%u\n", a.x, b.x);
        return 1;
    }

    TileCoord top = tile_from_meters(12, (MercatorMeters){0.0, half_world + 1000.0});
    TileCoord bottom = tile_from_meters(12, (MercatorMeters){0.0, -half_world - 1000.0});
    uint32_t max_index = tile_count(12) - 1u;
    if (top.y != 0u || bottom.y != max_index) {
        printf("FAIL y clamp mismatch top=%u bottom=%u expected 0/%u\n", top.y, bottom.y, max_index);
        return 1;
    }

    return 0;
}

static int test_affine_constraint_solver(void) {
    float weights[3][3] = {
        {1.0f, 0.25f, 0.125f},
        {0.25f, 1.0f, 0.5f},
        {0.125f, 0.5f, 1.0f}
    };
    float residuals[3] = {0.33f, -0.12f, 0.48f};
    float score = 0.0f;

    for (int row = 0; row < 3; ++row) {
        float row_accum = 0.0f;
        for (int col = 0; col < 3; ++col) {
            row_accum += weights[row][col] * residuals[col];
        }
        score += fabsf(row_accum);
    }

    if (score < 0.01f) {
        printf("FAIL affine constraint score collapsed unexpectedly: %.6f\n", score);
        return 1;
    }
    return 0;
}

int main(void) {
    int failures = 0;
    failures += test_tile_world_roundtrip();
    failures += test_tile_affine_matches_screen_projection();
    failures += test_screen_world_roundtrip();
    failures += test_tile_wrap_and_clamp();
    failures += test_affine_constraint_solver();

    if (failures != 0) {
        printf("map_space_test: %d failure(s)\n", failures);
        return 1;
    }

    printf("map_space_test passed\n");
    return 0;
}
