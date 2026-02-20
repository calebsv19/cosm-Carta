#include "app/app_internal.h"

#include "ui/font.h"

#include <stdio.h>

static SDL_FRect app_header_button_rect(const AppState *app) {
    float width = 140.0f;
    float height = 22.0f;
    float pad = 8.0f;
    SDL_FRect rect = {pad, (APP_HEADER_HEIGHT - height) * 0.5f, width, height};
    (void)app;
    return rect;
}

bool app_header_button_hit(const AppState *app, int x, int y) {
    if (!app) {
        return false;
    }
    if (y < 0 || y > (int)APP_HEADER_HEIGHT) {
        return false;
    }
    SDL_FRect rect = app_header_button_rect(app);
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

static const char *app_header_layer_chip_label(TileLayerKind kind) {
    switch (kind) {
        case TILE_LAYER_ROAD_ARTERY:
            return "ART";
        case TILE_LAYER_ROAD_LOCAL:
            return "LOC";
        case TILE_LAYER_POLY_WATER:
            return "WAT";
        case TILE_LAYER_POLY_PARK:
            return "PRK";
        case TILE_LAYER_POLY_LANDUSE:
            return "LND";
        case TILE_LAYER_POLY_BUILDING:
            return "BLD";
        default:
            return "---";
    }
}

static void app_draw_header_layer_chips(AppState *app, float left_limit, float box_y, float box_h, int text_h) {
    if (!app) {
        return;
    }

    static const TileLayerKind kHeaderChipPriority[] = {
        TILE_LAYER_ROAD_ARTERY,
        TILE_LAYER_ROAD_LOCAL,
        TILE_LAYER_POLY_WATER,
        TILE_LAYER_POLY_PARK,
        TILE_LAYER_POLY_LANDUSE,
        TILE_LAYER_POLY_BUILDING
    };

    const float chip_w = 88.0f;
    const float chip_gap = 6.0f;
    const float right_pad = 8.0f;
    const float text_pad_x = 6.0f;
    const float progress_h = 3.0f;
    SDL_Color text_color = {225, 230, 240, 255};
    float available_w = ((float)app->width - right_pad) - left_limit;
    if (available_w < chip_w) {
        return;
    }
    int max_chips = (int)((available_w + chip_gap) / (chip_w + chip_gap));
    int total_chips = (int)(sizeof(kHeaderChipPriority) / sizeof(kHeaderChipPriority[0]));
    if (max_chips > total_chips) {
        max_chips = total_chips;
    }

    float cursor_right = (float)app->width - right_pad;

    for (int i = 0; i < max_chips; ++i) {
        TileLayerKind kind = kHeaderChipPriority[i];
        float chip_x = cursor_right - chip_w;
        if (chip_x < left_limit) {
            break;
        }

        uint32_t expected = layer_policy_requires_full_ready(kind)
            ? app->layer_expected[kind]
            : app->layer_visible_expected[kind];
        uint32_t done = layer_policy_requires_full_ready(kind)
            ? app->layer_done[kind]
            : app->layer_visible_loaded[kind];
        bool runtime_active = app_layer_active_runtime(app, kind);
        bool is_ready = app->layer_state[kind] == LAYER_READINESS_READY;
        bool is_loading = app->layer_state[kind] == LAYER_READINESS_LOADING && expected > 0;

        if (!runtime_active || app->layer_state[kind] == LAYER_READINESS_HIDDEN) {
            renderer_set_draw_color(&app->renderer, 34, 38, 48, 200);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
            renderer_set_draw_color(&app->renderer, 58, 64, 80, 220);
            renderer_draw_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
        } else if (is_loading) {
            renderer_set_draw_color(&app->renderer, 30, 42, 58, 220);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
            renderer_set_draw_color(&app->renderer, 92, 146, 210, 230);
            renderer_draw_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
        } else if (is_ready) {
            renderer_set_draw_color(&app->renderer, 28, 50, 44, 220);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
            renderer_set_draw_color(&app->renderer, 78, 170, 130, 230);
            renderer_draw_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
        } else {
            renderer_set_draw_color(&app->renderer, 30, 35, 46, 220);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
            renderer_set_draw_color(&app->renderer, 80, 90, 110, 220);
            renderer_draw_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
        }

        char chip_text[24];
        const char *tag = app_header_layer_chip_label(kind);
        if (is_loading) {
            snprintf(chip_text, sizeof(chip_text), "%s %u/%u", tag, done, expected);
        } else {
            snprintf(chip_text, sizeof(chip_text), "%s", tag);
        }

        ui_draw_text(&app->renderer,
                     (int)(chip_x + text_pad_x),
                     (int)(box_y + (box_h - (float)text_h) * 0.5f),
                     chip_text,
                     text_color,
                     1.0f);

        if (is_loading && expected > 0) {
            float progress = app_clampf((float)done / (float)expected, 0.0f, 1.0f);
            SDL_FRect bar_bg = {chip_x + text_pad_x, box_y + box_h - 6.0f, chip_w - text_pad_x * 2.0f, progress_h};
            renderer_set_draw_color(&app->renderer, 42, 56, 76, 220);
            renderer_fill_rect(&app->renderer, &bar_bg);
            SDL_FRect bar_fg = {bar_bg.x, bar_bg.y, bar_bg.w * progress, bar_bg.h};
            renderer_set_draw_color(&app->renderer, 110, 180, 255, 230);
            renderer_fill_rect(&app->renderer, &bar_fg);
        }

        cursor_right = chip_x - chip_gap;
    }
}

void app_draw_header_bar(AppState *app) {
    if (!app) {
        return;
    }

    renderer_set_draw_color(&app->renderer, 18, 20, 28, 230);
    SDL_FRect bar = {0.0f, 0.0f, (float)app->width, APP_HEADER_HEIGHT};
    renderer_fill_rect(&app->renderer, &bar);

    SDL_FRect button = app_header_button_rect(app);
    renderer_set_draw_color(&app->renderer, 70, 80, 100, 220);
    renderer_draw_rect(&app->renderer, &button);

    SDL_FRect left = {button.x + 1.0f, button.y + 1.0f, (button.w * 0.5f) - 2.0f, button.h - 2.0f};
    SDL_FRect right = {button.x + button.w * 0.5f + 1.0f, button.y + 1.0f, (button.w * 0.5f) - 2.0f, button.h - 2.0f};

    if (app->route.mode == ROUTE_MODE_CAR) {
        renderer_set_draw_color(&app->renderer, 60, 140, 220, 220);
        renderer_fill_rect(&app->renderer, &left);
        renderer_set_draw_color(&app->renderer, 50, 70, 90, 200);
        renderer_fill_rect(&app->renderer, &right);
    } else {
        renderer_set_draw_color(&app->renderer, 50, 70, 90, 200);
        renderer_fill_rect(&app->renderer, &left);
        renderer_set_draw_color(&app->renderer, 60, 200, 130, 220);
        renderer_fill_rect(&app->renderer, &right);
    }

    SDL_Color label_color = {225, 230, 240, 255};
    int text_h = ui_font_line_height(1.0f);
    if (text_h > 0) {
        int car_w = ui_measure_text_width("CAR", 1.0f);
        int walk_w = ui_measure_text_width("WALK", 1.0f);
        int car_x = (int)(left.x + (left.w - car_w) * 0.5f);
        int walk_x = (int)(right.x + (right.w - walk_w) * 0.5f);
        int label_y = (int)(button.y + (button.h - (float)text_h) * 0.5f);
        ui_draw_text(&app->renderer, car_x, label_y, "CAR", label_color, 1.0f);
        ui_draw_text(&app->renderer, walk_x, label_y, "WALK", label_color, 1.0f);
    }

    float cursor_x = button.x + button.w + 12.0f;
    char speed_text[32];
    snprintf(speed_text, sizeof(speed_text), "Speed: %.1fx", app->playback_speed);
    char distance_text[48];
    char zoom_text[24];
    float km = 0.0f;
    float minutes = 0.0f;
    if (app->route.path.count > 1) {
        km = app->route.path.total_length_m / 1000.0f;
        minutes = app->route.path.total_time_s / 60.0f;
    }
    snprintf(distance_text, sizeof(distance_text), "Route: %.2f km | %.1f min", km, minutes);
    snprintf(zoom_text, sizeof(zoom_text), "Zoom: %.2f", app->camera.zoom);

    SDL_Color badge_fill = {30, 35, 46, 230};
    SDL_Color badge_outline = {80, 90, 110, 220};
    float pad_x = 8.0f;
    float pad_y = 3.0f;
    int speed_w = ui_measure_text_width(speed_text, 1.0f);
    int distance_w = ui_measure_text_width(distance_text, 1.0f);
    int zoom_w = ui_measure_text_width(zoom_text, 1.0f);
    float box_h = (float)text_h + pad_y * 2.0f;
    float box_y = (APP_HEADER_HEIGHT - box_h) * 0.5f;
    float speed_box_w = (float)speed_w + pad_x * 2.0f;
    SDL_FRect speed_box = {cursor_x, box_y, speed_box_w, box_h};
    renderer_set_draw_color(&app->renderer, badge_fill.r, badge_fill.g, badge_fill.b, badge_fill.a);
    renderer_fill_rect(&app->renderer, &speed_box);
    renderer_set_draw_color(&app->renderer, badge_outline.r, badge_outline.g, badge_outline.b, badge_outline.a);
    renderer_draw_rect(&app->renderer, &speed_box);
    ui_draw_text(&app->renderer, (int)(speed_box.x + pad_x), (int)(speed_box.y + (box_h - text_h) * 0.5f), speed_text, label_color, 1.0f);
    cursor_x += speed_box_w + 10.0f;

    float distance_box_w = (float)distance_w + pad_x * 2.0f;
    SDL_FRect distance_box = {cursor_x, box_y, distance_box_w, box_h};
    renderer_set_draw_color(&app->renderer, badge_fill.r, badge_fill.g, badge_fill.b, badge_fill.a);
    renderer_fill_rect(&app->renderer, &distance_box);
    renderer_set_draw_color(&app->renderer, badge_outline.r, badge_outline.g, badge_outline.b, badge_outline.a);
    renderer_draw_rect(&app->renderer, &distance_box);
    ui_draw_text(&app->renderer, (int)(distance_box.x + pad_x), (int)(distance_box.y + (box_h - text_h) * 0.5f), distance_text, label_color, 1.0f);

    cursor_x += distance_box_w + 10.0f;
    float zoom_box_w = (float)zoom_w + pad_x * 2.0f;
    SDL_FRect zoom_box = {cursor_x, box_y, zoom_box_w, box_h};
    renderer_set_draw_color(&app->renderer, badge_fill.r, badge_fill.g, badge_fill.b, badge_fill.a);
    renderer_fill_rect(&app->renderer, &zoom_box);
    renderer_set_draw_color(&app->renderer, badge_outline.r, badge_outline.g, badge_outline.b, badge_outline.a);
    renderer_draw_rect(&app->renderer, &zoom_box);
    ui_draw_text(&app->renderer, (int)(zoom_box.x + pad_x), (int)(zoom_box.y + (box_h - text_h) * 0.5f), zoom_text, label_color, 1.0f);
    cursor_x += zoom_box_w + 10.0f;

    app_draw_header_layer_chips(app, cursor_x + 8.0f, box_y, box_h, text_h);
}

static const char *app_layer_label(TileLayerKind kind) {
    return layer_policy_label(kind);
}

static const char *app_layer_runtime_state_label(const AppState *app, TileLayerKind kind) {
    if (!app) {
        return "off";
    }
    return app_layer_active_runtime(app, kind) ? "on" : "off";
}

void app_draw_layer_debug(AppState *app) {
    if (!app || !app->overlay.enabled) {
        return;
    }

    SDL_Color color = {220, 230, 245, 255};
    int line_h = ui_font_line_height(1.0f);
    if (line_h <= 0) {
        return;
    }

    int x = 10;
    int y = (int)APP_HEADER_HEIGHT + 6;
    char line[128];

    snprintf(line, sizeof(line), "Visible tiles: %u", app->visible_tile_count);
    ui_draw_text(&app->renderer, x, y, line, color, 1.0f);
    y += line_h + 2;

    if (app->active_layer_valid) {
        snprintf(line, sizeof(line), "Active layer: %s", app_layer_label(app->active_layer_kind));
    } else {
        snprintf(line, sizeof(line), "Active layer: none");
    }
    ui_draw_text(&app->renderer, x, y, line, color, 1.0f);
    y += line_h + 4;

    snprintf(line, sizeof(line), "Load total: %u/%u no_data=%.1fs",
             app->loading_done, app->loading_expected, app->loading_no_data_time);
    ui_draw_text(&app->renderer, x, y, line, color, 1.0f);
    y += line_h + 4;

    snprintf(line, sizeof(line), "Bands vis c=%u/%u m=%u/%u f=%u/%u d=%u/%u q(c=%u m=%u f=%u d=%u) fallback=%u",
             app->band_visible_loaded[TILE_BAND_COARSE], app->band_visible_expected[TILE_BAND_COARSE],
             app->band_visible_loaded[TILE_BAND_MID], app->band_visible_expected[TILE_BAND_MID],
             app->band_visible_loaded[TILE_BAND_FINE], app->band_visible_expected[TILE_BAND_FINE],
             app->band_visible_loaded[TILE_BAND_DEFAULT], app->band_visible_expected[TILE_BAND_DEFAULT],
             app->band_queue_depth[TILE_BAND_COARSE], app->band_queue_depth[TILE_BAND_MID],
             app->band_queue_depth[TILE_BAND_FINE], app->band_queue_depth[TILE_BAND_DEFAULT],
             app->vk_road_band_fallback_draws);
    ui_draw_text(&app->renderer, x, y, line, color, 1.0f);
    y += line_h + 4;

    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy) {
            continue;
        }
        TileLayerKind kind = policy->kind;
        float start = app_layer_zoom_start(app, kind);
        snprintf(line, sizeof(line), "%s z>=%.2f band=%s exp %u done %u vis %u/%u in %u state=%s runtime=%s",
                 app_layer_label(kind),
                 start,
                 layer_policy_band_label(app->layer_target_band[kind]),
                 app->layer_expected[kind],
                 app->layer_done[kind],
                 app->layer_visible_loaded[kind],
                 app->layer_visible_expected[kind],
                 app->layer_inflight[kind],
                 layer_policy_readiness_label(app->layer_state[kind]),
                 app_layer_runtime_state_label(app, kind));
        ui_draw_text(&app->renderer, x, y, line, color, 1.0f);
        y += line_h + 2;
    }
}

void app_copy_overlay_text(AppState *app) {
    if (!app) {
        return;
    }

    char buffer[2048];
    size_t offset = 0;
    int written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "Region: %s\nZoom: %.2f\nVisible tiles: %u\nLoad total: %u/%u no_data=%.1fs\n"
                           "Bands vis c=%u/%u m=%u/%u f=%u/%u d=%u/%u q(c=%u m=%u f=%u d=%u) fallback=%u\n",
                           app->region.name,
                           app->camera.zoom,
                           app->visible_tile_count,
                           app->loading_done,
                           app->loading_expected,
                           app->loading_no_data_time,
                           app->band_visible_loaded[TILE_BAND_COARSE], app->band_visible_expected[TILE_BAND_COARSE],
                           app->band_visible_loaded[TILE_BAND_MID], app->band_visible_expected[TILE_BAND_MID],
                           app->band_visible_loaded[TILE_BAND_FINE], app->band_visible_expected[TILE_BAND_FINE],
                           app->band_visible_loaded[TILE_BAND_DEFAULT], app->band_visible_expected[TILE_BAND_DEFAULT],
                           app->band_queue_depth[TILE_BAND_COARSE], app->band_queue_depth[TILE_BAND_MID],
                           app->band_queue_depth[TILE_BAND_FINE], app->band_queue_depth[TILE_BAND_DEFAULT],
                           app->vk_road_band_fallback_draws);
    if (written < 0) {
        return;
    }
    offset += (size_t)written;

    if (app->active_layer_valid) {
        written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "Active layer: %s\n",
                           app_layer_label(app->active_layer_kind));
    } else {
        written = snprintf(buffer + offset, sizeof(buffer) - offset, "Active layer: none\n");
    }
    if (written < 0) {
        return;
    }
    offset += (size_t)written;

    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy) {
            continue;
        }
        TileLayerKind kind = policy->kind;
        float start = app_layer_zoom_start(app, kind);
        written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "%s z>=%.2f band=%s exp %u done %u vis %u/%u in %u state=%s runtime=%s\n",
                           app_layer_label(kind),
                           start,
                           layer_policy_band_label(app->layer_target_band[kind]),
                           app->layer_expected[kind],
                           app->layer_done[kind],
                           app->layer_visible_loaded[kind],
                           app->layer_visible_expected[kind],
                           app->layer_inflight[kind],
                           layer_policy_readiness_label(app->layer_state[kind]),
                           app_layer_runtime_state_label(app, kind));
        if (written < 0) {
            return;
        }
        offset += (size_t)written;
        if (offset >= sizeof(buffer)) {
            break;
        }
    }

    SDL_SetClipboardText(buffer);
}
