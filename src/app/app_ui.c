#include "app/app_internal.h"

#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"

#include <stdio.h>
#include <string.h>

static SDL_FRect app_header_button_rect(const AppState *app) {
    float width = 140.0f;
    float height = 22.0f;
    float pad = 8.0f;
    SDL_FRect rect = {pad, (APP_HEADER_HEIGHT - height) * 0.5f, width, height};
    (void)app;
    return rect;
}

static MapForgeThemePalette app_theme_palette(void) {
    MapForgeThemePalette palette = {
        .background_clear = {20, 20, 28, 255},
        .header_fill = {18, 20, 28, 230},
        .header_outline = {80, 90, 110, 220},
        .button_fill = {50, 70, 90, 200},
        .button_outline = {70, 80, 100, 220},
        .button_active_primary = {60, 140, 220, 220},
        .button_active_success = {60, 200, 130, 220},
        .badge_fill = {30, 35, 46, 230},
        .badge_outline = {80, 90, 110, 220},
        .text_primary = {225, 230, 240, 255},
        .text_muted = {205, 212, 225, 255},
        .progress_fill = {110, 180, 255, 230},
        .progress_bg = {42, 56, 76, 220},
        .chip_idle_fill = {34, 38, 48, 200},
        .chip_idle_outline = {58, 64, 80, 220},
        .chip_loading_fill = {30, 42, 58, 220},
        .chip_loading_outline = {92, 146, 210, 230},
        .chip_ready_fill = {28, 50, 44, 220},
        .chip_ready_outline = {78, 170, 130, 230},
        .overlay_fill = {16, 18, 24, 224},
        .overlay_outline = {80, 90, 110, 220},
        .overlay_accent = {110, 180, 255, 230},
        .route_panel_fill = {20, 26, 36, 210},
        .route_panel_outline = {80, 90, 110, 220},
        .route_progress_fill = {80, 170, 255, 220},
        .playback_marker_fill = {255, 230, 80, 240}
    };
    mapforge_shared_theme_resolve_palette(&palette);
    return palette;
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
        case TILE_LAYER_CONTOUR:
            return "CNT";
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

static float app_draw_header_layer_chips(AppState *app,
                                         const MapForgeThemePalette *palette,
                                         float left_limit,
                                         float box_y,
                                         float box_h,
                                         int text_h) {
    if (!app) {
        return 0.0f;
    }

    static const TileLayerKind kHeaderChipPriority[] = {
        TILE_LAYER_ROAD_ARTERY,
        TILE_LAYER_ROAD_LOCAL,
        TILE_LAYER_POLY_WATER,
        TILE_LAYER_POLY_PARK,
        TILE_LAYER_POLY_LANDUSE,
        TILE_LAYER_POLY_BUILDING
    };

    const float chip_w = 94.0f;
    const float chip_gap = 6.0f;
    const float right_pad = 8.0f;
    const float text_pad_x = 6.0f;
    const float toggle_w = 24.0f;
    const float progress_h = 3.0f;
    SDL_Color text_color = palette ? palette->text_primary : (SDL_Color){225, 230, 240, 255};

    memset(&app->ui_state_bridge.header_layer_row_rects, 0, sizeof(app->ui_state_bridge.header_layer_row_rects));
    memset(&app->ui_state_bridge.header_layer_label_rects, 0, sizeof(app->ui_state_bridge.header_layer_label_rects));
    memset(&app->ui_state_bridge.header_layer_toggle_rects, 0, sizeof(app->ui_state_bridge.header_layer_toggle_rects));

    float available_w = ((float)app->width - right_pad) - left_limit;
    if (available_w < chip_w) {
        return (float)app->width - right_pad;
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
        SDL_FRect row_rect = (SDL_FRect){chip_x, box_y, chip_w, box_h};
        SDL_FRect label_rect = (SDL_FRect){chip_x + 1.0f, box_y + 1.0f, chip_w - toggle_w - 2.0f, box_h - 2.0f};
        SDL_FRect toggle_rect = (SDL_FRect){chip_x + chip_w - toggle_w, box_y + 1.0f, toggle_w - 1.0f, box_h - 2.0f};
        app->ui_state_bridge.header_layer_row_rects[kind] = row_rect;
        app->ui_state_bridge.header_layer_label_rects[kind] = label_rect;
        app->ui_state_bridge.header_layer_toggle_rects[kind] = toggle_rect;

        uint32_t expected = layer_policy_requires_full_ready(kind)
            ? app->tile_state_bridge.layer_expected[kind]
            : app->tile_state_bridge.layer_visible_expected[kind];
        uint32_t done = layer_policy_requires_full_ready(kind)
            ? app->tile_state_bridge.layer_done[kind]
            : app->tile_state_bridge.layer_visible_loaded[kind];
        bool runtime_enabled = app_layer_runtime_enabled(app, kind);
        bool runtime_active = app_layer_active_runtime(app, kind);
        bool is_ready = app->tile_state_bridge.layer_state[kind] == LAYER_READINESS_READY;
        bool is_loading = app->tile_state_bridge.layer_state[kind] == LAYER_READINESS_LOADING && expected > 0;

        renderer_set_draw_color(&app->renderer,
                                palette->chip_idle_outline.r, palette->chip_idle_outline.g, palette->chip_idle_outline.b, palette->chip_idle_outline.a);
        renderer_draw_rect(&app->renderer, &row_rect);

        if (!runtime_enabled) {
            renderer_set_draw_color(&app->renderer,
                                    palette->chip_idle_fill.r, palette->chip_idle_fill.g, palette->chip_idle_fill.b, palette->chip_idle_fill.a);
            renderer_fill_rect(&app->renderer, &label_rect);
            renderer_set_draw_color(&app->renderer,
                                    palette->chip_idle_outline.r, palette->chip_idle_outline.g, palette->chip_idle_outline.b, palette->chip_idle_outline.a);
            renderer_draw_rect(&app->renderer, &label_rect);
        } else if (!runtime_active || app->tile_state_bridge.layer_state[kind] == LAYER_READINESS_HIDDEN) {
            renderer_set_draw_color(&app->renderer,
                                    palette->badge_fill.r, palette->badge_fill.g, palette->badge_fill.b, palette->badge_fill.a);
            renderer_fill_rect(&app->renderer, &label_rect);
            renderer_set_draw_color(&app->renderer,
                                    palette->badge_outline.r, palette->badge_outline.g, palette->badge_outline.b, palette->badge_outline.a);
            renderer_draw_rect(&app->renderer, &label_rect);
        } else if (is_loading) {
            renderer_set_draw_color(&app->renderer,
                                    palette->chip_loading_fill.r, palette->chip_loading_fill.g, palette->chip_loading_fill.b, palette->chip_loading_fill.a);
            renderer_fill_rect(&app->renderer, &label_rect);
            renderer_set_draw_color(&app->renderer,
                                    palette->chip_loading_outline.r, palette->chip_loading_outline.g,
                                    palette->chip_loading_outline.b, palette->chip_loading_outline.a);
            renderer_draw_rect(&app->renderer, &label_rect);
        } else if (is_ready) {
            renderer_set_draw_color(&app->renderer,
                                    palette->chip_ready_fill.r, palette->chip_ready_fill.g, palette->chip_ready_fill.b, palette->chip_ready_fill.a);
            renderer_fill_rect(&app->renderer, &label_rect);
            renderer_set_draw_color(&app->renderer,
                                    palette->chip_ready_outline.r, palette->chip_ready_outline.g,
                                    palette->chip_ready_outline.b, palette->chip_ready_outline.a);
            renderer_draw_rect(&app->renderer, &label_rect);
        } else {
            renderer_set_draw_color(&app->renderer,
                                    palette->badge_fill.r, palette->badge_fill.g, palette->badge_fill.b, palette->badge_fill.a);
            renderer_fill_rect(&app->renderer, &label_rect);
            renderer_set_draw_color(&app->renderer,
                                    palette->badge_outline.r, palette->badge_outline.g, palette->badge_outline.b, palette->badge_outline.a);
            renderer_draw_rect(&app->renderer, &label_rect);
        }

        char chip_text[24];
        const char *tag = app_header_layer_chip_label(kind);
        if (is_loading && runtime_enabled) {
            snprintf(chip_text, sizeof(chip_text), "%s %u/%u", tag, done, expected);
        } else {
            snprintf(chip_text, sizeof(chip_text), "%s", tag);
        }

        ui_draw_text(&app->renderer,
                     (int)(label_rect.x + text_pad_x),
                     (int)(box_y + (box_h - (float)text_h) * 0.5f),
                     chip_text,
                     text_color,
                     1.0f);

        if (is_loading && runtime_enabled && expected > 0) {
            float progress = app_clampf((float)done / (float)expected, 0.0f, 1.0f);
            SDL_FRect bar_bg = {label_rect.x + text_pad_x, box_y + box_h - 6.0f, label_rect.w - text_pad_x * 2.0f, progress_h};
            renderer_set_draw_color(&app->renderer,
                                    palette->progress_bg.r, palette->progress_bg.g, palette->progress_bg.b, palette->progress_bg.a);
            renderer_fill_rect(&app->renderer, &bar_bg);
            SDL_FRect bar_fg = {bar_bg.x, bar_bg.y, bar_bg.w * progress, bar_bg.h};
            renderer_set_draw_color(&app->renderer,
                                    palette->progress_fill.r, palette->progress_fill.g, palette->progress_fill.b, palette->progress_fill.a);
            renderer_fill_rect(&app->renderer, &bar_fg);
        }

        SDL_Color toggle_fill = runtime_enabled ? palette->button_active_success : palette->button_fill;
        SDL_Color toggle_text_color = palette->text_primary;
        renderer_set_draw_color(&app->renderer, toggle_fill.r, toggle_fill.g, toggle_fill.b, toggle_fill.a);
        renderer_fill_rect(&app->renderer, &toggle_rect);
        renderer_set_draw_color(&app->renderer,
                                palette->button_outline.r, palette->button_outline.g, palette->button_outline.b, palette->button_outline.a);
        renderer_draw_rect(&app->renderer, &toggle_rect);
        ui_draw_text(&app->renderer,
                     (int)(toggle_rect.x + 4.0f),
                     (int)(toggle_rect.y + (toggle_rect.h - (float)text_h) * 0.5f),
                     runtime_enabled ? "ON" : "OFF",
                     toggle_text_color,
                     1.0f);

        cursor_right = chip_x - chip_gap;
    }
    return cursor_right;
}

void app_draw_header_bar(AppState *app) {
    MapForgeThemePalette palette;
    if (!app) {
        return;
    }
    palette = app_theme_palette();

    renderer_set_draw_color(&app->renderer, palette.header_fill.r, palette.header_fill.g, palette.header_fill.b, palette.header_fill.a);
    SDL_FRect bar = {0.0f, 0.0f, (float)app->width, APP_HEADER_HEIGHT};
    renderer_fill_rect(&app->renderer, &bar);

    SDL_FRect button = app_header_button_rect(app);
    renderer_set_draw_color(&app->renderer, palette.button_outline.r, palette.button_outline.g, palette.button_outline.b, palette.button_outline.a);
    renderer_draw_rect(&app->renderer, &button);

    SDL_FRect left = {button.x + 1.0f, button.y + 1.0f, (button.w * 0.5f) - 2.0f, button.h - 2.0f};
    SDL_FRect right = {button.x + button.w * 0.5f + 1.0f, button.y + 1.0f, (button.w * 0.5f) - 2.0f, button.h - 2.0f};

    if (app->route_state_bridge.route.mode == ROUTE_MODE_CAR) {
        renderer_set_draw_color(&app->renderer,
                                palette.button_active_primary.r, palette.button_active_primary.g,
                                palette.button_active_primary.b, palette.button_active_primary.a);
        renderer_fill_rect(&app->renderer, &left);
        renderer_set_draw_color(&app->renderer, palette.button_fill.r, palette.button_fill.g, palette.button_fill.b, palette.button_fill.a);
        renderer_fill_rect(&app->renderer, &right);
    } else {
        renderer_set_draw_color(&app->renderer, palette.button_fill.r, palette.button_fill.g, palette.button_fill.b, palette.button_fill.a);
        renderer_fill_rect(&app->renderer, &left);
        renderer_set_draw_color(&app->renderer,
                                palette.button_active_success.r, palette.button_active_success.g,
                                palette.button_active_success.b, palette.button_active_success.a);
        renderer_fill_rect(&app->renderer, &right);
    }

    SDL_Color label_color = palette.text_primary;
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
    snprintf(speed_text, sizeof(speed_text), "Speed: %.1fx", app->route_state_bridge.playback_speed);
    char distance_text[48];
    char zoom_text[24];
    float km = 0.0f;
    float minutes = 0.0f;
    const RoutePath *primary_path = app_route_primary_path(app, NULL);
    if (primary_path && primary_path->count > 1) {
        km = primary_path->total_length_m / 1000.0f;
        minutes = primary_path->total_time_s / 60.0f;
    }
    if (app->route_state_bridge.route.mode == ROUTE_MODE_CAR &&
        app->route_state_bridge.route.drive_path.count > 1 &&
        app->route_state_bridge.route.walk_path.count > 1) {
        snprintf(distance_text, sizeof(distance_text), "Route: %.2f km | drive %.1f + walk %.1f min",
                 km,
                 app->route_state_bridge.route.drive_path.total_time_s / 60.0f,
                 app->route_state_bridge.route.walk_path.total_time_s / 60.0f);
    } else if (app->route_state_bridge.route.mode == ROUTE_MODE_WALK) {
        snprintf(distance_text, sizeof(distance_text), "Route: %.2f km | %.1f min walk", km, minutes);
    } else {
        snprintf(distance_text, sizeof(distance_text), "Route: %.2f km | %.1f min", km, minutes);
    }
    snprintf(zoom_text, sizeof(zoom_text), "Zoom: %.2f", app->view_state_bridge.camera.zoom);

    SDL_Color badge_fill = palette.badge_fill;
    SDL_Color badge_outline = palette.badge_outline;
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

    float chip_left_cursor = app_draw_header_layer_chips(app, &palette, cursor_x + 8.0f, box_y, box_h, text_h);
    float zoom_toggle_w = 74.0f;
    float zoom_toggle_gap = 6.0f;
    float zoom_toggle_x = chip_left_cursor - zoom_toggle_w - zoom_toggle_gap;
    if (zoom_toggle_x >= cursor_x + 8.0f) {
        app->ui_state_bridge.header_zoom_toggle_rect = (SDL_FRect){zoom_toggle_x, box_y, zoom_toggle_w, box_h};
        renderer_set_draw_color(&app->renderer,
                                palette.button_outline.r, palette.button_outline.g, palette.button_outline.b, palette.button_outline.a);
        renderer_draw_rect(&app->renderer, &app->ui_state_bridge.header_zoom_toggle_rect);
        renderer_set_draw_color(&app->renderer,
                                app->view_state_bridge.zoom_logic_enabled ? palette.button_active_primary.r : palette.button_fill.r,
                                app->view_state_bridge.zoom_logic_enabled ? palette.button_active_primary.g : palette.button_fill.g,
                                app->view_state_bridge.zoom_logic_enabled ? palette.button_active_primary.b : palette.button_fill.b,
                                app->view_state_bridge.zoom_logic_enabled ? palette.button_active_primary.a : palette.button_fill.a);
        renderer_fill_rect(&app->renderer, &(SDL_FRect){
            app->ui_state_bridge.header_zoom_toggle_rect.x + 1.0f,
            app->ui_state_bridge.header_zoom_toggle_rect.y + 1.0f,
            app->ui_state_bridge.header_zoom_toggle_rect.w - 2.0f,
            app->ui_state_bridge.header_zoom_toggle_rect.h - 2.0f
        });
        ui_draw_text(&app->renderer,
                     (int)(app->ui_state_bridge.header_zoom_toggle_rect.x + 7.0f),
                     (int)(app->ui_state_bridge.header_zoom_toggle_rect.y + (box_h - (float)text_h) * 0.5f),
                     app->view_state_bridge.zoom_logic_enabled ? "ZOOM ON" : "ZOOM OFF",
                     label_color,
                     1.0f);
    } else {
        memset(&app->ui_state_bridge.header_zoom_toggle_rect, 0, sizeof(app->ui_state_bridge.header_zoom_toggle_rect));
    }

    if (app->ui_state_bridge.header_layer_selected_valid &&
        app->ui_state_bridge.header_layer_selected_kind >= 0 &&
        app->ui_state_bridge.header_layer_selected_kind < TILE_LAYER_COUNT) {
        SDL_FRect anchor = app->ui_state_bridge.header_layer_row_rects[app->ui_state_bridge.header_layer_selected_kind];
        if (anchor.w > 0.0f && anchor.h > 0.0f) {
            float panel_w = 220.0f;
            float panel_h = (app->ui_state_bridge.header_layer_panel_mode == 2) ? 66.0f : 44.0f;
            float panel_x = anchor.x;
            float panel_y = anchor.y + anchor.h + 3.0f;
            if (panel_x + panel_w > (float)app->width - 8.0f) {
                panel_x = (float)app->width - panel_w - 8.0f;
            }
            if (panel_x < 8.0f) {
                panel_x = 8.0f;
            }
            renderer_set_draw_color(&app->renderer, palette.overlay_fill.r, palette.overlay_fill.g, palette.overlay_fill.b, 240);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){panel_x, panel_y, panel_w, panel_h});
            renderer_set_draw_color(&app->renderer, palette.overlay_outline.r, palette.overlay_outline.g, palette.overlay_outline.b, palette.overlay_outline.a);
            renderer_draw_rect(&app->renderer, &(SDL_FRect){panel_x, panel_y, panel_w, panel_h});

            if (app->ui_state_bridge.header_layer_panel_mode == 2) {
                app->ui_state_bridge.header_layer_fade_panel_rect = (SDL_FRect){panel_x, panel_y, panel_w, panel_h};
                app->ui_state_bridge.header_layer_fade_start_track_rect = (SDL_FRect){panel_x + 10.0f, panel_y + 24.0f, panel_w - 20.0f, 8.0f};
                app->ui_state_bridge.header_layer_fade_speed_track_rect = (SDL_FRect){panel_x + 10.0f, panel_y + 44.0f, panel_w - 20.0f, 8.0f};
                memset(&app->ui_state_bridge.header_layer_opacity_panel_rect, 0, sizeof(app->ui_state_bridge.header_layer_opacity_panel_rect));
                memset(&app->ui_state_bridge.header_layer_opacity_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_opacity_track_rect));

                uint16_t start_milli = app->view_state_bridge.layer_fade_start_milli[app->ui_state_bridge.header_layer_selected_kind];
                uint16_t speed_milli = app->view_state_bridge.layer_fade_speed_milli[app->ui_state_bridge.header_layer_selected_kind];

                renderer_set_draw_color(&app->renderer, palette.progress_bg.r, palette.progress_bg.g, palette.progress_bg.b, palette.progress_bg.a);
                renderer_fill_rect(&app->renderer, &app->ui_state_bridge.header_layer_fade_start_track_rect);
                renderer_fill_rect(&app->renderer, &app->ui_state_bridge.header_layer_fade_speed_track_rect);

                SDL_FRect start_fill = app->ui_state_bridge.header_layer_fade_start_track_rect;
                start_fill.w *= ((float)start_milli / 1000.0f);
                SDL_FRect speed_fill = app->ui_state_bridge.header_layer_fade_speed_track_rect;
                speed_fill.w *= ((float)speed_milli / 1000.0f);
                renderer_set_draw_color(&app->renderer, palette.progress_fill.r, palette.progress_fill.g, palette.progress_fill.b, palette.progress_fill.a);
                renderer_fill_rect(&app->renderer, &start_fill);
                renderer_fill_rect(&app->renderer, &speed_fill);

                SDL_FRect start_knob = {start_fill.x + start_fill.w - 3.0f, start_fill.y - 2.0f, 6.0f, start_fill.h + 4.0f};
                SDL_FRect speed_knob = {speed_fill.x + speed_fill.w - 3.0f, speed_fill.y - 2.0f, 6.0f, speed_fill.h + 4.0f};
                renderer_set_draw_color(&app->renderer, palette.text_primary.r, palette.text_primary.g, palette.text_primary.b, palette.text_primary.a);
                renderer_fill_rect(&app->renderer, &start_knob);
                renderer_fill_rect(&app->renderer, &speed_knob);

                char line0[64];
                char line1[64];
                char line2[64];
                float start_zoom = ((float)start_milli / 1000.0f) * 20.0f;
                float span_zoom = 0.15f + ((float)speed_milli / 1000.0f) * 6.0f;
                snprintf(line0, sizeof(line0), "%s fade controls", app_header_layer_chip_label(app->ui_state_bridge.header_layer_selected_kind));
                snprintf(line1, sizeof(line1), "Begin: %.2f", start_zoom);
                snprintf(line2, sizeof(line2), "Speed: %.2f", span_zoom);
                ui_draw_text(&app->renderer, (int)(panel_x + 10.0f), (int)(panel_y + 6.0f), line0, label_color, 1.0f);
                ui_draw_text(&app->renderer, (int)(panel_x + 150.0f), (int)(panel_y + 19.0f), line1, label_color, 1.0f);
                ui_draw_text(&app->renderer, (int)(panel_x + 150.0f), (int)(panel_y + 39.0f), line2, label_color, 1.0f);
            } else {
                app->ui_state_bridge.header_layer_opacity_panel_rect = (SDL_FRect){panel_x, panel_y, panel_w, panel_h};
                app->ui_state_bridge.header_layer_opacity_track_rect = (SDL_FRect){panel_x + 10.0f, panel_y + 24.0f, panel_w - 20.0f, 9.0f};
                memset(&app->ui_state_bridge.header_layer_fade_panel_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_panel_rect));
                memset(&app->ui_state_bridge.header_layer_fade_start_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_start_track_rect));
                memset(&app->ui_state_bridge.header_layer_fade_speed_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_speed_track_rect));

                renderer_set_draw_color(&app->renderer, palette.progress_bg.r, palette.progress_bg.g, palette.progress_bg.b, palette.progress_bg.a);
                renderer_fill_rect(&app->renderer, &app->ui_state_bridge.header_layer_opacity_track_rect);

                uint16_t milli = app->view_state_bridge.layer_opacity_milli[app->ui_state_bridge.header_layer_selected_kind];
                float fill_w = app->ui_state_bridge.header_layer_opacity_track_rect.w * ((float)milli / 1000.0f);
                SDL_FRect fill = app->ui_state_bridge.header_layer_opacity_track_rect;
                fill.w = fill_w;
                renderer_set_draw_color(&app->renderer, palette.progress_fill.r, palette.progress_fill.g, palette.progress_fill.b, palette.progress_fill.a);
                renderer_fill_rect(&app->renderer, &fill);

                float knob_x = app->ui_state_bridge.header_layer_opacity_track_rect.x + fill_w - 3.0f;
                if (knob_x < app->ui_state_bridge.header_layer_opacity_track_rect.x - 3.0f) {
                    knob_x = app->ui_state_bridge.header_layer_opacity_track_rect.x - 3.0f;
                }
                if (knob_x > app->ui_state_bridge.header_layer_opacity_track_rect.x + app->ui_state_bridge.header_layer_opacity_track_rect.w - 3.0f) {
                    knob_x = app->ui_state_bridge.header_layer_opacity_track_rect.x + app->ui_state_bridge.header_layer_opacity_track_rect.w - 3.0f;
                }
                SDL_FRect knob = {knob_x, app->ui_state_bridge.header_layer_opacity_track_rect.y - 2.0f, 6.0f, app->ui_state_bridge.header_layer_opacity_track_rect.h + 4.0f};
                renderer_set_draw_color(&app->renderer, palette.text_primary.r, palette.text_primary.g, palette.text_primary.b, palette.text_primary.a);
                renderer_fill_rect(&app->renderer, &knob);

                char line[64];
                snprintf(line, sizeof(line), "%s opacity: %u",
                         app_header_layer_chip_label(app->ui_state_bridge.header_layer_selected_kind),
                         (unsigned)milli);
                ui_draw_text(&app->renderer, (int)(panel_x + 10.0f), (int)(panel_y + 8.0f), line, label_color, 1.0f);
            }
        } else {
            memset(&app->ui_state_bridge.header_layer_opacity_panel_rect, 0, sizeof(app->ui_state_bridge.header_layer_opacity_panel_rect));
            memset(&app->ui_state_bridge.header_layer_opacity_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_opacity_track_rect));
            memset(&app->ui_state_bridge.header_layer_fade_panel_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_panel_rect));
            memset(&app->ui_state_bridge.header_layer_fade_start_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_start_track_rect));
            memset(&app->ui_state_bridge.header_layer_fade_speed_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_speed_track_rect));
        }
    } else {
        memset(&app->ui_state_bridge.header_layer_opacity_panel_rect, 0, sizeof(app->ui_state_bridge.header_layer_opacity_panel_rect));
        memset(&app->ui_state_bridge.header_layer_opacity_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_opacity_track_rect));
        memset(&app->ui_state_bridge.header_layer_fade_panel_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_panel_rect));
        memset(&app->ui_state_bridge.header_layer_fade_start_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_start_track_rect));
        memset(&app->ui_state_bridge.header_layer_fade_speed_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_speed_track_rect));
    }
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

static bool app_point_in_rect(int x, int y, const SDL_FRect *rect) {
    if (!rect) {
        return false;
    }
    if (rect->w <= 0.0f || rect->h <= 0.0f) {
        return false;
    }
    return (float)x >= rect->x &&
           (float)x <= rect->x + rect->w &&
           (float)y >= rect->y &&
           (float)y <= rect->y + rect->h;
}

bool app_header_layer_toggle_click(AppState *app, int x, int y) {
    if (!app) {
        return false;
    }
    if (y < 0 || y > (int)APP_HEADER_HEIGHT) {
        return false;
    }

    if (app_point_in_rect(x, y, &app->ui_state_bridge.header_zoom_toggle_rect)) {
        app->view_state_bridge.zoom_logic_enabled = !app->view_state_bridge.zoom_logic_enabled;
        app_refresh_layer_states(app);
        return true;
    }

    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy) {
            continue;
        }
        TileLayerKind kind = policy->kind;
        if (kind < 0 || kind >= TILE_LAYER_COUNT) {
            continue;
        }
        if (app_point_in_rect(x, y, &app->ui_state_bridge.header_layer_toggle_rects[kind])) {
            app->view_state_bridge.layer_user_enabled[kind] = !app->view_state_bridge.layer_user_enabled[kind];
            app_refresh_layer_states(app);
            return true;
        }
        if (app_point_in_rect(x, y, &app->ui_state_bridge.header_layer_label_rects[kind]) ||
            app_point_in_rect(x, y, &app->ui_state_bridge.header_layer_row_rects[kind])) {
            if (app->ui_state_bridge.input.shift_down) {
                app->ui_state_bridge.header_layer_selected_valid = true;
                app->ui_state_bridge.header_layer_selected_kind = kind;
                app->ui_state_bridge.header_layer_panel_mode = 1;
            } else if (app->ui_state_bridge.input.alt_down) {
                app->ui_state_bridge.header_layer_selected_valid = true;
                app->ui_state_bridge.header_layer_selected_kind = kind;
                app->ui_state_bridge.header_layer_panel_mode = 2;
            } else {
                app->view_state_bridge.layer_user_enabled[kind] = !app->view_state_bridge.layer_user_enabled[kind];
                app_refresh_layer_states(app);
                if (app->ui_state_bridge.header_layer_selected_valid && app->ui_state_bridge.header_layer_selected_kind == kind) {
                    app->ui_state_bridge.header_layer_selected_valid = false;
                    app->ui_state_bridge.header_layer_panel_mode = 0;
                }
            }
            return true;
        }
    }

    return false;
}

static void app_header_layer_slider_set_from_mouse(AppState *app, int mouse_x) {
    if (!app || !app->ui_state_bridge.header_layer_selected_valid) {
        return;
    }
    if (app->ui_state_bridge.header_layer_selected_kind < 0 || app->ui_state_bridge.header_layer_selected_kind >= TILE_LAYER_COUNT) {
        return;
    }
    const SDL_FRect *track = &app->ui_state_bridge.header_layer_opacity_track_rect;
    if (track->w <= 0.0f) {
        return;
    }
    float t = ((float)mouse_x - track->x) / track->w;
    t = app_clampf(t, 0.0f, 1.0f);
    app->view_state_bridge.layer_opacity_milli[app->ui_state_bridge.header_layer_selected_kind] = (uint16_t)(t * 1000.0f + 0.5f);
}

static void app_header_layer_fade_set_from_mouse(AppState *app, int mouse_x, int target) {
    if (!app || !app->ui_state_bridge.header_layer_selected_valid) {
        return;
    }
    if (app->ui_state_bridge.header_layer_selected_kind < 0 || app->ui_state_bridge.header_layer_selected_kind >= TILE_LAYER_COUNT) {
        return;
    }
    const SDL_FRect *track = (target == 1)
        ? &app->ui_state_bridge.header_layer_fade_start_track_rect
        : &app->ui_state_bridge.header_layer_fade_speed_track_rect;
    if (track->w <= 0.0f) {
        return;
    }
    float t = ((float)mouse_x - track->x) / track->w;
    t = app_clampf(t, 0.0f, 1.0f);
    uint16_t value = (uint16_t)(t * 1000.0f + 0.5f);
    if (target == 1) {
        app->view_state_bridge.layer_fade_start_milli[app->ui_state_bridge.header_layer_selected_kind] = value;
    } else {
        if (value < 1u) {
            value = 1u;
        }
        app->view_state_bridge.layer_fade_speed_milli[app->ui_state_bridge.header_layer_selected_kind] = value;
    }
    app_refresh_layer_states(app);
}

bool app_header_layer_slider_update(AppState *app) {
    if (!app || !app->ui_state_bridge.header_layer_selected_valid) {
        return false;
    }
    int mx = app->ui_state_bridge.input.mouse_x;
    int my = app->ui_state_bridge.input.mouse_y;
    bool on_opacity_panel = app_point_in_rect(mx, my, &app->ui_state_bridge.header_layer_opacity_panel_rect);
    bool on_fade_panel = app_point_in_rect(mx, my, &app->ui_state_bridge.header_layer_fade_panel_rect);
    bool on_panel = on_opacity_panel || on_fade_panel;
    bool on_selected_row = false;
    if (app->ui_state_bridge.header_layer_selected_kind >= 0 && app->ui_state_bridge.header_layer_selected_kind < TILE_LAYER_COUNT) {
        on_selected_row = app_point_in_rect(mx, my, &app->ui_state_bridge.header_layer_row_rects[app->ui_state_bridge.header_layer_selected_kind]);
    }

    if (app->ui_state_bridge.input.left_click_pressed && on_panel) {
        if (app->ui_state_bridge.header_layer_panel_mode == 2) {
            if (app_point_in_rect(mx, my, &app->ui_state_bridge.header_layer_fade_start_track_rect)) {
                app->ui_state_bridge.header_layer_fade_drag_target = 1;
            } else if (app_point_in_rect(mx, my, &app->ui_state_bridge.header_layer_fade_speed_track_rect)) {
                app->ui_state_bridge.header_layer_fade_drag_target = 2;
            } else {
                app->ui_state_bridge.header_layer_fade_drag_target = 0;
            }
            if (app->ui_state_bridge.header_layer_fade_drag_target != 0) {
                app_header_layer_fade_set_from_mouse(app, mx, app->ui_state_bridge.header_layer_fade_drag_target);
            }
        } else {
            app->ui_state_bridge.header_layer_opacity_dragging = true;
            app_header_layer_slider_set_from_mouse(app, mx);
        }
        return true;
    }
    if (app->ui_state_bridge.header_layer_opacity_dragging) {
        if (app->ui_state_bridge.input.mouse_buttons & SDL_BUTTON_LMASK) {
            app_header_layer_slider_set_from_mouse(app, mx);
            return true;
        }
        app->ui_state_bridge.header_layer_opacity_dragging = false;
    }
    if (app->ui_state_bridge.header_layer_fade_drag_target != 0) {
        if (app->ui_state_bridge.input.mouse_buttons & SDL_BUTTON_LMASK) {
            app_header_layer_fade_set_from_mouse(app, mx, app->ui_state_bridge.header_layer_fade_drag_target);
            return true;
        }
        app->ui_state_bridge.header_layer_fade_drag_target = 0;
    }
    if (app->ui_state_bridge.input.left_click_pressed && !on_panel && !on_selected_row &&
        !app_point_in_rect(mx, my, &app->ui_state_bridge.header_zoom_toggle_rect)) {
        app->ui_state_bridge.header_layer_selected_valid = false;
        app->ui_state_bridge.header_layer_panel_mode = 0;
        return false;
    }
    return false;
}

static int app_layer_debug_line_count(void) {
    return 6 + (int)layer_policy_count();
}

static int app_digits_u32(uint32_t value) {
    int digits = 1;
    while (value >= 10u) {
        value /= 10u;
        digits += 1;
    }
    return digits;
}

static uint64_t app_hash_mix_u64(uint64_t seed, uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    return seed;
}

static uint64_t app_layer_debug_layout_hash(const AppState *app) {
    if (!app) {
        return 0ull;
    }

    uint64_t hash = 1469598103934665603ull;
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->width);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->height);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->tile_state_bridge.visible_tile_count);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->tile_state_bridge.active_layer_kind);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->tile_state_bridge.active_layer_valid);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.loading_done));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.loading_expected));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->route_state_bridge.route_snap_debug_cells));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->route_state_bridge.route_snap_debug_segments));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->route_state_bridge.route_snap_debug_hits));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.draw_path_vk_count));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.draw_path_fallback_count));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.transition_blend_draw_count));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.presenter_invariant_fail_count));

    for (size_t i = 0; i < TILE_BAND_COUNT; ++i) {
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.band_visible_loaded[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.band_visible_expected[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.band_queue_depth[i]));
    }
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.layer_expected[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.layer_done[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.layer_visible_loaded[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.layer_visible_expected[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.layer_inflight[i]));
    }
    return hash;
}

static bool app_layer_debug_format_line(const AppState *app, int index, char *line, size_t line_size) {
    if (!app || !line || line_size == 0u) {
        return false;
    }
    if (index == 0) {
        snprintf(line, line_size, "Visible tiles: %u", app->tile_state_bridge.visible_tile_count);
        return true;
    }
    if (index == 1) {
        if (app->tile_state_bridge.active_layer_valid) {
            snprintf(line, line_size, "Active layer: %s", app_layer_label(app->tile_state_bridge.active_layer_kind));
        } else {
            snprintf(line, line_size, "Active layer: none");
        }
        return true;
    }
    if (index == 2) {
        snprintf(line, line_size, "Load total: %u/%u no_data=%.1fs",
                 app->tile_state_bridge.loading_done, app->tile_state_bridge.loading_expected, app->tile_state_bridge.loading_no_data_time);
        return true;
    }
    if (index == 3) {
        snprintf(line, line_size, "Bands vis c=%u/%u m=%u/%u f=%u/%u d=%u/%u q(c=%u m=%u f=%u d=%u) fallback=%u",
                 app->tile_state_bridge.band_visible_loaded[TILE_BAND_COARSE], app->tile_state_bridge.band_visible_expected[TILE_BAND_COARSE],
                 app->tile_state_bridge.band_visible_loaded[TILE_BAND_MID], app->tile_state_bridge.band_visible_expected[TILE_BAND_MID],
                 app->tile_state_bridge.band_visible_loaded[TILE_BAND_FINE], app->tile_state_bridge.band_visible_expected[TILE_BAND_FINE],
                 app->tile_state_bridge.band_visible_loaded[TILE_BAND_DEFAULT], app->tile_state_bridge.band_visible_expected[TILE_BAND_DEFAULT],
                 app->tile_state_bridge.band_queue_depth[TILE_BAND_COARSE], app->tile_state_bridge.band_queue_depth[TILE_BAND_MID],
                 app->tile_state_bridge.band_queue_depth[TILE_BAND_FINE], app->tile_state_bridge.band_queue_depth[TILE_BAND_DEFAULT],
                 app->tile_state_bridge.vk_road_band_fallback_draws);
        return true;
    }
    if (index == 4) {
        snprintf(line, line_size, "Route snap cells=%u seg=%u hits=%u q=%.2fms",
                 app->route_state_bridge.route_snap_debug_cells,
                 app->route_state_bridge.route_snap_debug_segments,
                 app->route_state_bridge.route_snap_debug_hits,
                 app->route_state_bridge.route_snap_debug_query_ms);
        return true;
    }
    if (index == 5) {
        snprintf(line, line_size, "Draw vk=%u fallback=%u blend=%u hold %u/%u upd=%u inv_fail=%u",
                 app->tile_state_bridge.draw_path_vk_count,
                 app->tile_state_bridge.draw_path_fallback_count,
                 app->tile_state_bridge.transition_blend_draw_count,
                 app->tile_state_bridge.present_hold_hits,
                 app->tile_state_bridge.present_hold_misses,
                 app->tile_state_bridge.present_hold_updates,
                 app->tile_state_bridge.presenter_invariant_fail_count);
        return true;
    }

    int policy_index = index - 6;
    if (policy_index < 0 || (size_t)policy_index >= layer_policy_count()) {
        return false;
    }
    const LayerPolicy *policy = layer_policy_at((size_t)policy_index);
    if (!policy) {
        return false;
    }
    TileLayerKind kind = policy->kind;
    float start = app_layer_zoom_start(app, kind);
    snprintf(line, line_size, "%s z>=%.2f band=%s exp %u done %u vis %u/%u in %u state=%s runtime=%s",
             app_layer_label(kind),
             start,
             layer_policy_band_label(app->tile_state_bridge.layer_target_band[kind]),
             app->tile_state_bridge.layer_expected[kind],
             app->tile_state_bridge.layer_done[kind],
             app->tile_state_bridge.layer_visible_loaded[kind],
             app->tile_state_bridge.layer_visible_expected[kind],
             app->tile_state_bridge.layer_inflight[kind],
             layer_policy_readiness_label(app->tile_state_bridge.layer_state[kind]),
             app_layer_runtime_state_label(app, kind));
    return true;
}

void app_draw_layer_debug(AppState *app) {
    if (!app || !app->ui_state_bridge.overlay.enabled) {
        if (app) {
            memset(&app->ui_state_bridge.hud_layer_debug_panel_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_panel_rect));
            memset(&app->ui_state_bridge.hud_layer_debug_collapse_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_collapse_rect));
            memset(&app->ui_state_bridge.hud_layer_debug_handle_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_handle_rect));
            app->ui_state_bridge.hud_layer_debug_layout_dirty = true;
        }
        return;
    }

    MapForgeThemePalette palette = app_theme_palette();
    SDL_Color color = palette.text_primary;
    int line_h = ui_font_line_height(1.0f);
    if (line_h <= 0) {
        return;
    }

    int x = 10;
    int y = (int)APP_HEADER_HEIGHT + 6;
    int total_lines = app_layer_debug_line_count();
    char line[256];
    uint64_t layout_hash = app_layer_debug_layout_hash(app);
    if (app->ui_state_bridge.hud_layer_debug_layout_dirty ||
        app->ui_state_bridge.hud_layer_debug_layout_hash != layout_hash ||
        app->ui_state_bridge.hud_layer_debug_cached_line_count != total_lines) {
        int max_line_w = 0;
        for (int i = 0; i < total_lines; ++i) {
            if (!app_layer_debug_format_line(app, i, line, sizeof(line))) {
                continue;
            }
            int width = ui_measure_text_width(line, 1.0f);
            if (width > max_line_w) {
                max_line_w = width;
            }
        }
        app->ui_state_bridge.hud_layer_debug_cached_max_text_w = max_line_w;
        app->ui_state_bridge.hud_layer_debug_cached_w = (float)(max_line_w + 22 + 18);
        app->ui_state_bridge.hud_layer_debug_cached_h = (float)(total_lines * (line_h + 2) + 14);
        app->ui_state_bridge.hud_layer_debug_cached_line_count = total_lines;
        app->ui_state_bridge.hud_layer_debug_layout_hash = layout_hash;
        app->ui_state_bridge.hud_layer_debug_layout_dirty = false;
    }

    float panel_w = app->ui_state_bridge.hud_layer_debug_cached_w;
    float panel_w_max = (float)(app->width - 20);
    if (panel_w < 220.0f) {
        panel_w = 220.0f;
    }
    if (panel_w > panel_w_max) {
        panel_w = panel_w_max;
    }
    float panel_h = app->ui_state_bridge.hud_layer_debug_cached_h;
    SDL_FRect panel = {(float)(x - 6), (float)(y - 4), panel_w, panel_h};
    app->ui_state_bridge.hud_layer_debug_panel_rect = panel;

    if (app->ui_state_bridge.hud_layer_debug_collapsed) {
        SDL_FRect handle = {6.0f, APP_HEADER_HEIGHT + 8.0f, 20.0f, 20.0f};
        app->ui_state_bridge.hud_layer_debug_handle_rect = handle;
        memset(&app->ui_state_bridge.hud_layer_debug_collapse_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_collapse_rect));
        renderer_set_draw_color(&app->renderer, palette.overlay_fill.r, palette.overlay_fill.g, palette.overlay_fill.b, palette.overlay_fill.a);
        renderer_fill_rect(&app->renderer, &handle);
        renderer_set_draw_color(&app->renderer, palette.overlay_outline.r, palette.overlay_outline.g, palette.overlay_outline.b, palette.overlay_outline.a);
        renderer_draw_rect(&app->renderer, &handle);
        ui_draw_text(&app->renderer, (int)handle.x + 6, (int)handle.y + 4, ">", color, 1.0f);
        return;
    }

    memset(&app->ui_state_bridge.hud_layer_debug_handle_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_handle_rect));
    SDL_FRect collapse = {panel.x + panel.w - 18.0f, panel.y + 3.0f, 14.0f, 14.0f};
    app->ui_state_bridge.hud_layer_debug_collapse_rect = collapse;

    renderer_set_draw_color(&app->renderer, palette.overlay_fill.r, palette.overlay_fill.g, palette.overlay_fill.b, palette.overlay_fill.a);
    renderer_fill_rect(&app->renderer, &panel);
    renderer_set_draw_color(&app->renderer, palette.overlay_outline.r, palette.overlay_outline.g, palette.overlay_outline.b, palette.overlay_outline.a);
    renderer_draw_rect(&app->renderer, &panel);
    SDL_FRect accent = {panel.x, panel.y, 3.0f, panel.h};
    renderer_set_draw_color(&app->renderer, palette.overlay_accent.r, palette.overlay_accent.g, palette.overlay_accent.b, palette.overlay_accent.a);
    renderer_fill_rect(&app->renderer, &accent);

    renderer_set_draw_color(&app->renderer, palette.overlay_fill.r, palette.overlay_fill.g, palette.overlay_fill.b, 245);
    renderer_fill_rect(&app->renderer, &collapse);
    renderer_set_draw_color(&app->renderer, palette.overlay_outline.r, palette.overlay_outline.g, palette.overlay_outline.b, palette.overlay_outline.a);
    renderer_draw_rect(&app->renderer, &collapse);
    ui_draw_text(&app->renderer, (int)collapse.x + 4, (int)collapse.y + 1, "-", color, 1.0f);

    int max_text_w = (int)(panel.w - 16.0f - collapse.w - 4.0f);
    for (int i = 0; i < total_lines; ++i) {
        if (!app_layer_debug_format_line(app, i, line, sizeof(line))) {
            continue;
        }
        ui_draw_text_clipped(&app->renderer, x, y, line, color, 1.0f, max_text_w);
        y += line_h + ((i < 4) ? 4 : 2);
    }
}

bool app_handle_hud_clicks(AppState *app) {
    if (!app) {
        return false;
    }
    bool any_click = app->ui_state_bridge.input.left_click_pressed || app->ui_state_bridge.input.right_click_pressed || app->ui_state_bridge.input.middle_click_pressed;
    if (!any_click) {
        return false;
    }

    if (app_route_panel_handle_click(app)) {
        return true;
    }
    if (!app->ui_state_bridge.overlay.enabled) {
        return false;
    }

    int mx = app->ui_state_bridge.input.mouse_x;
    int my = app->ui_state_bridge.input.mouse_y;

    if (app->ui_state_bridge.hud_layer_debug_collapsed) {
        if (app->ui_state_bridge.input.left_click_pressed && app_point_in_rect(mx, my, &app->ui_state_bridge.hud_layer_debug_handle_rect)) {
            app->ui_state_bridge.hud_layer_debug_collapsed = false;
            return true;
        }
        return app_point_in_rect(mx, my, &app->ui_state_bridge.hud_layer_debug_handle_rect);
    }

    if (app->ui_state_bridge.input.left_click_pressed && app_point_in_rect(mx, my, &app->ui_state_bridge.hud_layer_debug_collapse_rect)) {
        app->ui_state_bridge.hud_layer_debug_collapsed = true;
        return true;
    }
    return app_point_in_rect(mx, my, &app->ui_state_bridge.hud_layer_debug_panel_rect);
}

void app_copy_overlay_text(AppState *app) {
    if (!app) {
        return;
    }

    char buffer[2048];
    size_t offset = 0;
    int written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "Region: %s\nZoom: %.2f\nVisible tiles: %u\nLoad total: %u/%u no_data=%.1fs\n"
                           "Bands vis c=%u/%u m=%u/%u f=%u/%u d=%u/%u q(c=%u m=%u f=%u d=%u) fallback=%u\n"
                           "Draw vk=%u fallback=%u blend=%u hold %u/%u upd=%u inv_fail=%u\n"
                           "Hardening invariants=%s contour=%s\n",
                           app->region.name,
                           app->view_state_bridge.camera.zoom,
                           app->tile_state_bridge.visible_tile_count,
                           app->tile_state_bridge.loading_done,
                           app->tile_state_bridge.loading_expected,
                           app->tile_state_bridge.loading_no_data_time,
                           app->tile_state_bridge.band_visible_loaded[TILE_BAND_COARSE], app->tile_state_bridge.band_visible_expected[TILE_BAND_COARSE],
                           app->tile_state_bridge.band_visible_loaded[TILE_BAND_MID], app->tile_state_bridge.band_visible_expected[TILE_BAND_MID],
                           app->tile_state_bridge.band_visible_loaded[TILE_BAND_FINE], app->tile_state_bridge.band_visible_expected[TILE_BAND_FINE],
                           app->tile_state_bridge.band_visible_loaded[TILE_BAND_DEFAULT], app->tile_state_bridge.band_visible_expected[TILE_BAND_DEFAULT],
                           app->tile_state_bridge.band_queue_depth[TILE_BAND_COARSE], app->tile_state_bridge.band_queue_depth[TILE_BAND_MID],
                           app->tile_state_bridge.band_queue_depth[TILE_BAND_FINE], app->tile_state_bridge.band_queue_depth[TILE_BAND_DEFAULT],
                           app->tile_state_bridge.vk_road_band_fallback_draws,
                           app->tile_state_bridge.draw_path_vk_count,
                           app->tile_state_bridge.draw_path_fallback_count,
                           app->tile_state_bridge.transition_blend_draw_count,
                           app->tile_state_bridge.present_hold_hits,
                           app->tile_state_bridge.present_hold_misses,
                           app->tile_state_bridge.present_hold_updates,
                           app->tile_state_bridge.presenter_invariant_fail_count,
                           app->tile_state_bridge.presenter_invariants_enabled ? "on" : "off",
                           app->tile_state_bridge.contour_runtime_enabled ? "on" : "off");
    if (written < 0) {
        return;
    }
    offset += (size_t)written;

    if (app->tile_state_bridge.active_layer_valid) {
        written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "Active layer: %s\n",
                           app_layer_label(app->tile_state_bridge.active_layer_kind));
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
                           layer_policy_band_label(app->tile_state_bridge.layer_target_band[kind]),
                           app->tile_state_bridge.layer_expected[kind],
                           app->tile_state_bridge.layer_done[kind],
                           app->tile_state_bridge.layer_visible_loaded[kind],
                           app->tile_state_bridge.layer_visible_expected[kind],
                           app->tile_state_bridge.layer_inflight[kind],
                           layer_policy_readiness_label(app->tile_state_bridge.layer_state[kind]),
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
