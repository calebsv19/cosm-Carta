#include "app/app_headless.h"

#include <math.h>
#include <string.h>

static float map_forge_headless_playback_clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float map_forge_headless_playback_deg_to_rad(float degrees) {
    return degrees * 0.01745329251994329577f;
}

static float map_forge_headless_playback_normalize_angle(float angle_rad) {
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

static float map_forge_headless_playback_shortest_angle_delta(float from_rad, float to_rad) {
    return map_forge_headless_playback_normalize_angle(to_rad - from_rad);
}

static float map_forge_headless_playback_heading_from_delta(float dx, float dy) {
    return atan2f(dx, dy);
}

static MapForgeHeadlessHeadingMode map_forge_headless_playback_heading_mode(const MapForgeHeadlessPlaybackConfig *config) {
    if (!config) {
        return MAPFORGE_HEADLESS_HEADING_MODE_BLENDED;
    }
    return config->heading.mode;
}

static float map_forge_headless_playback_heading_tau_seconds(const MapForgeHeadlessPlaybackConfig *config) {
    if (config && config->heading.has_smoothing_tau_seconds) {
        return map_forge_headless_playback_clampf(config->heading.smoothing_tau_seconds, 0.01f, 5.0f);
    }
    return 0.18f;
}

static float map_forge_headless_playback_lookahead_seconds(const MapForgeHeadlessPlaybackConfig *config,
                                                           const RoutePath *path) {
    if (config && config->heading.has_lookahead_seconds) {
        return map_forge_headless_playback_clampf(config->heading.lookahead_seconds, 0.05f, 12.0f);
    }
    return map_forge_headless_playback_clampf(path ? path->total_time_s * 0.02f : 2.0f, 1.0f, 5.0f);
}

static float map_forge_headless_playback_measurement_window_seconds(const MapForgeHeadlessPlaybackConfig *config,
                                                                    const RoutePath *path) {
    if (config && config->heading.has_measurement_window_seconds) {
        return map_forge_headless_playback_clampf(config->heading.measurement_window_seconds, 0.05f, 12.0f);
    }
    return map_forge_headless_playback_clampf(path ? path->total_time_s * 0.04f : 3.0f, 2.0f, 6.0f);
}

static float map_forge_headless_playback_max_turn_rate_rad_per_s(const MapForgeHeadlessPlaybackConfig *config) {
    if (config && config->heading.has_max_turn_rate_deg_per_sec &&
        config->heading.max_turn_rate_deg_per_sec > 0.0f) {
        return map_forge_headless_playback_deg_to_rad(config->heading.max_turn_rate_deg_per_sec);
    }
    return 0.0f;
}

static bool map_forge_headless_playback_sample_path(const RouteGraph *graph,
                                                    const RoutePath *path,
                                                    float route_time_s,
                                                    uint32_t *out_segment_index,
                                                    double *out_world_x,
                                                    double *out_world_y) {
    if (!graph || !path || !out_segment_index || !out_world_x || !out_world_y ||
        path->count < 2u || !path->nodes || !path->cumulative_time_s || path->total_time_s <= 0.0f ||
        !graph->node_x || !graph->node_y) {
        return false;
    }

    route_time_s = map_forge_headless_playback_clampf(route_time_s, 0.0f, path->total_time_s);
    if (route_time_s <= 0.0f) {
        uint32_t node = path->nodes[0];
        *out_segment_index = 0u;
        *out_world_x = graph->node_x[node];
        *out_world_y = graph->node_y[node];
        return true;
    }
    if (route_time_s >= path->total_time_s) {
        uint32_t last_segment = path->count - 2u;
        uint32_t node = path->nodes[path->count - 1u];
        *out_segment_index = last_segment;
        *out_world_x = graph->node_x[node];
        *out_world_y = graph->node_y[node];
        return true;
    }

    uint32_t segment = path->count - 2u;
    for (uint32_t i = 0; i + 1u < path->count; ++i) {
        if (route_time_s <= path->cumulative_time_s[i + 1u]) {
            segment = i;
            break;
        }
    }

    {
        float t0 = path->cumulative_time_s[segment];
        float t1 = path->cumulative_time_s[segment + 1u];
        float denom = t1 - t0;
        float alpha = denom > 0.0001f ? (route_time_s - t0) / denom : 0.0f;

        uint32_t from = path->nodes[segment];
        uint32_t to = path->nodes[segment + 1u];
        double ax = graph->node_x[from];
        double ay = graph->node_y[from];
        double bx = graph->node_x[to];
        double by = graph->node_y[to];

        *out_segment_index = segment;
        *out_world_x = ax + (bx - ax) * (double)alpha;
        *out_world_y = ay + (by - ay) * (double)alpha;
    }
    return true;
}

static float map_forge_headless_playback_blend_heading(const MapForgeHeadlessPlaybackConfig *config,
                                                       MapForgeHeadlessPlaybackHeadingState *state,
                                                       float raw_heading_rad,
                                                       float route_time_s) {
    raw_heading_rad = map_forge_headless_playback_normalize_angle(raw_heading_rad);
    if (!state) {
        return raw_heading_rad;
    }
    if (!state->valid) {
        state->valid = true;
        state->heading_rad = raw_heading_rad;
        state->route_time_s = route_time_s;
        return raw_heading_rad;
    }

    {
        float sample_dt_s = fabsf(route_time_s - state->route_time_s);
        float effective_dt_s = map_forge_headless_playback_clampf(sample_dt_s, 0.0f, 0.25f);
        if (effective_dt_s <= 0.0001f) {
            return state->heading_rad;
        }

        {
            const float heading_memory_tau_s = map_forge_headless_playback_heading_tau_seconds(config);
            const float max_turn_rate_rad_per_s = map_forge_headless_playback_max_turn_rate_rad_per_s(config);
            float alpha = 1.0f - expf(-effective_dt_s / heading_memory_tau_s);
            float desired_delta_rad =
                map_forge_headless_playback_shortest_angle_delta(state->heading_rad, raw_heading_rad) * alpha;
            if (max_turn_rate_rad_per_s > 0.0f) {
                float max_step_rad = max_turn_rate_rad_per_s * effective_dt_s;
                desired_delta_rad = map_forge_headless_playback_clampf(desired_delta_rad,
                                                                       -max_step_rad,
                                                                       max_step_rad);
            }
            float blended_heading_rad =
                map_forge_headless_playback_normalize_angle(state->heading_rad + desired_delta_rad);
            state->heading_rad = blended_heading_rad;
            state->route_time_s = route_time_s;
            return blended_heading_rad;
        }
    }
}

void map_forge_headless_playback_reset_heading_state(MapForgeHeadlessPlaybackHeadingState *state) {
    if (!state) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

bool map_forge_headless_playback_plan(const MapForgeHeadlessPlaybackConfig *config,
                                      const RoutePath *path,
                                      float *out_playback_duration_s,
                                      int *out_fps,
                                      uint32_t *out_frame_count) {
    float duration_seconds = 0.0f;
    int fps = 0;
    uint32_t frame_count = 0u;

    if (!config || !path || !out_playback_duration_s || !out_fps || !out_frame_count ||
        path->count < 2u || path->total_time_s <= 0.0f ||
        !config->has_duration_seconds || !config->has_fps ||
        config->duration_seconds <= 0.0f || config->fps <= 0) {
        return false;
    }

    duration_seconds = config->duration_seconds;
    fps = config->fps;
    frame_count = (uint32_t)(duration_seconds * (float)fps + 0.5f);
    if (frame_count == 0u) {
        frame_count = 1u;
    }

    *out_playback_duration_s = duration_seconds;
    *out_fps = fps;
    *out_frame_count = frame_count;
    return true;
}

bool map_forge_headless_playback_sample(const RouteGraph *graph,
                                        const RoutePath *path,
                                        const MapForgeHeadlessPlaybackConfig *config,
                                        float route_time_s,
                                        MapForgeHeadlessPlaybackHeadingState *io_heading_state,
                                        MapForgeHeadlessPlaybackSample *out_sample) {
    MapForgeHeadlessPlaybackSample sample;
    uint32_t lookahead_segment = 0u;
    uint32_t heading_segment = 0u;

    if (!graph || !path || !out_sample || path->count < 2u || path->total_time_s <= 0.0f) {
        return false;
    }

    memset(&sample, 0, sizeof(sample));
    if (!map_forge_headless_playback_sample_path(graph,
                                                 path,
                                                 route_time_s,
                                                 &sample.segment_index,
                                                 &sample.world_x,
                                                 &sample.world_y)) {
        return false;
    }

    sample.valid = true;
    sample.route_time_s = map_forge_headless_playback_clampf(route_time_s, 0.0f, path->total_time_s);
    sample.progress = path->total_time_s > 0.0f ? (sample.route_time_s / path->total_time_s) : 0.0f;

    {
        float lookahead_window_s = map_forge_headless_playback_lookahead_seconds(config, path);
        float heading_window_s = map_forge_headless_playback_measurement_window_seconds(config, path);
        MapForgeHeadlessHeadingMode heading_mode = map_forge_headless_playback_heading_mode(config);
        double lookahead_x = sample.world_x;
        double lookahead_y = sample.world_y;
        double heading_start_x = sample.world_x;
        double heading_start_y = sample.world_y;
        double heading_end_x = sample.world_x;
        double heading_end_y = sample.world_y;

        if (map_forge_headless_playback_sample_path(graph,
                                                    path,
                                                    sample.route_time_s + lookahead_window_s,
                                                    &lookahead_segment,
                                                    &lookahead_x,
                                                    &lookahead_y)) {
            double dx = lookahead_x - sample.world_x;
            double dy = lookahead_y - sample.world_y;
            if (fabs(dx) > 0.001 || fabs(dy) > 0.001) {
                sample.has_lookahead = true;
                sample.lookahead_world_x = lookahead_x;
                sample.lookahead_world_y = lookahead_y;
                sample.heading_rad = map_forge_headless_playback_heading_from_delta((float)dx, (float)dy);
            }
        }

        if (heading_mode == MAPFORGE_HEADLESS_HEADING_MODE_PATH_TANGENT ||
            heading_mode == MAPFORGE_HEADLESS_HEADING_MODE_BLENDED) {
            (void)map_forge_headless_playback_sample_path(graph,
                                                          path,
                                                          sample.route_time_s - heading_window_s * 0.5f,
                                                          &heading_segment,
                                                          &heading_start_x,
                                                          &heading_start_y);
            (void)map_forge_headless_playback_sample_path(graph,
                                                          path,
                                                          sample.route_time_s + heading_window_s * 0.5f,
                                                          &heading_segment,
                                                          &heading_end_x,
                                                          &heading_end_y);

            {
                double heading_dx = heading_end_x - heading_start_x;
                double heading_dy = heading_end_y - heading_start_y;
                if (fabs(heading_dx) > 0.001 || fabs(heading_dy) > 0.001) {
                    sample.heading_rad =
                        map_forge_headless_playback_heading_from_delta((float)heading_dx, (float)heading_dy);
                }
            }
        }

        if (heading_mode == MAPFORGE_HEADLESS_HEADING_MODE_LOOKAHEAD && sample.has_lookahead) {
            double dx = sample.lookahead_world_x - sample.world_x;
            double dy = sample.lookahead_world_y - sample.world_y;
            sample.heading_rad = map_forge_headless_playback_heading_from_delta((float)dx, (float)dy);
        } else if (!sample.has_lookahead) {
            uint32_t from = path->nodes[sample.segment_index];
            uint32_t to = path->nodes[sample.segment_index + 1u];
            double from_x = graph->node_x[from];
            double from_y = graph->node_y[from];
            double to_x = graph->node_x[to];
            double to_y = graph->node_y[to];
            sample.lookahead_world_x = to_x;
            sample.lookahead_world_y = to_y;
            sample.heading_rad =
                map_forge_headless_playback_heading_from_delta((float)(to_x - from_x), (float)(to_y - from_y));
        }
    }

    sample.heading_rad = map_forge_headless_playback_blend_heading(config,
                                                                   io_heading_state,
                                                                   sample.heading_rad,
                                                                   sample.route_time_s);
    *out_sample = sample;
    return true;
}
