#include "app/app_internal.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static float g_last_heading_target = 0.0f;

void app_route_schedule_recompute(AppState *app, double debounce_sec) {
    (void)app;
    (void)debounce_sec;
}

void camera_set_heading_target(Camera *camera, float heading_rad) {
    g_last_heading_target = heading_rad;
    if (camera) {
        camera->heading_target_rad = heading_rad;
    }
}

static void seed_path(AppState *app) {
    assert(app);
    app->route_state_bridge.route.graph.node_count = 3u;
    static double node_x[] = {0.0, 0.0, 10.0};
    static double node_y[] = {0.0, 10.0, 10.0};
    static uint32_t nodes[] = {0u, 1u, 2u};
    static float cumulative_time_s[] = {0.0f, 10.0f, 20.0f};

    app->route_state_bridge.route.graph.node_x = node_x;
    app->route_state_bridge.route.graph.node_y = node_y;
    app->route_state_bridge.route.path.nodes = nodes;
    app->route_state_bridge.route.path.count = 3u;
    app->route_state_bridge.route.path.cumulative_time_s = cumulative_time_s;
    app->route_state_bridge.route.path.total_time_s = 20.0f;
}

static void seed_sway_path(AppState *app) {
    assert(app);
    app->route_state_bridge.route.graph.node_count = 4u;
    static double node_x[] = {0.0, 0.0, 1.0, 0.0};
    static double node_y[] = {0.0, 10.0, 20.0, 30.0};
    static uint32_t nodes[] = {0u, 1u, 2u, 3u};
    static float cumulative_time_s[] = {0.0f, 10.0f, 20.0f, 30.0f};

    app->route_state_bridge.route.graph.node_x = node_x;
    app->route_state_bridge.route.graph.node_y = node_y;
    app->route_state_bridge.route.path.nodes = nodes;
    app->route_state_bridge.route.path.count = 4u;
    app->route_state_bridge.route.path.cumulative_time_s = cumulative_time_s;
    app->route_state_bridge.route.path.total_time_s = 30.0f;
}

static void test_preview_samples_paused_position_without_follow(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    seed_path(&app);

    app.view_state_bridge.camera.x_target = 111.0f;
    app.view_state_bridge.camera.y_target = 222.0f;
    app.view_state_bridge.camera.heading_target_rad = -1.0f;
    app.route_state_bridge.playback_time_s = 5.0f;
    app.route_state_bridge.playback_playing = false;
    g_last_heading_target = -10.0f;

    app_route_preview_update(&app);

    assert(app.route_state_bridge.preview.valid);
    assert(!app.route_state_bridge.preview.follow_active);
    assert(app.route_state_bridge.preview.segment_index == 0u);
    assert(fabsf(app.route_state_bridge.preview.world_x - 0.0f) < 0.001f);
    assert(fabsf(app.route_state_bridge.preview.world_y - 5.0f) < 0.001f);
    assert(fabsf(app.route_state_bridge.preview.heading_rad - 0.0f) < 0.001f);
    assert(app.view_state_bridge.camera.x_target == 111.0f);
    assert(app.view_state_bridge.camera.y_target == 222.0f);
    assert(g_last_heading_target == -10.0f);
}

static void test_preview_drives_follow_camera_while_playing(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    seed_path(&app);

    app.route_state_bridge.playback_time_s = 15.0f;
    app.route_state_bridge.playback_playing = true;
    app.route_state_bridge.preview_follow_enabled = true;
    app.route_state_bridge.preview_heading_up = true;
    g_last_heading_target = 0.0f;

    app_route_preview_update(&app);

    assert(app.route_state_bridge.preview.valid);
    assert(app.route_state_bridge.preview.follow_active);
    assert(app.route_state_bridge.preview.segment_index == 1u);
    assert(fabsf(app.route_state_bridge.preview.world_x - 5.0f) < 0.001f);
    assert(fabsf(app.route_state_bridge.preview.world_y - 10.0f) < 0.001f);
    assert(fabsf(app.route_state_bridge.preview.heading_rad - 1.57079632679f) < 0.001f);
    assert(fabsf(app.view_state_bridge.camera.x_target - 5.0f) < 0.001f);
    assert(fabsf(app.view_state_bridge.camera.y_target - 10.0f) < 0.001f);
    assert(fabsf(app.view_state_bridge.camera.heading_target_rad - 1.57079632679f) < 0.001f);
    assert(fabsf(g_last_heading_target - 1.57079632679f) < 0.001f);
}

static void test_preview_follows_while_paused_when_follow_mode_enabled(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    seed_path(&app);

    app.route_state_bridge.playback_time_s = 5.0f;
    app.route_state_bridge.playback_playing = false;
    app.route_state_bridge.preview_follow_enabled = true;
    app.route_state_bridge.preview_heading_up = true;
    g_last_heading_target = 0.0f;

    app_route_preview_update(&app);

    assert(app.route_state_bridge.preview.valid);
    assert(app.route_state_bridge.preview.follow_active);
    assert(fabsf(app.view_state_bridge.camera.x_target - 0.0f) < 0.001f);
    assert(fabsf(app.view_state_bridge.camera.y_target - 5.0f) < 0.001f);
    assert(fabsf(app.view_state_bridge.camera.heading_target_rad - 0.0f) < 0.001f);
}

static void test_preview_north_up_mode_forces_zero_heading(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    seed_path(&app);

    app.route_state_bridge.playback_time_s = 15.0f;
    app.route_state_bridge.preview_follow_enabled = true;
    app.route_state_bridge.preview_heading_up = false;
    g_last_heading_target = 99.0f;

    app_route_preview_update(&app);

    assert(app.route_state_bridge.preview.valid);
    assert(app.route_state_bridge.preview.follow_active);
    assert(fabsf(app.route_state_bridge.preview.heading_rad - 1.57079632679f) < 0.001f);
    assert(fabsf(app.view_state_bridge.camera.heading_target_rad - 0.0f) < 0.001f);
    assert(fabsf(g_last_heading_target - 0.0f) < 0.001f);
}

static void test_preview_spatial_heading_window_softens_subtle_bends(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    seed_sway_path(&app);

    app.route_state_bridge.playback_time_s = 10.5f;

    app_route_preview_update(&app);

    const float raw_segment_heading = atan2f(1.0f, 10.0f);
    assert(app.route_state_bridge.preview.valid);
    assert(app.route_state_bridge.preview.heading_rad > 0.0f);
    assert(app.route_state_bridge.preview.heading_rad < raw_segment_heading);
}

static void test_preview_heading_memory_smooths_abrupt_turn_transitions(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    seed_path(&app);

    app.route_state_bridge.preview_follow_enabled = true;
    app.route_state_bridge.preview_heading_up = true;

    app.route_state_bridge.playback_time_s = 5.0f;
    app_route_preview_update(&app);
    assert(fabsf(app.route_state_bridge.preview.heading_rad - 0.0f) < 0.001f);

    app.route_state_bridge.playback_time_s = 10.1f;
    app_route_preview_update(&app);

    assert(app.route_state_bridge.preview.valid);
    assert(app.route_state_bridge.preview.heading_rad > 0.2f);
    assert(app.route_state_bridge.preview.heading_rad < 1.3f);
    assert(app.view_state_bridge.camera.heading_target_rad > 0.2f);
    assert(app.view_state_bridge.camera.heading_target_rad < 1.3f);
}

static void test_preview_reset_clears_state(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    app.route_state_bridge.preview.valid = true;
    app.route_state_bridge.preview.follow_active = true;
    app.route_state_bridge.preview.world_x = 7.0f;
    app.route_state_bridge.preview_heading_memory_valid = true;
    app.route_state_bridge.preview_heading_memory_rad = 1.0f;

    app_route_preview_reset(&app);

    assert(!app.route_state_bridge.preview.valid);
    assert(!app.route_state_bridge.preview.follow_active);
    assert(app.route_state_bridge.preview.world_x == 0.0f);
    assert(!app.route_state_bridge.preview_heading_memory_valid);
    assert(app.route_state_bridge.preview_heading_memory_rad == 0.0f);
}

int main(void) {
    test_preview_samples_paused_position_without_follow();
    test_preview_drives_follow_camera_while_playing();
    test_preview_follows_while_paused_when_follow_mode_enabled();
    test_preview_north_up_mode_forces_zero_heading();
    test_preview_spatial_heading_window_softens_subtle_bends();
    test_preview_heading_memory_smooths_abrupt_turn_transitions();
    test_preview_reset_clears_state();
    printf("app_route_preview_test: success\n");
    return 0;
}
