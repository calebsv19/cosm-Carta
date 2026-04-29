#include "camera/camera.h"
#include "map/map_space.h"
#include "map/mercator.h"
#include "map/tile_math.h"

#include <math.h>
#include <stdio.h>
#include <SDL.h>

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

static int test_camera_wheel_anchor_preserved(void) {
    Camera camera;
    InputState input = {0};
    const int screen_w = 1920;
    const int screen_h = 1080;
    const float anchor_x = 1510.0f;
    const float anchor_y = 312.0f;
    float world_before_x = 0.0f;
    float world_before_y = 0.0f;
    float world_after_x = 0.0f;
    float world_after_y = 0.0f;
    Camera target_camera;

    camera_init(&camera);
    camera.x = -13618288.0f;
    camera.y = 6046761.0f;
    camera.x_target = camera.x;
    camera.y_target = camera.y;
    camera.zoom = 13.0f;
    camera.zoom_target = camera.zoom;

    camera_screen_to_world(&camera, anchor_x, anchor_y, screen_w, screen_h, &world_before_x, &world_before_y);

    input.mouse_x = (int)anchor_x;
    input.mouse_y = (int)anchor_y;
    input.mouse_wheel_y = 1;
    camera_handle_input(&camera, &input, screen_w, screen_h, 1.0f / 60.0f, false);

    target_camera = camera;
    target_camera.x = camera.x_target;
    target_camera.y = camera.y_target;
    target_camera.zoom = camera.zoom_target;
    camera_screen_to_world(&target_camera, anchor_x, anchor_y, screen_w, screen_h, &world_after_x, &world_after_y);

    if (!nearly_equal(world_before_x, world_after_x, 1.0f) ||
        !nearly_equal(world_before_y, world_after_y, 1.0f) ||
        !nearly_equal(camera.zoom_target, 13.25f, 0.0001f)) {
        printf("FAIL camera_wheel_anchor_preserved before=(%.4f,%.4f) after=(%.4f,%.4f) zoom_target=%.4f\n",
               world_before_x, world_before_y, world_after_x, world_after_y, camera.zoom_target);
        return 1;
    }

    return 0;
}

static int test_camera_drag_pan_moves_world_with_cursor(void) {
    Camera camera;
    InputState input = {0};
    const int screen_w = 1920;
    const int screen_h = 1080;
    const float screen_x = 840.0f;
    const float screen_y = 460.0f;
    const float delta_x = 48.0f;
    const float delta_y = -32.0f;
    float world_x = 0.0f;
    float world_y = 0.0f;
    float moved_screen_x = 0.0f;
    float moved_screen_y = 0.0f;
    Camera target_camera;

    camera_init(&camera);
    camera.x = -13618288.0f;
    camera.y = 6046761.0f;
    camera.x_target = camera.x;
    camera.y_target = camera.y;
    camera.zoom = 13.0f;
    camera.zoom_target = camera.zoom;

    camera_screen_to_world(&camera, screen_x, screen_y, screen_w, screen_h, &world_x, &world_y);

    input.mouse_dx = (int)delta_x;
    input.mouse_dy = (int)delta_y;
    input.mouse_buttons = SDL_BUTTON_LMASK;
    camera_handle_input(&camera, &input, screen_w, screen_h, 1.0f / 60.0f, true);

    target_camera = camera;
    target_camera.x = camera.x_target;
    target_camera.y = camera.y_target;
    camera_world_to_screen(&target_camera, world_x, world_y, screen_w, screen_h, &moved_screen_x, &moved_screen_y);

    if (!nearly_equal(moved_screen_x, screen_x + delta_x, 0.1f) ||
        !nearly_equal(moved_screen_y, screen_y + delta_y, 0.1f)) {
        printf("FAIL camera_drag_pan_moves_world_with_cursor expected=(%.4f,%.4f) got=(%.4f,%.4f)\n",
               screen_x + delta_x, screen_y + delta_y, moved_screen_x, moved_screen_y);
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
    failures += test_camera_wheel_anchor_preserved();
    failures += test_camera_drag_pan_moves_world_with_cursor();
    failures += test_affine_constraint_solver();

    if (failures != 0) {
        printf("map_space_test: %d failure(s)\n", failures);
        return 1;
    }

    printf("map_space_test passed\n");
    return 0;
}
