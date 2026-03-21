#include "app/app_internal.h"

#include "route/route_render.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static MapForgeThemePalette playback_theme_palette(void) {
    MapForgeThemePalette palette = {
        .route_panel_fill = {20, 26, 36, 248},
        .route_panel_outline = {80, 90, 110, 255},
        .route_progress_fill = {80, 170, 255, 240},
        .playback_marker_fill = {255, 230, 80, 240}
    };
    mapforge_shared_theme_resolve_palette(&palette);
    /* Keep route HUD nearly opaque to avoid map/route bleed-through artifacts. */
    if (palette.route_panel_fill.a < 244) {
        palette.route_panel_fill.a = 244;
    }
    if (palette.route_panel_outline.a < 245) {
        palette.route_panel_outline.a = 245;
    }
    return palette;
}

static const char *playback_objective_label(RouteObjective objective) {
    switch (objective) {
        case ROUTE_OBJECTIVE_SHORTEST_DISTANCE:
            return "Shortest";
        case ROUTE_OBJECTIVE_LOWEST_TIME:
            return "Lowest Time";
        case ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN:
            return "Low Elev Gain";
        case ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD:
            return "Time > Speed";
        default:
            return "Unknown";
    }
}

static SDL_Color playback_route_color(RouteObjective objective, bool selected) {
    (void)selected;
    SDL_Color color = {120, 170, 255, 220};
    switch (objective) {
        case ROUTE_OBJECTIVE_SHORTEST_DISTANCE:
            color.r = 40;
            color.g = 220;
            color.b = 255;
            break;
        case ROUTE_OBJECTIVE_LOWEST_TIME:
            color.r = 36;
            color.g = 142;
            color.b = 255;
            break;
        case ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN:
            color.r = 92;
            color.g = 255;
            color.b = 150;
            break;
        case ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD:
            color.r = 24;
            color.g = 236;
            color.b = 192;
            break;
        default:
            break;
    }
    return color;
}

static bool playback_point_in_rect(int x, int y, const SDL_FRect *rect) {
    if (!rect) {
        return false;
    }
    return (float)x >= rect->x &&
           (float)x <= rect->x + rect->w &&
           (float)y >= rect->y &&
           (float)y <= rect->y + rect->h;
}

static int playback_digits_scaled(float value, float scale) {
    int scaled = (int)fabsf(value * scale);
    int digits = 1;
    while (scaled >= 10) {
        scaled /= 10;
        digits += 1;
    }
    return digits;
}

static uint64_t playback_hash_mix_u64(uint64_t seed, uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    return seed;
}

static uint64_t playback_route_panel_layout_hash(const AppState *app) {
    if (!app) {
        return 0ull;
    }

    uint64_t hash = 1469598103934665603ull;
    hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)app->width);
    hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)app->route_state_bridge.route.mode);
    hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)app->route_state_bridge.route.objective);
    hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)app->route_state_bridge.route.alternatives.count);
    hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)app->route_state_bridge.route.drive_path.count);
    hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)app->route_state_bridge.route.walk_path.count);
    hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)playback_digits_scaled(app->route_state_bridge.route.drive_path.total_time_s / 60.0f, 10.0f));
    hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)playback_digits_scaled(app->route_state_bridge.route.walk_path.total_time_s / 60.0f, 10.0f));
    for (uint32_t i = 0; i < app->route_state_bridge.route.alternatives.count && i < ROUTE_ALTERNATIVE_MAX; ++i) {
        const RoutePath *path = &app->route_state_bridge.route.alternatives.paths[i];
        hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)app->route_state_bridge.route.alternatives.objectives[i]);
        hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)path->count);
        hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)playback_digits_scaled(path->total_length_m / 1000.0f, 100.0f));
        hash = playback_hash_mix_u64(hash, (uint64_t)(uint32_t)playback_digits_scaled(path->total_time_s / 60.0f, 10.0f));
    }
    return hash;
}

static bool playback_find_edge_index(const RouteGraph *graph, uint32_t from, uint32_t to, uint32_t *out_edge) {
    if (!graph || from >= graph->node_count || !out_edge) {
        return false;
    }
    uint32_t begin = graph->edge_start[from];
    uint32_t end = graph->edge_start[from + 1];
    for (uint32_t e = begin; e < end; ++e) {
        if (graph->edge_to[e] == to) {
            *out_edge = e;
            return true;
        }
    }
    return false;
}

static float playback_path_elevation_gain_m(const RouteGraph *graph, const RoutePath *path, bool *out_available) {
    if (out_available) {
        *out_available = false;
    }
    if (!graph || !path || path->count < 2 || !graph->edge_grade || !graph->edge_length) {
        return 0.0f;
    }
    float gain = 0.0f;
    for (uint32_t i = 0; i + 1 < path->count; ++i) {
        uint32_t edge = 0;
        if (!playback_find_edge_index(graph, path->nodes[i], path->nodes[i + 1], &edge)) {
            continue;
        }
        float grade = graph->edge_grade[edge];
        if (grade > 0.0f) {
            gain += grade * graph->edge_length[edge];
        }
    }
    if (out_available) {
        *out_available = true;
    }
    return gain;
}

static const RoutePath *playback_active_path(const AppState *app, int *out_alt_index) {
    uint32_t idx = UINT32_MAX;
    const RoutePath *path = app_route_primary_path(app, &idx);
    if (out_alt_index) {
        *out_alt_index = (idx == UINT32_MAX) ? -1 : (int)idx;
    }
    return path;
}

static void playback_format_route_line(char *buffer,
                                       size_t buffer_size,
                                       RouteTravelMode mode,
                                       RouteObjective objective,
                                       const RoutePath *path,
                                       float elevation_gain_m,
                                       bool elev_available) {
    if (!buffer || buffer_size == 0u || !path) {
        return;
    }

    float km = path->total_length_m / 1000.0f;
    float min = path->total_time_s / 60.0f;
    if (mode == ROUTE_MODE_WALK) {
        snprintf(buffer, buffer_size, "%s | %.2f km | %.1f min walk | elev %s",
                 playback_objective_label(objective), km, min, elev_available ? "" : "N/A");
    } else {
        snprintf(buffer, buffer_size, "%s | %.2f km | %.1f min | elev %s",
                 playback_objective_label(objective), km, min, elev_available ? "" : "N/A");
    }
    if (elev_available) {
        size_t len = strlen(buffer);
        snprintf(buffer + len, buffer_size - len, "%.0f m", elevation_gain_m);
    }
}

void app_playback_reset(AppState *app) {
    if (!app) {
        return;
    }
    app->route_state_bridge.playback_time_s = 0.0f;
    app->route_state_bridge.playback_playing = false;
}

void app_playback_update(AppState *app, float dt) {
    const RoutePath *active_path = playback_active_path(app, NULL);
    if (!app || !app->route_state_bridge.playback_playing || !active_path || active_path->count < 2 || active_path->total_time_s <= 0.0f) {
        return;
    }

    app->route_state_bridge.playback_time_s += dt * app->route_state_bridge.playback_speed;
    if (app->route_state_bridge.playback_time_s >= active_path->total_time_s) {
        app->route_state_bridge.playback_time_s = active_path->total_time_s;
        app->route_state_bridge.playback_playing = false;
    }
    if (app->route_state_bridge.playback_time_s < 0.0f) {
        app->route_state_bridge.playback_time_s = 0.0f;
    }
}

static bool app_playback_position(const AppState *app, float *out_x, float *out_y) {
    const RoutePath *active_path = playback_active_path(app, NULL);
    if (!app || !out_x || !out_y || !active_path || active_path->count < 2 ||
        !active_path->cumulative_time_s || active_path->total_time_s <= 0.0f) {
        return false;
    }

    float t = app->route_state_bridge.playback_time_s;
    if (t <= 0.0f) {
        uint32_t node = active_path->nodes[0];
        *out_x = (float)app->route_state_bridge.route.graph.node_x[node];
        *out_y = (float)app->route_state_bridge.route.graph.node_y[node];
        return true;
    }
    if (t >= active_path->total_time_s) {
        uint32_t node = active_path->nodes[active_path->count - 1];
        *out_x = (float)app->route_state_bridge.route.graph.node_x[node];
        *out_y = (float)app->route_state_bridge.route.graph.node_y[node];
        return true;
    }

    uint32_t segment = 0;
    for (uint32_t i = 0; i + 1 < active_path->count; ++i) {
        if (t <= active_path->cumulative_time_s[i + 1]) {
            segment = i;
            break;
        }
    }

    float t0 = active_path->cumulative_time_s[segment];
    float t1 = active_path->cumulative_time_s[segment + 1];
    float denom = t1 - t0;
    float alpha = denom > 0.0001f ? (t - t0) / denom : 0.0f;

    uint32_t a = active_path->nodes[segment];
    uint32_t b = active_path->nodes[segment + 1];
    float ax = (float)app->route_state_bridge.route.graph.node_x[a];
    float ay = (float)app->route_state_bridge.route.graph.node_y[a];
    float bx = (float)app->route_state_bridge.route.graph.node_x[b];
    float by = (float)app->route_state_bridge.route.graph.node_y[b];

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
    camera_world_to_screen(&app->view_state_bridge.camera, wx, wy, app->width, app->height, &sx, &sy);
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
    if (!app) {
        return;
    }

    if (app->route_state_bridge.route.path.count < 2 && app->route_state_bridge.route.alternatives.count == 0) {
        memset(&app->ui_state_bridge.hud_route_panel_rect, 0, sizeof(app->ui_state_bridge.hud_route_panel_rect));
        memset(&app->ui_state_bridge.hud_route_panel_collapse_rect, 0, sizeof(app->ui_state_bridge.hud_route_panel_collapse_rect));
        memset(&app->ui_state_bridge.hud_route_panel_handle_rect, 0, sizeof(app->ui_state_bridge.hud_route_panel_handle_rect));
        memset(&app->ui_state_bridge.hud_route_panel_row_rects, 0, sizeof(app->ui_state_bridge.hud_route_panel_row_rects));
        memset(&app->ui_state_bridge.hud_route_panel_toggle_rects, 0, sizeof(app->ui_state_bridge.hud_route_panel_toggle_rects));
        app->ui_state_bridge.hud_route_panel_layout_dirty = true;
        return;
    }

    int line_h = ui_font_line_height(1.0f);
    if (line_h <= 0) {
        line_h = 12;
    }

    int route_rows = 0;
    int alt_index = -1;
    const RoutePath *active_path = playback_active_path(app, &alt_index);
    if (!active_path) {
        active_path = &app->route_state_bridge.route.path;
    }
    float active_km = active_path && active_path->count >= 2 ? active_path->total_length_m / 1000.0f : 0.0f;
    float active_min = active_path && active_path->count >= 2 ? active_path->total_time_s / 60.0f : 0.0f;
    uint64_t layout_hash = playback_route_panel_layout_hash(app);
    if (app->ui_state_bridge.hud_route_panel_layout_dirty || app->ui_state_bridge.hud_route_panel_layout_hash != layout_hash) {
        int max_text_w = ui_measure_text_width("Route Comparison", 1.0f);
        memset(&app->ui_state_bridge.hud_route_panel_row_text, 0, sizeof(app->ui_state_bridge.hud_route_panel_row_text));
        route_rows = 0;
        for (uint32_t i = 0; i < app->route_state_bridge.route.alternatives.count && i < ROUTE_ALTERNATIVE_MAX; ++i) {
            const RoutePath *candidate = &app->route_state_bridge.route.alternatives.paths[i];
            if (candidate->count < 2) {
                continue;
            }
            bool elev_available = false;
            float elev_gain = playback_path_elevation_gain_m(&app->route_state_bridge.route.graph, candidate, &elev_available);
            playback_format_route_line(app->ui_state_bridge.hud_route_panel_row_text[i],
                                       APP_HUD_ROUTE_LINE_CAPACITY,
                                       app->route_state_bridge.route.mode,
                                       app->route_state_bridge.route.alternatives.objectives[i],
                                       candidate,
                                       elev_gain,
                                       elev_available);
            int w = ui_measure_text_width(app->ui_state_bridge.hud_route_panel_row_text[i], 1.0f);
            if (w > max_text_w) {
                max_text_w = w;
            }
            route_rows += 1;
        }
        if (route_rows == 0) {
            route_rows = 1;
        }

        if (app->route_state_bridge.route.mode == ROUTE_MODE_CAR &&
            app->route_state_bridge.route.drive_path.count > 1 &&
            app->route_state_bridge.route.walk_path.count > 1) {
            snprintf(app->ui_state_bridge.hud_route_panel_summary_text,
                     APP_HUD_ROUTE_LINE_CAPACITY,
                     "Primary: %s | %.2f km | drive %.1f min + walk %.1f min",
                     playback_objective_label(app->route_state_bridge.route.objective),
                     active_km,
                     app->route_state_bridge.route.drive_path.total_time_s / 60.0f,
                     app->route_state_bridge.route.walk_path.total_time_s / 60.0f);
        } else if (app->route_state_bridge.route.mode == ROUTE_MODE_WALK) {
            snprintf(app->ui_state_bridge.hud_route_panel_summary_text,
                     APP_HUD_ROUTE_LINE_CAPACITY,
                     "Primary: %s | %.2f km | %.1f min walk",
                     playback_objective_label(app->route_state_bridge.route.objective),
                     active_km,
                     active_min);
        } else {
            snprintf(app->ui_state_bridge.hud_route_panel_summary_text,
                     APP_HUD_ROUTE_LINE_CAPACITY,
                     "Primary: %s | %.2f km | %.1f min",
                     playback_objective_label(app->route_state_bridge.route.objective),
                     active_km,
                     active_min);
        }
        int summary_w = ui_measure_text_width(app->ui_state_bridge.hud_route_panel_summary_text, 1.0f);
        if (summary_w > max_text_w) {
            max_text_w = summary_w;
        }

        app->ui_state_bridge.hud_route_panel_cached_max_text_w = max_text_w;
        app->ui_state_bridge.hud_route_panel_cached_row_count = route_rows;
        app->ui_state_bridge.hud_route_panel_cached_w = (float)(max_text_w + 132);
        app->ui_state_bridge.hud_route_panel_cached_h = (float)(line_h * (5 + route_rows) + 30);
        app->ui_state_bridge.hud_route_panel_layout_hash = layout_hash;
        app->ui_state_bridge.hud_route_panel_layout_dirty = false;
    } else {
        route_rows = app->ui_state_bridge.hud_route_panel_cached_row_count > 0 ? app->ui_state_bridge.hud_route_panel_cached_row_count : 1;
    }

    float panel_w = app->ui_state_bridge.hud_route_panel_cached_w;
    float panel_w_max = (float)(app->width - 20);
    if (panel_w > panel_w_max) {
        panel_w = panel_w_max;
    }
    if (panel_w < 280.0f) {
        panel_w = 280.0f;
    }
    float panel_h = app->ui_state_bridge.hud_route_panel_cached_h;
    float pad = 10.0f;
    SDL_FRect panel = {(float)app->width - panel_w - pad, APP_HEADER_HEIGHT + pad, panel_w, panel_h};
    app->ui_state_bridge.hud_route_panel_rect = panel;
    memset(&app->ui_state_bridge.hud_route_panel_row_rects, 0, sizeof(app->ui_state_bridge.hud_route_panel_row_rects));
    memset(&app->ui_state_bridge.hud_route_panel_toggle_rects, 0, sizeof(app->ui_state_bridge.hud_route_panel_toggle_rects));

    MapForgeThemePalette palette = playback_theme_palette();

    if (app->ui_state_bridge.hud_route_panel_collapsed) {
        SDL_FRect handle = {(float)app->width - 26.0f, APP_HEADER_HEIGHT + 8.0f, 20.0f, 20.0f};
        app->ui_state_bridge.hud_route_panel_handle_rect = handle;
        memset(&app->ui_state_bridge.hud_route_panel_collapse_rect, 0, sizeof(app->ui_state_bridge.hud_route_panel_collapse_rect));
        renderer_set_draw_color(&app->renderer,
                                palette.route_panel_fill.r,
                                palette.route_panel_fill.g,
                                palette.route_panel_fill.b,
                                palette.route_panel_fill.a);
        renderer_fill_rect(&app->renderer, &handle);
        renderer_set_draw_color(&app->renderer,
                                palette.route_panel_outline.r,
                                palette.route_panel_outline.g,
                                palette.route_panel_outline.b,
                                palette.route_panel_outline.a);
        renderer_draw_rect(&app->renderer, &handle);
        ui_draw_text(&app->renderer, (int)handle.x + 6, (int)handle.y + 4, "<", (SDL_Color){226, 232, 242, 255}, 1.0f);
        return;
    }

    memset(&app->ui_state_bridge.hud_route_panel_handle_rect, 0, sizeof(app->ui_state_bridge.hud_route_panel_handle_rect));
    SDL_FRect collapse = {panel.x + panel.w - 18.0f, panel.y + 3.0f, 14.0f, 14.0f};
    app->ui_state_bridge.hud_route_panel_collapse_rect = collapse;

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
    renderer_fill_rect(&app->renderer, &(SDL_FRect){panel.x, panel.y, 3.0f, panel.h});

    SDL_Color text_primary = {226, 232, 242, 255};
    SDL_Color text_muted = {178, 190, 210, 255};
    int text_x = (int)(panel.x + 10.0f);
    int text_y = (int)(panel.y + 8.0f);
    int toggle_w = 34;
    int right_pad = 8;

    renderer_set_draw_color(&app->renderer, palette.route_panel_fill.r, palette.route_panel_fill.g, palette.route_panel_fill.b, 245);
    renderer_fill_rect(&app->renderer, &collapse);
    renderer_set_draw_color(&app->renderer, palette.route_panel_outline.r, palette.route_panel_outline.g, palette.route_panel_outline.b, palette.route_panel_outline.a);
    renderer_draw_rect(&app->renderer, &collapse);
    ui_draw_text(&app->renderer, (int)collapse.x + 4, (int)collapse.y + 1, "-", text_primary, 1.0f);

    ui_draw_text(&app->renderer, text_x, text_y, "Route Comparison", text_primary, 1.0f);
    text_y += line_h + 2;

    {
        ui_draw_text(&app->renderer, text_x, text_y, app->ui_state_bridge.hud_route_panel_summary_text, text_muted, 1.0f);
        text_y += line_h + 2;
    }

    for (uint32_t i = 0; i < app->route_state_bridge.route.alternatives.count && i < ROUTE_ALTERNATIVE_MAX; ++i) {
        const RoutePath *candidate = &app->route_state_bridge.route.alternatives.paths[i];
        if (candidate->count < 2) {
            continue;
        }

        bool selected = app->route_state_bridge.route.alternatives.objectives[i] == app->route_state_bridge.route.objective;
        bool visible = app->route_state_bridge.route_alt_visible[i];
        SDL_Color swatch = playback_route_color(app->route_state_bridge.route.alternatives.objectives[i], selected);
        SDL_FRect swatch_rect = {(float)text_x, (float)(text_y + 3), 8.0f, 8.0f};
        renderer_set_draw_color(&app->renderer, swatch.r, swatch.g, swatch.b, swatch.a);
        renderer_fill_rect(&app->renderer, &swatch_rect);

        int button_y = text_y - 1;
        SDL_FRect toggle_rect = {(float)(panel.x + panel.w - right_pad - toggle_w), (float)button_y, (float)toggle_w, (float)line_h};
        SDL_FRect row_rect = {(float)(text_x + 12), (float)button_y, (float)(toggle_rect.x - (float)(text_x + 12) - 6.0f), (float)line_h};
        if (row_rect.w < 1.0f) {
            row_rect.w = 1.0f;
        }
        app->ui_state_bridge.hud_route_panel_row_rects[i] = row_rect;
        app->ui_state_bridge.hud_route_panel_toggle_rects[i] = toggle_rect;

        renderer_set_draw_color(&app->renderer, selected ? 46 : 34, selected ? 74 : 44, selected ? 88 : 56, 236);
        renderer_fill_rect(&app->renderer, &row_rect);

        renderer_set_draw_color(&app->renderer, visible ? 52 : 46, visible ? 128 : 64, visible ? 84 : 68, 240);
        renderer_fill_rect(&app->renderer, &toggle_rect);
        renderer_set_draw_color(&app->renderer, 86, 100, 120, 230);
        renderer_draw_rect(&app->renderer, &toggle_rect);
        ui_draw_text(&app->renderer, (int)toggle_rect.x + 4, (int)toggle_rect.y + 1, visible ? "ON" : "OFF", text_primary, 1.0f);

        int row_text_max = (int)(toggle_rect.x - (float)(text_x + 14) - 6.0f);
        ui_draw_text_clipped(&app->renderer,
                             text_x + 14,
                             text_y,
                             app->ui_state_bridge.hud_route_panel_row_text[i][0] != '\0' ? app->ui_state_bridge.hud_route_panel_row_text[i] : playback_objective_label(app->route_state_bridge.route.alternatives.objectives[i]),
                             selected ? text_primary : text_muted,
                             1.0f,
                             row_text_max);
        text_y += line_h + 1;
    }

    if (active_path && active_path->total_time_s > 0.0f) {
        float progress = app->route_state_bridge.playback_time_s / active_path->total_time_s;
        if (progress < 0.0f) {
            progress = 0.0f;
        } else if (progress > 1.0f) {
            progress = 1.0f;
        }
        SDL_FRect bar_bg = {panel.x + 8.0f, panel.y + panel.h - 10.0f, panel.w - 16.0f, 4.0f};
        renderer_set_draw_color(&app->renderer, 46, 56, 72, 220);
        renderer_fill_rect(&app->renderer, &bar_bg);

        SDL_FRect bar = {bar_bg.x, bar_bg.y, bar_bg.w * progress, bar_bg.h};
        renderer_set_draw_color(&app->renderer,
                                palette.route_progress_fill.r,
                                palette.route_progress_fill.g,
                                palette.route_progress_fill.b,
                                palette.route_progress_fill.a);
        renderer_fill_rect(&app->renderer, &bar);

        char progress_line[128];
        snprintf(progress_line, sizeof(progress_line), "Playback (%s): %.1f / %.1f min",
                 playback_objective_label(app->route_state_bridge.route.objective),
                 app->route_state_bridge.playback_time_s / 60.0f, active_path->total_time_s / 60.0f);
        ui_draw_text(&app->renderer, text_x, (int)(bar_bg.y - (float)line_h - 2.0f), progress_line, text_muted, 1.0f);
    }
}

bool app_route_panel_handle_click(AppState *app) {
    if (!app) {
        return false;
    }
    bool any_click = app->ui_state_bridge.input.left_click_pressed || app->ui_state_bridge.input.right_click_pressed || app->ui_state_bridge.input.middle_click_pressed;
    if (!any_click) {
        return false;
    }

    int mx = app->ui_state_bridge.input.mouse_x;
    int my = app->ui_state_bridge.input.mouse_y;

    if (app->ui_state_bridge.hud_route_panel_collapsed) {
        if (app->ui_state_bridge.input.left_click_pressed && playback_point_in_rect(mx, my, &app->ui_state_bridge.hud_route_panel_handle_rect)) {
            app->ui_state_bridge.hud_route_panel_collapsed = false;
            return true;
        }
        return playback_point_in_rect(mx, my, &app->ui_state_bridge.hud_route_panel_handle_rect);
    }

    if (app->ui_state_bridge.input.left_click_pressed && playback_point_in_rect(mx, my, &app->ui_state_bridge.hud_route_panel_collapse_rect)) {
        app->ui_state_bridge.hud_route_panel_collapsed = true;
        return true;
    }

    for (uint32_t i = 0; i < ROUTE_ALTERNATIVE_MAX; ++i) {
        if (app->ui_state_bridge.input.left_click_pressed && playback_point_in_rect(mx, my, &app->ui_state_bridge.hud_route_panel_row_rects[i])) {
            if (i < app->route_state_bridge.route.alternatives.count) {
                RouteObjective next_objective = app->route_state_bridge.route.alternatives.objectives[i];
                bool objective_changed = app->route_state_bridge.route.objective != next_objective;
                app->route_state_bridge.route.objective = next_objective;
                app->route_state_bridge.route_alt_visible[i] = true;
                if (objective_changed) {
                    route_path_free(&app->route_state_bridge.route.drive_path);
                    route_path_free(&app->route_state_bridge.route.walk_path);
                    app->route_state_bridge.route.has_transfer = false;
                    app->route_state_bridge.route.transfer_node = 0;
                }
                if (app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
                    app_route_schedule_recompute(app, 0.0);
                }
                app_playback_reset(app);
            }
            return true;
        }
        if (app->ui_state_bridge.input.left_click_pressed && playback_point_in_rect(mx, my, &app->ui_state_bridge.hud_route_panel_toggle_rects[i])) {
            app->route_state_bridge.route_alt_visible[i] = !app->route_state_bridge.route_alt_visible[i];
            return true;
        }
    }

    return playback_point_in_rect(mx, my, &app->ui_state_bridge.hud_route_panel_rect);
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
