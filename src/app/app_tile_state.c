#include "app/app_internal.h"

void app_tile_viewport_invalidate(AppState *app) {
    if (!app) {
        return;
    }
    app->tile_state_bridge.queue_valid = false;
    app->tile_state_bridge.visible_valid = false;
}
