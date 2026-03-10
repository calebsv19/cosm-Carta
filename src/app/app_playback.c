#include "app/app_internal.h"

#include "route/route_render.h"
#include "ui/shared_theme_font_adapter.h"

#include <math.h>

static MapForgeThemePalette playback_theme_palette(void) {
    MapForgeThemePalette palette = {
        .route_panel_fill = {20, 26, 36, 210},
        .route_panel_outline = {80, 90, 110, 220},
        .route_progress_fill = {80, 170, 255, 220},
        .playback_marker_fill = {255, 230, 80, 240}
    };
    mapforge_shared_theme_resolve_palette(&palette);
    return palette;
}

void app_playback_reset(AppState *app) {
    if (!app) {
        return;
    }
    app->playback_time_s = 0.0f;
    app->playback_playing = false;
}

void app_playback_update(AppState *app, float dt) {
    if (!app || !app->playback_playing || app->route.path.count < 2 || app->route.path.total_time_s <= 0.0f) {
        return;
    }

    app->playback_time_s += dt * app->playback_speed;
    if (app->playback_time_s >= app->route.path.total_time_s) {
        app->playback_time_s = app->route.path.total_time_s;
        app->playback_playing = false;
    }
    if (app->playback_time_s < 0.0f) {
        app->playback_time_s = 0.0f;
    }
}

static bool app_playback_position(const AppState *app, float *out_x, float *out_y) {
    if (!app || !out_x || !out_y || app->route.path.count < 2 ||
        !app->route.path.cumulative_time_s || app->route.path.total_time_s <= 0.0f) {
        return false;
    }

    float t = app->playback_time_s;
    if (t <= 0.0f) {
        uint32_t node = app->route.path.nodes[0];
        *out_x = (float)app->route.graph.node_x[node];
        *out_y = (float)app->route.graph.node_y[node];
        return true;
    }
    if (t >= app->route.path.total_time_s) {
        uint32_t node = app->route.path.nodes[app->route.path.count - 1];
        *out_x = (float)app->route.graph.node_x[node];
        *out_y = (float)app->route.graph.node_y[node];
        return true;
    }

    uint32_t segment = 0;
    for (uint32_t i = 0; i + 1 < app->route.path.count; ++i) {
        if (t <= app->route.path.cumulative_time_s[i + 1]) {
            segment = i;
            break;
        }
    }

    float t0 = app->route.path.cumulative_time_s[segment];
    float t1 = app->route.path.cumulative_time_s[segment + 1];
    float denom = t1 - t0;
    float alpha = denom > 0.0001f ? (t - t0) / denom : 0.0f;

    uint32_t a = app->route.path.nodes[segment];
    uint32_t b = app->route.path.nodes[segment + 1];
    float ax = (float)app->route.graph.node_x[a];
    float ay = (float)app->route.graph.node_y[a];
    float bx = (float)app->route.graph.node_x[b];
    float by = (float)app->route.graph.node_y[b];

    *out_x = ax + (bx - ax) * alpha;
    *out_y = ay + (by - ay) * alpha;
    return true;
}

void app_draw_playback_marker(AppState *app) {
    if (!app) {
        return;
    }

    float wx = 0.0f;
    float wy = 0.0f;
    if (!app_playback_position(app, &wx, &wy)) {
        return;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->camera, wx, wy, app->width, app->height, &sx, &sy);
    MapForgeThemePalette palette = playback_theme_palette();
    renderer_set_draw_color(&app->renderer,
                            palette.playback_marker_fill.r,
                            palette.playback_marker_fill.g,
                            palette.playback_marker_fill.b,
                            palette.playback_marker_fill.a);
    SDL_FRect rect = {sx - 4.0f, sy - 4.0f, 8.0f, 8.0f};
    renderer_fill_rect(&app->renderer, &rect);
}

void app_draw_route_panel(AppState *app) {
    if (!app || app->route.path.count < 2) {
        return;
    }

    float panel_w = 220.0f;
    float panel_h = 36.0f;
    float pad = 10.0f;
    SDL_FRect panel = {pad, APP_HEADER_HEIGHT + pad, panel_w, panel_h};
    MapForgeThemePalette palette = playback_theme_palette();
    renderer_set_draw_color(&app->renderer,
                            palette.route_panel_fill.r,
                            palette.route_panel_fill.g,
                            palette.route_panel_fill.b,
                            palette.route_panel_fill.a);
    renderer_fill_rect(&app->renderer, &panel);
    renderer_set_draw_color(&app->renderer,
                            palette.route_panel_outline.r,
                            palette.route_panel_outline.g,
                            palette.route_panel_outline.b,
                            palette.route_panel_outline.a);
    renderer_draw_rect(&app->renderer, &panel);

    if (app->route.path.total_time_s > 0.0f) {
        float progress = app->playback_time_s / app->route.path.total_time_s;
        if (progress < 0.0f) {
            progress = 0.0f;
        } else if (progress > 1.0f) {
            progress = 1.0f;
        }
        SDL_FRect bar = {panel.x + 8.0f, panel.y + panel.h - 10.0f, (panel.w - 16.0f) * progress, 4.0f};
        renderer_set_draw_color(&app->renderer,
                                palette.route_progress_fill.r,
                                palette.route_progress_fill.g,
                                palette.route_progress_fill.b,
                                palette.route_progress_fill.a);
        renderer_fill_rect(&app->renderer, &bar);
    }
}

float app_next_playback_speed(float current, int direction) {
    static const float kSpeeds[] = {1.0f, 2.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f};
    size_t count = sizeof(kSpeeds) / sizeof(kSpeeds[0]);
    size_t idx = 0;
    for (size_t i = 0; i < count; ++i) {
        if (fabsf(kSpeeds[i] - current) < 0.01f) {
            idx = i;
            break;
        }
    }
    if (direction > 0 && idx + 1 < count) {
        idx += 1;
    } else if (direction < 0 && idx > 0) {
        idx -= 1;
    }
    return kSpeeds[idx];
}
