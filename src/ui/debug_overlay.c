#include "ui/debug_overlay.h"

void debug_overlay_init(DebugOverlay *overlay) {
    if (!overlay) {
        return;
    }

    overlay->dt = 0.0f;
    overlay->fps = 0.0f;
    overlay->frame_count = 0;
    overlay->accumulator = 0.0;
    overlay->enabled = true;
    overlay->visible_tiles = 0;
    overlay->cached_tiles = 0;
    overlay->cache_capacity = 0;
}

void debug_overlay_update(DebugOverlay *overlay, float dt) {
    if (!overlay) {
        return;
    }

    overlay->dt = dt;
    overlay->accumulator += dt;
    overlay->frame_count += 1;

    if (overlay->accumulator >= 1.0) {
        overlay->fps = (float)overlay->frame_count / (float)overlay->accumulator;
        overlay->accumulator = 0.0;
        overlay->frame_count = 0;
    }
}

void debug_overlay_render(const DebugOverlay *overlay, Renderer *renderer) {
    (void)overlay;
    (void)renderer;
}
