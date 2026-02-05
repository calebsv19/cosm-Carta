#ifndef MAPFORGE_UI_DEBUG_OVERLAY_H
#define MAPFORGE_UI_DEBUG_OVERLAY_H

#include "render/renderer.h"

#include <stdbool.h>
#include <stdint.h>

// Tracks frame timing stats for the debug overlay.
typedef struct DebugOverlay {
    float dt;
    float fps;
    int frame_count;
    double accumulator;
    bool enabled;
    uint32_t visible_tiles;
    uint32_t cached_tiles;
    uint32_t cache_capacity;
} DebugOverlay;

// Initializes the debug overlay state.
void debug_overlay_init(DebugOverlay *overlay);

// Updates the overlay's timing statistics.
void debug_overlay_update(DebugOverlay *overlay, float dt);

// Renders the overlay contents (stub for Phase 0).
void debug_overlay_render(const DebugOverlay *overlay, Renderer *renderer);

#endif
