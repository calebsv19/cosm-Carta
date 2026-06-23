#include "app/app_internal.h"
#include "app/app_headless.h"

#include "route_preview_test_fixture.h"

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
    mapforge_test_route_fixture_seed_corner(&app->route_state_bridge.route.graph,
                                            &app->route_state_bridge.route.path);
}

static void seed_sway_path(AppState *app) {
    assert(app);
    mapforge_test_route_fixture_seed_sway(&app->route_state_bridge.route.graph,
                                          &app->route_state_bridge.route.path);
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

static void test_preview_follow_entrypoints_own_settings(void) {
    AppState app;
    memset(&app, 0, sizeof(app));

    assert(!app_route_preview_toggle_follow(&app));
    assert(!app.route_state_bridge.preview_follow_enabled);

    seed_path(&app);
    assert(app_route_preview_toggle_follow(&app));
    assert(app.route_state_bridge.preview_follow_enabled);

    app_route_preview_disable_follow(&app);
    assert(!app.route_state_bridge.preview_follow_enabled);

    app_route_preview_set_follow_enabled(&app, true);
    assert(app.route_state_bridge.preview_follow_enabled);
    app.route_state_bridge.preview_heading_up = true;
    g_last_heading_target = 1.0f;

    app_route_preview_toggle_heading_mode(&app);
    assert(!app.route_state_bridge.preview_heading_up);
    assert(fabsf(g_last_heading_target - 0.0f) < 0.001f);
}

static void test_preview_matches_headless_playback_sample_on_shared_fixture(void) {
    AppState app;
    MapForgeHeadlessPlaybackSample sample;
    memset(&app, 0, sizeof(app));
    memset(&sample, 0, sizeof(sample));
    seed_path(&app);

    app.route_state_bridge.playback_time_s = 15.0f;
    app_route_preview_update(&app);

    assert(map_forge_headless_playback_sample(&app.route_state_bridge.route.graph,
                                              &app.route_state_bridge.route.path,
                                              NULL,
                                              15.0f,
                                              NULL,
                                              &sample));
    assert(app.route_state_bridge.preview.valid);
    assert(sample.valid);
    assert(app.route_state_bridge.preview.segment_index == sample.segment_index);
    assert(fabsf(app.route_state_bridge.preview.world_x - (float)sample.world_x) < 0.001f);
    assert(fabsf(app.route_state_bridge.preview.world_y - (float)sample.world_y) < 0.001f);
    assert(fabsf(app.route_state_bridge.preview.heading_rad - sample.heading_rad) < 0.001f);
}

int main(void) {
    test_preview_samples_paused_position_without_follow();
    test_preview_drives_follow_camera_while_playing();
    test_preview_follows_while_paused_when_follow_mode_enabled();
    test_preview_north_up_mode_forces_zero_heading();
    test_preview_spatial_heading_window_softens_subtle_bends();
    test_preview_heading_memory_smooths_abrupt_turn_transitions();
    test_preview_reset_clears_state();
    test_preview_follow_entrypoints_own_settings();
    test_preview_matches_headless_playback_sample_on_shared_fixture();
    printf("app_route_preview_test: success\n");
    return 0;
}
