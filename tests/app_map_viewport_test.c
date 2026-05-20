#include "app/app_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool renderer_set_viewport(Renderer *renderer, int x, int y, int width, int height) {
    if (!renderer) {
        return false;
    }
    renderer->viewport_enabled = true;
    renderer->viewport_x = x;
    renderer->viewport_y = y;
    renderer->width = width;
    renderer->height = height;
    return true;
}

void renderer_reset_viewport(Renderer *renderer) {
    if (!renderer) {
        return;
    }
    renderer->viewport_enabled = false;
    renderer->viewport_x = 0;
    renderer->viewport_y = 0;
}

static int nearly_equal(float a, float b, float epsilon) {
    return fabsf(a - b) <= epsilon;
}

static int test_viewport_screen_and_local_spaces_stay_consistent(void) {
    AppState app;
    float world_x = -13618288.0f + 150.0f;
    float world_y = 6046761.0f - 90.0f;
    float screen_x = 0.0f;
    float screen_y = 0.0f;
    float local_x = 0.0f;
    float local_y = 0.0f;
    float roundtrip_x = 0.0f;
    float roundtrip_y = 0.0f;

    memset(&app, 0, sizeof(app));
    app.width = 1280;
    app.height = 720;
    app.ui_state_bridge.map_viewport_rect = (SDL_FRect){
        0.0f,
        APP_HEADER_HEIGHT,
        1280.0f,
        720.0f - APP_HEADER_HEIGHT
    };

    camera_init(&app.view_state_bridge.camera);
    app.view_state_bridge.camera.x = -13618288.0f;
    app.view_state_bridge.camera.y = 6046761.0f;
    app.view_state_bridge.camera.x_target = app.view_state_bridge.camera.x;
    app.view_state_bridge.camera.y_target = app.view_state_bridge.camera.y;
    app.view_state_bridge.camera.zoom = 13.0f;
    app.view_state_bridge.camera.zoom_target = app.view_state_bridge.camera.zoom;
    app.view_state_bridge.camera.heading_rad = 0.0f;
    app.view_state_bridge.camera.heading_target_rad = 0.0f;

    if (!app_map_world_to_screen(&app, world_x, world_y, &screen_x, &screen_y)) {
        printf("FAIL app_map_world_to_screen returned false\n");
        return 1;
    }
    if (!app_map_world_to_viewport_local(&app, world_x, world_y, &local_x, &local_y)) {
        printf("FAIL app_map_world_to_viewport_local returned false\n");
        return 1;
    }
    if (!nearly_equal(screen_x, local_x, 0.01f) ||
        !nearly_equal(screen_y, local_y + APP_HEADER_HEIGHT, 0.01f)) {
        printf("FAIL viewport local mismatch screen=(%.3f,%.3f) local=(%.3f,%.3f) header=%.3f\n",
               screen_x,
               screen_y,
               local_x,
               local_y,
               (float)APP_HEADER_HEIGHT);
        return 1;
    }
    if (!app_map_screen_to_world(&app, screen_x, screen_y, &roundtrip_x, &roundtrip_y)) {
        printf("FAIL app_map_screen_to_world returned false\n");
        return 1;
    }
    if (!nearly_equal(roundtrip_x, world_x, 0.01f) ||
        !nearly_equal(roundtrip_y, world_y, 0.01f)) {
        printf("FAIL viewport roundtrip mismatch world=(%.3f,%.3f) roundtrip=(%.3f,%.3f)\n",
               world_x,
               world_y,
               roundtrip_x,
               roundtrip_y);
        return 1;
    }
    return 0;
}

int main(void) {
    int failures = 0;
    failures += test_viewport_screen_and_local_spaces_stay_consistent();
    if (failures != 0) {
        return 1;
    }
    printf("app_map_viewport_test: success\n");
    return 0;
}
