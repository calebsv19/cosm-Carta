#include "app/app_tile_pipeline_helpers.h"
#include <math.h>

uint32_t app_tile_queue_serviceable_count(const TileQueue *queue, uint32_t desired,
                                          const uint32_t caps[TILE_QUEUE_LANE_COUNT]) {
    if (!queue || !caps || !queue->items) return 0;
    uint32_t used[TILE_QUEUE_LANE_COUNT] = {0}, count = 0;
    for (uint32_t i = queue->index; i < queue->count && count < desired; ++i) {
        unsigned lane = queue->items[i].lane;
        if (lane >= TILE_QUEUE_LANE_COUNT) lane = TILE_QUEUE_LANE_L3_FAR_PREFETCH;
        if (used[lane] >= caps[lane]) break;
        used[lane]++;
        count++;
    }
    return count;
}

uint32_t app_tile_visible_missing_budget(const TileQueue *queue, uint32_t base, uint32_t visible_cap) {
    if (!queue || !queue->items) return base;
    uint32_t count = 0;
    for (uint32_t i = queue->index; i < queue->count && count < visible_cap; ++i) {
        if (queue->items[i].lane != TILE_QUEUE_LANE_L0_VISIBLE_MISSING) break;
        count++;
    }
    return count > base ? count : base;
}

uint32_t app_tile_submission_frame_scale(float frame_ms) {
    if (!isfinite(frame_ms) || frame_ms < 1000.0f / 30.0f) return 1;
    if (frame_ms >= 1000.0f / 15.0f) return 4;
    return (uint32_t)(frame_ms / (1000.0f / 60.0f));
}
