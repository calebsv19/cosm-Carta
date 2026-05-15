#include "app/app_internal.h"

#include <math.h>
#include <string.h>

static float app_route_preview_clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float app_route_preview_normalize_angle(float angle_rad) {
    const float tau = 6.28318530717958647693f;
    if (!isfinite(angle_rad)) {
        return 0.0f;
    }

    angle_rad = fmodf(angle_rad, tau);
    if (angle_rad <= -3.14159265358979323846f) {
        angle_rad += tau;
    } else if (angle_rad > 3.14159265358979323846f) {
        angle_rad -= tau;
    }
    return angle_rad;
}

static float app_route_preview_shortest_angle_delta(float from_rad, float to_rad) {
    return app_route_preview_normalize_angle(to_rad - from_rad);
}

static float app_route_preview_heading_from_delta(float dx, float dy) {
    return atan2f(dx, dy);
}

static void app_route_preview_reset_heading_memory(AppState *app) {
    if (!app) {
        return;
    }
    app->route_state_bridge.preview_heading_memory_valid = false;
    app->route_state_bridge.preview_heading_memory_rad = 0.0f;
    app->route_state_bridge.preview_heading_memory_sample_time_s = 0.0f;
}

static bool app_route_preview_sample_path(const AppState *app,
                                          const RoutePath *path,
                                          float sample_time_s,
                                          uint32_t *out_segment_index,
                                          float *out_world_x,
                                          float *out_world_y) {
    if (!app || !path || !out_segment_index || !out_world_x || !out_world_y ||
        path->count < 2 || !path->nodes || !path->cumulative_time_s || path->total_time_s <= 0.0f) {
        return false;
    }

    const RouteGraph *graph = &app->route_state_bridge.route.graph;
    if (!graph->node_x || !graph->node_y) {
        return false;
    }

    float t = app_route_preview_clampf(sample_time_s, 0.0f, path->total_time_s);
    if (t <= 0.0f) {
        uint32_t node = path->nodes[0];
        *out_segment_index = 0u;
        *out_world_x = (float)graph->node_x[node];
        *out_world_y = (float)graph->node_y[node];
        return true;
    }
    if (t >= path->total_time_s) {
        uint32_t last_segment = path->count - 2u;
        uint32_t node = path->nodes[path->count - 1u];
        *out_segment_index = last_segment;
        *out_world_x = (float)graph->node_x[node];
        *out_world_y = (float)graph->node_y[node];
        return true;
    }

    uint32_t segment = path->count - 2u;
    for (uint32_t i = 0; i + 1u < path->count; ++i) {
        if (t <= path->cumulative_time_s[i + 1u]) {
            segment = i;
            break;
        }
    }

    float t0 = path->cumulative_time_s[segment];
    float t1 = path->cumulative_time_s[segment + 1u];
    float denom = t1 - t0;
    float alpha = denom > 0.0001f ? (t - t0) / denom : 0.0f;

    uint32_t from = path->nodes[segment];
    uint32_t to = path->nodes[segment + 1u];
    float ax = (float)graph->node_x[from];
    float ay = (float)graph->node_y[from];
    float bx = (float)graph->node_x[to];
    float by = (float)graph->node_y[to];

    *out_segment_index = segment;
    *out_world_x = ax + (bx - ax) * alpha;
    *out_world_y = ay + (by - ay) * alpha;
    return true;
}

void app_route_preview_reset(AppState *app) {
    if (!app) {
        return;
    }
    memset(&app->route_state_bridge.preview, 0, sizeof(app->route_state_bridge.preview));
    app_route_preview_reset_heading_memory(app);
}

static float app_route_preview_blend_heading(AppState *app,
                                             float raw_heading_rad,
                                             float sample_time_s) {
    if (!app) {
        return app_route_preview_normalize_angle(raw_heading_rad);
    }

    raw_heading_rad = app_route_preview_normalize_angle(raw_heading_rad);
    if (!app->route_state_bridge.preview_heading_memory_valid) {
        app->route_state_bridge.preview_heading_memory_valid = true;
        app->route_state_bridge.preview_heading_memory_rad = raw_heading_rad;
        app->route_state_bridge.preview_heading_memory_sample_time_s = sample_time_s;
        return raw_heading_rad;
    }

    float previous_heading_rad = app->route_state_bridge.preview_heading_memory_rad;
    float sample_dt_s = fabsf(sample_time_s - app->route_state_bridge.preview_heading_memory_sample_time_s);
    float effective_dt_s = app_route_preview_clampf(sample_dt_s, 0.0f, 0.25f);
    if (effective_dt_s <= 0.0001f) {
        return previous_heading_rad;
    }

    const float heading_memory_tau_s = 0.18f;
    float alpha = 1.0f - expf(-effective_dt_s / heading_memory_tau_s);
    float blended_heading_rad =
        app_route_preview_normalize_angle(previous_heading_rad +
                                          app_route_preview_shortest_angle_delta(previous_heading_rad, raw_heading_rad) * alpha);
    app->route_state_bridge.preview_heading_memory_rad = blended_heading_rad;
    app->route_state_bridge.preview_heading_memory_sample_time_s = sample_time_s;
    return blended_heading_rad;
}

void app_route_preview_update(AppState *app) {
    if (!app) {
        return;
    }

    AppRoutePreviewState preview = {0};
    const RoutePath *active_path = app_route_primary_path(app, NULL);
    if (!active_path || active_path->count < 2u || active_path->total_time_s <= 0.0f) {
        app->route_state_bridge.preview = preview;
        app_route_preview_reset_heading_memory(app);
        return;
    }

    if (!app_route_preview_sample_path(app,
                                       active_path,
                                       app->route_state_bridge.playback_time_s,
                                       &preview.segment_index,
                                       &preview.world_x,
                                       &preview.world_y)) {
        app->route_state_bridge.preview = preview;
        app_route_preview_reset_heading_memory(app);
        return;
    }

    preview.valid = true;
    preview.sample_time_s = app_route_preview_clampf(app->route_state_bridge.playback_time_s, 0.0f, active_path->total_time_s);

    float lookahead_window_s = app_route_preview_clampf(active_path->total_time_s * 0.02f, 1.0f, 5.0f);
    float heading_window_s = app_route_preview_clampf(active_path->total_time_s * 0.04f, 2.0f, 6.0f);
    float lookahead_x = preview.world_x;
    float lookahead_y = preview.world_y;
    uint32_t lookahead_segment = preview.segment_index;
    if (app_route_preview_sample_path(app,
                                      active_path,
                                      preview.sample_time_s + lookahead_window_s,
                                      &lookahead_segment,
                                      &lookahead_x,
                                      &lookahead_y)) {
        float dx = lookahead_x - preview.world_x;
        float dy = lookahead_y - preview.world_y;
        if (fabsf(dx) > 0.001f || fabsf(dy) > 0.001f) {
            preview.has_lookahead = true;
            preview.lookahead_world_x = lookahead_x;
            preview.lookahead_world_y = lookahead_y;
            preview.heading_rad = app_route_preview_heading_from_delta(dx, dy);
        }
    }

    float heading_start_x = preview.world_x;
    float heading_start_y = preview.world_y;
    float heading_end_x = lookahead_x;
    float heading_end_y = lookahead_y;
    uint32_t heading_window_segment = preview.segment_index;
    (void)app_route_preview_sample_path(app,
                                        active_path,
                                        preview.sample_time_s - heading_window_s * 0.5f,
                                        &heading_window_segment,
                                        &heading_start_x,
                                        &heading_start_y);
    (void)app_route_preview_sample_path(app,
                                        active_path,
                                        preview.sample_time_s + heading_window_s * 0.5f,
                                        &heading_window_segment,
                                        &heading_end_x,
                                        &heading_end_y);

    float heading_dx = heading_end_x - heading_start_x;
    float heading_dy = heading_end_y - heading_start_y;
    if (fabsf(heading_dx) > 0.001f || fabsf(heading_dy) > 0.001f) {
        preview.heading_rad = app_route_preview_heading_from_delta(heading_dx, heading_dy);
    }

    if (!preview.has_lookahead) {
        const RouteGraph *graph = &app->route_state_bridge.route.graph;
        uint32_t from = active_path->nodes[preview.segment_index];
        uint32_t to = active_path->nodes[preview.segment_index + 1u];
        float from_x = (float)graph->node_x[from];
        float from_y = (float)graph->node_y[from];
        float to_x = (float)graph->node_x[to];
        float to_y = (float)graph->node_y[to];
        preview.lookahead_world_x = to_x;
        preview.lookahead_world_y = to_y;
        preview.heading_rad = app_route_preview_heading_from_delta(to_x - from_x, to_y - from_y);
    }

    preview.heading_rad = app_route_preview_blend_heading(app, preview.heading_rad, preview.sample_time_s);
    preview.follow_active = app->route_state_bridge.preview_follow_enabled &&
                            !app->viewport_scenario_active;

    app->route_state_bridge.preview = preview;
    if (!preview.follow_active) {
        return;
    }

    app->view_state_bridge.camera.x_target = preview.world_x;
    app->view_state_bridge.camera.y_target = preview.world_y;
    camera_set_heading_target(&app->view_state_bridge.camera,
                              app->route_state_bridge.preview_heading_up ? preview.heading_rad : 0.0f);
}
