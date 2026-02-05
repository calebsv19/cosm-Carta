#include "app/app.h"

#include "camera/camera.h"
#include "core/input.h"
#include "core/log.h"
#include "core/time.h"
#include "app/region.h"
#include "app/region_loader.h"
#include "map/road_renderer.h"
#include "map/tile_manager.h"
#include "map/tile_math.h"
#include "map/mercator.h"
#include "render/renderer.h"
#include "route/route.h"
#include "route/route_render.h"
#include "ui/debug_overlay.h"
#include "ui/font.h"

#include <SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>

static const float kHeaderHeight = 34.0f;

// Owns core application state for the main loop.
typedef struct AppState {
    SDL_Window *window;
    Renderer renderer;
    Camera camera;
    InputState input;
    DebugOverlay overlay;
    TileManager tile_manager;
    bool single_line;
    RegionInfo region;
    int region_index;
    RouteState route;
    bool dragging_start;
    bool dragging_goal;
    bool has_hover;
    uint32_t hover_node;
    bool playback_playing;
    float playback_time_s;
    float playback_speed;
    int width;
    int height;
} AppState;

static const uint16_t kRegionMinZoom = 12;
static const uint16_t kRegionMaxZoom = 12;

static uint16_t app_zoom_to_tile_level(float zoom) {
    int level = (int)floorf(zoom + 0.5f);
    if (level < (int)kRegionMinZoom) {
        level = (int)kRegionMinZoom;
    }
    if (level > (int)kRegionMaxZoom) {
        level = (int)kRegionMaxZoom;
    }
    return (uint16_t)level;
}

static float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float app_zoom_for_bounds(const Camera *camera, const RegionInfo *region, int screen_w, int screen_h, float padding) {
    if (!camera || !region || !region->has_bounds || screen_w <= 0 || screen_h <= 0) {
        return camera ? camera->zoom : 14.0f;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){region->min_lat, region->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){region->max_lat, region->max_lon});

    double width = max_m.x - min_m.x;
    double height = max_m.y - min_m.y;
    if (width <= 0.0 || height <= 0.0) {
        return camera->zoom;
    }

    double target_w = (double)screen_w / (width * (double)padding);
    double target_h = (double)screen_h / (height * (double)padding);
    double ppm = target_w < target_h ? target_w : target_h;
    if (ppm <= 0.0) {
        return camera->zoom;
    }

    double world_size = mercator_world_size_meters();
    double zoom = log2(ppm * world_size / 256.0);
    return clampf((float)zoom, 10.0f, 18.0f);
}

static void app_center_camera_on_region(Camera *camera, const RegionInfo *region, int screen_w, int screen_h) {
    if (!camera || !region || !region->has_bounds) {
        return;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){region->min_lat, region->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){region->max_lat, region->max_lon});

    camera->x = (float)((min_m.x + max_m.x) * 0.5);
    camera->y = (float)((min_m.y + max_m.y) * 0.5);
    camera->zoom = app_zoom_for_bounds(camera, region, screen_w, screen_h, 1.15f);
    camera->x_target = camera->x;
    camera->y_target = camera->y;
    camera->zoom_target = camera->zoom;
}

static void app_draw_region_bounds(AppState *app) {
    if (!app || !app->region.has_bounds) {
        return;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){app->region.min_lat, app->region.min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){app->region.max_lat, app->region.max_lon});

    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    camera_world_to_screen(&app->camera, (float)min_m.x, (float)max_m.y, app->width, app->height, &x0, &y0);
    camera_world_to_screen(&app->camera, (float)max_m.x, (float)min_m.y, app->width, app->height, &x1, &y1);

    float left = x0 < x1 ? x0 : x1;
    float right = x0 < x1 ? x1 : x0;
    float top = y0 < y1 ? y0 : y1;
    float bottom = y0 < y1 ? y1 : y0;

    SDL_SetRenderDrawColor(app->renderer.sdl, 80, 140, 220, 200);
    SDL_FRect rect = {left, top, right - left, bottom - top};
    SDL_RenderDrawRectF(app->renderer.sdl, &rect);
}

static uint32_t app_draw_visible_tiles(AppState *app) {
    if (!app) {
        return 0;
    }

    float ppm = camera_pixels_per_meter(&app->camera);
    float half_w_m = (float)app->width * 0.5f / ppm;
    float half_h_m = (float)app->height * 0.5f / ppm;

    float min_x = app->camera.x - half_w_m;
    float max_x = app->camera.x + half_w_m;
    float min_y = app->camera.y - half_h_m;
    float max_y = app->camera.y + half_h_m;

    uint16_t z = app_zoom_to_tile_level(app->camera.zoom);
    TileCoord top_left = tile_from_meters(z, (MercatorMeters){min_x, max_y});
    TileCoord bottom_right = tile_from_meters(z, (MercatorMeters){max_x, min_y});

    uint32_t count = tile_count(z);
    if (bottom_right.x >= count) {
        bottom_right.x = count - 1;
    }
    if (bottom_right.y >= count) {
        bottom_right.y = count - 1;
    }

    uint32_t visible = 0;
    for (uint32_t y = top_left.y; y <= bottom_right.y; ++y) {
        for (uint32_t x = top_left.x; x <= bottom_right.x; ++x) {
            TileCoord coord = {z, x, y};
            const MftTile *tile = tile_manager_get_tile(&app->tile_manager, coord);
            if (tile) {
                road_renderer_draw_tile(&app->renderer, &app->camera, tile, app->single_line);
                visible += 1;
            }
        }
    }

    return visible;
}

static bool app_load_route_graph(AppState *app) {
    if (!app) {
        return false;
    }

    char path[512];
    snprintf(path, sizeof(path), "data/regions/%s/graph/graph.bin", app->region.name);
    if (!route_state_load_graph(&app->route, path)) {
        log_error("Missing route graph for region: %s", app->region.name);
        return false;
    }
    return true;
}

static bool app_find_nearest_node(const RouteGraph *graph, float world_x, float world_y, uint32_t *out_node, double *out_dist) {
    if (!graph || graph->node_count == 0 || !out_node || !out_dist) {
        return false;
    }

    uint32_t best = 0;
    double best_dist = 0.0;
    for (uint32_t i = 0; i < graph->node_count; ++i) {
        double dx = graph->node_x[i] - world_x;
        double dy = graph->node_y[i] - world_y;
        double dist = dx * dx + dy * dy;
        if (i == 0 || dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }

    *out_node = best;
    *out_dist = best_dist;
    return true;
}

static bool app_is_near_node(const AppState *app, float world_x, float world_y, uint32_t *out_node) {
    if (!app || !app->route.loaded || app->route.graph.node_count == 0 || !out_node) {
        return false;
    }

    uint32_t node = 0;
    double dist = 0.0;
    if (!app_find_nearest_node(&app->route.graph, world_x, world_y, &node, &dist)) {
        return false;
    }

    float ppm = camera_pixels_per_meter(&app->camera);
    if (ppm <= 0.0f) {
        return false;
    }

    float snap_radius_m = 12.0f / ppm;
    if (dist > (double)(snap_radius_m * snap_radius_m)) {
        return false;
    }

    *out_node = node;
    return true;
}

static void app_update_hover(AppState *app) {
    if (!app || !app->route.loaded) {
        if (app) {
            app->has_hover = false;
        }
        return;
    }

    float world_x = 0.0f;
    float world_y = 0.0f;
    camera_screen_to_world(&app->camera, (float)app->input.mouse_x, (float)app->input.mouse_y, app->width, app->height, &world_x, &world_y);

    uint32_t node = 0;
    if (app_is_near_node(app, world_x, world_y, &node)) {
        app->hover_node = node;
        app->has_hover = true;
    } else {
        app->has_hover = false;
    }
}

static bool app_mouse_over_node(const AppState *app, uint32_t node, float radius) {
    if (!app || !app->route.loaded || node >= app->route.graph.node_count) {
        return false;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->camera,
                           (float)app->route.graph.node_x[node],
                           (float)app->route.graph.node_y[node],
                           app->width, app->height, &sx, &sy);
    float dx = (float)app->input.mouse_x - sx;
    float dy = (float)app->input.mouse_y - sy;
    return (dx * dx + dy * dy) <= radius * radius;
}

static void app_draw_hover_marker(AppState *app) {
    if (!app || !app->has_hover || !app->route.loaded || app->hover_node >= app->route.graph.node_count) {
        return;
    }

    if ((app->route.has_start && app->hover_node == app->route.start_node) ||
        (app->route.has_goal && app->hover_node == app->route.goal_node)) {
        return;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->camera,
                           (float)app->route.graph.node_x[app->hover_node],
                           (float)app->route.graph.node_y[app->hover_node],
                           app->width, app->height, &sx, &sy);
    SDL_SetRenderDrawColor(app->renderer.sdl, 80, 200, 255, 220);
    SDL_FRect rect = {sx - 5.0f, sy - 5.0f, 10.0f, 10.0f};
    SDL_RenderDrawRectF(app->renderer.sdl, &rect);
}

static SDL_FRect app_header_button_rect(const AppState *app) {
    float width = 140.0f;
    float height = 22.0f;
    float pad = 8.0f;
    SDL_FRect rect = {pad, (kHeaderHeight - height) * 0.5f, width, height};
    (void)app;
    return rect;
}

static bool app_header_button_hit(const AppState *app, int x, int y) {
    if (!app) {
        return false;
    }
    if (y < 0 || y > (int)kHeaderHeight) {
        return false;
    }
    SDL_FRect rect = app_header_button_rect(app);
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

static void app_draw_header_bar(AppState *app) {
    if (!app) {
        return;
    }

    SDL_SetRenderDrawColor(app->renderer.sdl, 18, 20, 28, 230);
    SDL_FRect bar = {0.0f, 0.0f, (float)app->width, kHeaderHeight};
    SDL_RenderFillRectF(app->renderer.sdl, &bar);

    SDL_FRect button = app_header_button_rect(app);
    SDL_SetRenderDrawColor(app->renderer.sdl, 70, 80, 100, 220);
    SDL_RenderDrawRectF(app->renderer.sdl, &button);

    SDL_FRect left = {button.x + 1.0f, button.y + 1.0f, (button.w * 0.5f) - 2.0f, button.h - 2.0f};
    SDL_FRect right = {button.x + button.w * 0.5f + 1.0f, button.y + 1.0f, (button.w * 0.5f) - 2.0f, button.h - 2.0f};

    if (app->route.mode == ROUTE_MODE_CAR) {
        SDL_SetRenderDrawColor(app->renderer.sdl, 60, 140, 220, 220);
        SDL_RenderFillRectF(app->renderer.sdl, &left);
        SDL_SetRenderDrawColor(app->renderer.sdl, 50, 70, 90, 200);
        SDL_RenderFillRectF(app->renderer.sdl, &right);
    } else {
        SDL_SetRenderDrawColor(app->renderer.sdl, 50, 70, 90, 200);
        SDL_RenderFillRectF(app->renderer.sdl, &left);
        SDL_SetRenderDrawColor(app->renderer.sdl, 60, 200, 130, 220);
        SDL_RenderFillRectF(app->renderer.sdl, &right);
    }

    SDL_Color label_color = {225, 230, 240, 255};
    int text_h = ui_font_line_height(1.0f);
    if (text_h > 0) {
        int car_w = ui_measure_text_width("CAR", 1.0f);
        int walk_w = ui_measure_text_width("WALK", 1.0f);
        int car_x = (int)(left.x + (left.w - car_w) * 0.5f);
        int walk_x = (int)(right.x + (right.w - walk_w) * 0.5f);
        int label_y = (int)(button.y + (button.h - (float)text_h) * 0.5f);
        ui_draw_text(app->renderer.sdl, car_x, label_y, "CAR", label_color, 1.0f);
        ui_draw_text(app->renderer.sdl, walk_x, label_y, "WALK", label_color, 1.0f);
    }

    float cursor_x = button.x + button.w + 12.0f;
    char speed_text[32];
    snprintf(speed_text, sizeof(speed_text), "Speed: %.1fx", app->playback_speed);
    char distance_text[48];
    float km = 0.0f;
    float minutes = 0.0f;
    if (app->route.path.count > 1) {
        km = app->route.path.total_length_m / 1000.0f;
        minutes = app->route.path.total_time_s / 60.0f;
    }
    snprintf(distance_text, sizeof(distance_text), "Route: %.2f km | %.1f min", km, minutes);

    SDL_Color badge_fill = {30, 35, 46, 230};
    SDL_Color badge_outline = {80, 90, 110, 220};
    float pad_x = 8.0f;
    float pad_y = 3.0f;
    int speed_w = ui_measure_text_width(speed_text, 1.0f);
    int distance_w = ui_measure_text_width(distance_text, 1.0f);
    float box_h = (float)text_h + pad_y * 2.0f;
    float box_y = (kHeaderHeight - box_h) * 0.5f;
    float speed_box_w = (float)speed_w + pad_x * 2.0f;
    SDL_FRect speed_box = {cursor_x, box_y, speed_box_w, box_h};
    SDL_SetRenderDrawColor(app->renderer.sdl, badge_fill.r, badge_fill.g, badge_fill.b, badge_fill.a);
    SDL_RenderFillRectF(app->renderer.sdl, &speed_box);
    SDL_SetRenderDrawColor(app->renderer.sdl, badge_outline.r, badge_outline.g, badge_outline.b, badge_outline.a);
    SDL_RenderDrawRectF(app->renderer.sdl, &speed_box);
    ui_draw_text(app->renderer.sdl, (int)(speed_box.x + pad_x), (int)(speed_box.y + (box_h - text_h) * 0.5f), speed_text, label_color, 1.0f);
    cursor_x += speed_box_w + 10.0f;

    float distance_box_w = (float)distance_w + pad_x * 2.0f;
    SDL_FRect distance_box = {cursor_x, box_y, distance_box_w, box_h};
    SDL_SetRenderDrawColor(app->renderer.sdl, badge_fill.r, badge_fill.g, badge_fill.b, badge_fill.a);
    SDL_RenderFillRectF(app->renderer.sdl, &distance_box);
    SDL_SetRenderDrawColor(app->renderer.sdl, badge_outline.r, badge_outline.g, badge_outline.b, badge_outline.a);
    SDL_RenderDrawRectF(app->renderer.sdl, &distance_box);
    ui_draw_text(app->renderer.sdl, (int)(distance_box.x + pad_x), (int)(distance_box.y + (box_h - text_h) * 0.5f), distance_text, label_color, 1.0f);
}

static void app_playback_reset(AppState *app) {
    if (!app) {
        return;
    }
    app->playback_time_s = 0.0f;
    app->playback_playing = false;
}

static void app_playback_update(AppState *app, float dt) {
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

static void app_draw_playback_marker(AppState *app) {
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
    SDL_SetRenderDrawColor(app->renderer.sdl, 255, 230, 80, 240);
    SDL_FRect rect = {sx - 4.0f, sy - 4.0f, 8.0f, 8.0f};
    SDL_RenderFillRectF(app->renderer.sdl, &rect);
}

static void app_draw_route_panel(AppState *app) {
    if (!app || app->route.path.count < 2) {
        return;
    }

    float panel_w = 220.0f;
    float panel_h = 36.0f;
    float pad = 10.0f;
    SDL_FRect panel = {pad, kHeaderHeight + pad, panel_w, panel_h};
    SDL_SetRenderDrawColor(app->renderer.sdl, 20, 26, 36, 210);
    SDL_RenderFillRectF(app->renderer.sdl, &panel);
    SDL_SetRenderDrawColor(app->renderer.sdl, 80, 90, 110, 220);
    SDL_RenderDrawRectF(app->renderer.sdl, &panel);

    if (app->route.path.total_time_s > 0.0f) {
        float progress = app->playback_time_s / app->route.path.total_time_s;
        if (progress < 0.0f) {
            progress = 0.0f;
        } else if (progress > 1.0f) {
            progress = 1.0f;
        }
        SDL_FRect bar = {panel.x + 8.0f, panel.y + panel.h - 10.0f, (panel.w - 16.0f) * progress, 4.0f};
        SDL_SetRenderDrawColor(app->renderer.sdl, 80, 170, 255, 220);
        SDL_RenderFillRectF(app->renderer.sdl, &bar);
    }
}

static bool app_recompute_route(AppState *app) {
    if (!app || !app->route.has_start || !app->route.has_goal) {
        return false;
    }

    bool ok = route_state_route(&app->route, app->route.start_node, app->route.goal_node);
    if (ok) {
        app_playback_reset(app);
    }
    return ok;
}

static float app_next_playback_speed(float current, int direction) {
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

static bool app_init(AppState *app) {
    if (!app) {
        return false;
    }

    app->width = 1280;
    app->height = 720;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    app->window = SDL_CreateWindow(
        "MapForge",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        app->width,
        app->height,
        SDL_WINDOW_SHOWN
    );

    if (!app->window) {
        log_error("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    if (!renderer_init(&app->renderer, app->window, app->width, app->height)) {
        log_error("renderer_init failed: %s", SDL_GetError());
        return false;
    }

    if (TTF_Init() != 0) {
        log_error("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    ui_font_set("assets/fonts/Montserrat-Regular.ttf", 10);

    app->region_index = 0;
    const RegionInfo *info = region_get(app->region_index);
    if (!info) {
        log_error("No region configured");
        return false;
    }

    app->region = *info;
    region_load_meta(info, &app->region);

    if (!tile_manager_init(&app->tile_manager, 256, app->region.tiles_dir)) {
        log_error("tile_manager_init failed");
        return false;
    }

    input_init(&app->input);
    camera_init(&app->camera);
    if (app->region.has_center) {
        MercatorMeters center = mercator_from_latlon((LatLon){app->region.center_lat, app->region.center_lon});
        app->camera.x = (float)center.x;
        app->camera.y = (float)center.y;
    }
    app_center_camera_on_region(&app->camera, &app->region, app->width, app->height);
    debug_overlay_init(&app->overlay);
    app->single_line = false;
    route_state_init(&app->route);
    app_load_route_graph(app);
    app->dragging_start = false;
    app->dragging_goal = false;
    app->has_hover = false;
    app->playback_playing = false;
    app->playback_time_s = 0.0f;
    app->playback_speed = 1.0f;

    return true;
}

static void app_shutdown(AppState *app) {
    if (!app) {
        return;
    }

    if (TTF_WasInit()) {
        ui_font_shutdown();
        TTF_Quit();
    }

    renderer_shutdown(&app->renderer);
    tile_manager_shutdown(&app->tile_manager);
    route_state_shutdown(&app->route);

    if (app->window) {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
    }

    SDL_Quit();
}

int app_run(void) {
    AppState app = {0};
    if (!app_init(&app)) {
        app_shutdown(&app);
        return 1;
    }

    double last_time = time_now_seconds();

    while (!app.input.quit) {
        input_begin_frame(&app.input);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input_handle_event(&app.input, &event);
        }

        if (app.input.toggle_debug_pressed) {
            app.overlay.enabled = !app.overlay.enabled;
        }
        if (app.input.toggle_single_line_pressed) {
            app.single_line = !app.single_line;
        }
        if (app.input.toggle_region_pressed) {
            app.region_index = (app.region_index + 1) % region_count();
            const RegionInfo *info = region_get(app.region_index);
            if (info) {
                app.region = *info;
                region_load_meta(info, &app.region);
                tile_manager_shutdown(&app.tile_manager);
                tile_manager_init(&app.tile_manager, 256, app.region.tiles_dir);
                app_load_route_graph(&app);
                route_state_clear(&app.route);
                app_playback_reset(&app);
                app_center_camera_on_region(&app.camera, &app.region, app.width, app.height);
            }
        }
        if (app.input.toggle_profile_pressed) {
            app.route.fastest = !app.route.fastest;
            if (app.route.has_start && app.route.has_goal) {
                app_recompute_route(&app);
            }
        }
        if (app.input.toggle_playback_pressed && app.route.path.count >= 2) {
            app.playback_playing = !app.playback_playing;
        }
        if (app.input.playback_step_forward && app.route.path.total_time_s > 0.0f) {
            app.playback_time_s += 5.0f;
            if (app.playback_time_s > app.route.path.total_time_s) {
                app.playback_time_s = app.route.path.total_time_s;
            }
        }
        if (app.input.playback_step_back && app.route.path.total_time_s > 0.0f) {
            app.playback_time_s -= 5.0f;
            if (app.playback_time_s < 0.0f) {
                app.playback_time_s = 0.0f;
            }
        }
        if (app.input.playback_speed_up) {
            app.playback_speed = app_next_playback_speed(app.playback_speed, 1);
        }
        if (app.input.playback_speed_down) {
            app.playback_speed = app_next_playback_speed(app.playback_speed, -1);
        }

        double now = time_now_seconds();
        float dt = (float)(now - last_time);
        last_time = now;

        if (!app.dragging_start && !app.dragging_goal && app.input.left_click_pressed) {
            bool over_start = app.route.has_start && app_mouse_over_node(&app, app.route.start_node, 7.0f);
            bool over_goal = app.route.has_goal && app_mouse_over_node(&app, app.route.goal_node, 7.0f);
            if (over_goal && !over_start) {
                app.dragging_goal = true;
            } else if (over_start) {
                app.dragging_start = true;
            }
        }
        if (!app.dragging_goal && app.input.right_click_pressed && app.route.has_goal &&
            app_mouse_over_node(&app, app.route.goal_node, 7.0f)) {
            app.dragging_goal = true;
        }

        bool over_start = app.route.has_start && app_mouse_over_node(&app, app.route.start_node, 7.0f);
        bool over_goal = app.route.has_goal && app_mouse_over_node(&app, app.route.goal_node, 7.0f);
        bool allow_mouse_pan = !(app.dragging_start || app.dragging_goal) &&
            !((app.input.mouse_buttons & SDL_BUTTON_LMASK) && (over_start || over_goal));
        camera_handle_input(&app.camera, &app.input, app.width, app.height, dt, allow_mouse_pan);
        camera_update(&app.camera, dt);
        debug_overlay_update(&app.overlay, dt);

        app_update_hover(&app);

        bool consumed_click = false;
        if (app.input.left_click_pressed && app_header_button_hit(&app, app.input.mouse_x, app.input.mouse_y)) {
            app.route.mode = (app.route.mode == ROUTE_MODE_CAR) ? ROUTE_MODE_WALK : ROUTE_MODE_CAR;
            if (app.route.has_start && app.route.has_goal) {
                app_recompute_route(&app);
            }
            consumed_click = true;
        } else if ((app.input.left_click_pressed || app.input.right_click_pressed || app.input.middle_click_pressed) &&
                   app.input.mouse_y <= (int)kHeaderHeight) {
            consumed_click = true;
        }

        if (!consumed_click && (app.input.left_click_pressed || app.input.right_click_pressed || app.input.middle_click_pressed)) {
            if (app.input.middle_click_pressed) {
                route_state_clear(&app.route);
                app_playback_reset(&app);
                app.dragging_start = false;
                app.dragging_goal = false;
            } else if (app.route.loaded) {
                float world_x = 0.0f;
                float world_y = 0.0f;
                camera_screen_to_world(&app.camera, (float)app.input.mouse_x, (float)app.input.mouse_y, app.width, app.height, &world_x, &world_y);
                if (app.input.left_click_pressed) {
                    if (app.has_hover && app.route.has_start && app.hover_node == app.route.start_node) {
                        app.dragging_start = true;
                    } else if (app.input.shift_down) {
                        uint32_t node = 0;
                        if (app_is_near_node(&app, world_x, world_y, &node)) {
                            app.route.start_node = node;
                            app.route.has_start = true;
                        }
                    }
                }
                if (app.input.right_click_pressed) {
                    if (app.has_hover && app.route.has_goal && app.hover_node == app.route.goal_node) {
                        app.dragging_goal = true;
                    } else {
                        uint32_t node = 0;
                        if (app_is_near_node(&app, world_x, world_y, &node)) {
                            app.route.goal_node = node;
                            app.route.has_goal = true;
                        }
                    }
                }
            }
        }
        if (app.input.enter_pressed && app.route.has_start && app.route.has_goal) {
            app_recompute_route(&app);
        }

        if (app.dragging_start || app.dragging_goal) {
            uint32_t node = 0;
            if (app.has_hover) {
                node = app.hover_node;
            }
            if (app.has_hover) {
                bool changed = false;
                if (app.dragging_start && node != app.route.start_node) {
                    app.route.start_node = node;
                    app.route.has_start = true;
                    changed = true;
                }
                if (app.dragging_goal && node != app.route.goal_node) {
                    app.route.goal_node = node;
                    app.route.has_goal = true;
                    changed = true;
                }
                if (changed && app.route.has_start && app.route.has_goal) {
                    app_recompute_route(&app);
                }
            }
        }

        if (app.input.left_click_released) {
            if (app.dragging_start) {
                app.dragging_start = false;
                if (app.route.has_start && app.route.has_goal) {
                    app_recompute_route(&app);
                }
            }
            if (app.dragging_goal) {
                app.dragging_goal = false;
                if (app.route.has_start && app.route.has_goal) {
                    app_recompute_route(&app);
                }
            }
        }
        if (app.input.right_click_released) {
            if (app.dragging_goal) {
                app.dragging_goal = false;
                if (app.route.has_start && app.route.has_goal) {
                    app_recompute_route(&app);
                }
            }
        }

        app_playback_update(&app, dt);

        renderer_begin_frame(&app.renderer);
        renderer_clear(&app.renderer, 20, 20, 28, 255);
        uint32_t visible_tiles = app_draw_visible_tiles(&app);
        app_draw_region_bounds(&app);
        route_render_draw(&app.renderer, &app.camera, &app.route.graph, &app.route.path, &app.route.drive_path, &app.route.walk_path,
            app.route.has_start, app.route.start_node,
            app.route.has_goal, app.route.goal_node,
            app.route.has_transfer, app.route.transfer_node);
        app_draw_hover_marker(&app);
        app_draw_playback_marker(&app);
        app_draw_route_panel(&app);
        app_draw_header_bar(&app);
        debug_overlay_render(&app.overlay, &app.renderer);
        renderer_end_frame(&app.renderer);

        app.overlay.visible_tiles = visible_tiles;
        app.overlay.cached_tiles = tile_manager_count(&app.tile_manager);
        app.overlay.cache_capacity = tile_manager_capacity(&app.tile_manager);

        if (app.overlay.enabled) {
            char title[128];
            const char *profile = app.route.fastest ? "fastest" : "shortest";
            const char *tier = road_renderer_zoom_tier_label(app.camera.zoom);
            const char *mode = app.route.mode == ROUTE_MODE_CAR ? "car" : "walk";
            if (app.route.path.count > 1) {
                float km = app.route.path.total_length_m / 1000.0f;
                float minutes = app.route.path.total_time_s / 60.0f;
                if (app.route.drive_path.count > 1 && app.route.walk_path.count > 1) {
                    float drive_minutes = app.route.drive_path.total_time_s / 60.0f;
                    float walk_minutes = app.route.walk_path.total_time_s / 60.0f;
                    snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | drive %.1f min + walk %.1f min | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                             app.region.name, mode, profile, tier, km, drive_minutes, walk_minutes, app.playback_speed, app.overlay.fps, app.overlay.visible_tiles, app.overlay.cached_tiles, app.overlay.cache_capacity);
                } else {
                    snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | %.1f min | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                             app.region.name, mode, profile, tier, km, minutes, app.playback_speed, app.overlay.fps, app.overlay.visible_tiles, app.overlay.cached_tiles, app.overlay.cache_capacity);
                }
            } else {
                const char *graph_status = app.route.loaded ? "graph ok" : "graph missing";
                snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %s | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                         app.region.name, mode, profile, tier, graph_status, app.playback_speed, app.overlay.fps, app.overlay.visible_tiles, app.overlay.cached_tiles, app.overlay.cache_capacity);
            }
            SDL_SetWindowTitle(app.window, title);
        } else {
            SDL_SetWindowTitle(app.window, "MapForge");
        }
    }

    app_shutdown(&app);
    return 0;
}
