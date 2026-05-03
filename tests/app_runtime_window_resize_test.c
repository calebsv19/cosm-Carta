#include "app/app_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int resize_calls = 0;

bool renderer_resize(Renderer *renderer, int width, int height) {
    resize_calls += 1;
    if (!renderer || width <= 0 || height <= 0) {
        return false;
    }
    renderer->width = width;
    renderer->height = height;
    return true;
}

static void test_resize_updates_app_and_renderer(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    app.width = 1280;
    app.height = 720;
    app.renderer.width = 1280;
    app.renderer.height = 720;
    app.ui_state_bridge.hud_layer_debug_layout_dirty = false;
    app.ui_state_bridge.hud_route_panel_layout_dirty = false;
    app.tile_state_bridge.queue_valid = true;
    app.tile_state_bridge.visible_valid = true;

    assert(app_runtime_apply_window_size(&app, 1440, 900));
    assert(app.width == 1440);
    assert(app.height == 900);
    assert(app.renderer.width == 1440);
    assert(app.renderer.height == 900);
    assert(app.ui_state_bridge.hud_layer_debug_layout_dirty);
    assert(app.ui_state_bridge.hud_route_panel_layout_dirty);
    assert(!app.tile_state_bridge.queue_valid);
    assert(!app.tile_state_bridge.visible_valid);
}

static void test_resize_rejects_zero_extent(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    app.width = 1280;
    app.height = 720;
    app.renderer.width = 1280;
    app.renderer.height = 720;

    assert(!app_runtime_apply_window_size(&app, 0, 720));
    assert(app.width == 1280);
    assert(app.height == 720);
    assert(app.renderer.width == 1280);
    assert(app.renderer.height == 720);
}

static void test_resize_clamps_to_minimum(void) {
    AppState app;
    memset(&app, 0, sizeof(app));

    assert(app_runtime_apply_window_size(&app, 10, 20));
    assert(app.width == 320);
    assert(app.height == 240);
    assert(app.renderer.width == 320);
    assert(app.renderer.height == 240);
}

int main(void) {
    test_resize_updates_app_and_renderer();
    test_resize_rejects_zero_extent();
    test_resize_clamps_to_minimum();
    assert(resize_calls == 2);
    printf("app_runtime_window_resize_test: success\n");
    return 0;
}
