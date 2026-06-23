#include "app/app_internal.h"

enum {
    MAPFORGE_MIN_WINDOW_WIDTH = 320,
    MAPFORGE_MIN_WINDOW_HEIGHT = 240
};

static int app_runtime_clamp_window_extent(int value, int min_value) {
    if (value < min_value) {
        return min_value;
    }
    return value;
}

bool app_runtime_apply_window_size(AppState *app, int width, int height) {
    if (!app || width <= 0 || height <= 0) {
        return false;
    }

    int clamped_w = app_runtime_clamp_window_extent(width, MAPFORGE_MIN_WINDOW_WIDTH);
    int clamped_h = app_runtime_clamp_window_extent(height, MAPFORGE_MIN_WINDOW_HEIGHT);
    bool changed = app->width != clamped_w || app->height != clamped_h ||
                   app->renderer.width != clamped_w || app->renderer.height != clamped_h;

    app->width = clamped_w;
    app->height = clamped_h;
    if (!renderer_resize(&app->renderer, clamped_w, clamped_h)) {
        return false;
    }

    if (changed) {
        app->ui_state_bridge.hud_layer_debug_layout_dirty = true;
        app->ui_state_bridge.hud_route_panel_layout_dirty = true;
        app_tile_viewport_invalidate(app);
    }
    return true;
}
