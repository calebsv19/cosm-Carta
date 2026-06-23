#include "app/app_headless.h"

#include "route_preview_test_fixture.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_playback_plan_requires_duration_and_fps(void) {
    RouteGraph graph;
    RoutePath path;
    MapForgeHeadlessPlaybackConfig config;
    float playback_duration_s = 0.0f;
    int fps = 0;
    uint32_t frame_count = 0u;

    mapforge_test_route_fixture_seed_corner(&graph, &path);
    memset(&config, 0, sizeof(config));
    assert(!map_forge_headless_playback_plan(&config, &path, &playback_duration_s, &fps, &frame_count));

    config.has_duration_seconds = true;
    config.duration_seconds = 12.0f;
    config.has_fps = true;
    config.fps = 30;
    assert(map_forge_headless_playback_plan(&config, &path, &playback_duration_s, &fps, &frame_count));
    assert(fabsf(playback_duration_s - 12.0f) < 0.001f);
    assert(fps == 30);
    assert(frame_count == 360u);
}

static void test_playback_sample_tracks_midpoint_without_heading_state(void) {
    RouteGraph graph;
    RoutePath path;
    MapForgeHeadlessPlaybackSample sample;

    mapforge_test_route_fixture_seed_corner(&graph, &path);
    memset(&sample, 0, sizeof(sample));

    assert(map_forge_headless_playback_sample(&graph, &path, NULL, 5.0f, NULL, &sample));
    assert(sample.valid);
    assert(sample.segment_index == 0u);
    assert(fabs(sample.world_x - 0.0) < 0.001);
    assert(fabs(sample.world_y - 5.0) < 0.001);
    assert(fabsf(sample.progress - 0.25f) < 0.001f);
    assert(fabsf(sample.heading_rad - 0.0f) < 0.001f);
}

static void test_playback_sample_uses_heading_memory_for_abrupt_turn(void) {
    RouteGraph graph;
    RoutePath path;
    MapForgeHeadlessPlaybackSample sample;
    MapForgeHeadlessPlaybackHeadingState heading_state;

    mapforge_test_route_fixture_seed_corner(&graph, &path);
    memset(&sample, 0, sizeof(sample));
    map_forge_headless_playback_reset_heading_state(&heading_state);

    assert(map_forge_headless_playback_sample(&graph, &path, NULL, 5.0f, &heading_state, &sample));
    assert(fabsf(sample.heading_rad - 0.0f) < 0.001f);

    assert(map_forge_headless_playback_sample(&graph, &path, NULL, 10.1f, &heading_state, &sample));
    assert(sample.valid);
    assert(sample.heading_rad > 0.2f);
    assert(sample.heading_rad < 1.3f);
}

static void test_playback_sample_softens_subtle_bends(void) {
    RouteGraph graph;
    RoutePath path;
    MapForgeHeadlessPlaybackSample sample;

    mapforge_test_route_fixture_seed_sway(&graph, &path);
    memset(&sample, 0, sizeof(sample));

    assert(map_forge_headless_playback_sample(&graph, &path, NULL, 10.5f, NULL, &sample));
    assert(sample.valid);

    {
        const float raw_segment_heading = atan2f(1.0f, 10.0f);
        assert(sample.heading_rad > 0.0f);
        assert(sample.heading_rad < raw_segment_heading);
    }
}

static void test_playback_sample_respects_heading_rate_limit(void) {
    RouteGraph graph;
    RoutePath path;
    MapForgeHeadlessPlaybackSample sample;
    MapForgeHeadlessPlaybackHeadingState heading_state;
    MapForgeHeadlessPlaybackConfig config;

    mapforge_test_route_fixture_seed_corner(&graph, &path);
    memset(&sample, 0, sizeof(sample));
    memset(&config, 0, sizeof(config));
    map_forge_headless_playback_reset_heading_state(&heading_state);
    config.heading.mode = MAPFORGE_HEADLESS_HEADING_MODE_PATH_TANGENT;
    config.heading.has_smoothing_tau_seconds = true;
    config.heading.smoothing_tau_seconds = 0.05f;
    config.heading.has_max_turn_rate_deg_per_sec = true;
    config.heading.max_turn_rate_deg_per_sec = 30.0f;

    assert(map_forge_headless_playback_sample(&graph, &path, &config, 5.0f, &heading_state, &sample));
    assert(fabsf(sample.heading_rad - 0.0f) < 0.001f);

    assert(map_forge_headless_playback_sample(&graph, &path, &config, 10.1f, &heading_state, &sample));
    assert(sample.valid);
    assert(sample.heading_rad > 0.0f);
    assert(sample.heading_rad < 0.2f);
}

int main(void) {
    test_playback_plan_requires_duration_and_fps();
    test_playback_sample_tracks_midpoint_without_heading_state();
    test_playback_sample_uses_heading_memory_for_abrupt_turn();
    test_playback_sample_softens_subtle_bends();
    test_playback_sample_respects_heading_rate_limit();
    printf("app_headless_playback_test: success\n");
    return 0;
}
