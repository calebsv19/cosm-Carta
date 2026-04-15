#include "app/app_internal.h"
#include "core/time.h"

#include <string.h>

static uint32_t app_tile_lifecycle_hash(TileLayerKind kind, TileCoord coord, TileZoomBand band) {
    uint32_t h = 2166136261u;
    h ^= (uint32_t)kind + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= (uint32_t)coord.z + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= coord.x + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= coord.y + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= (uint32_t)band + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

static bool app_tile_lifecycle_key_equals(const AppTileLifecycleEntry *entry,
                                          TileLayerKind kind,
                                          TileCoord coord,
                                          TileZoomBand band) {
    if (!entry || !entry->occupied) {
        return false;
    }
    return entry->kind == kind &&
           entry->band == band &&
           entry->coord.z == coord.z &&
           entry->coord.x == coord.x &&
           entry->coord.y == coord.y;
}

static AppTileLifecycleEntry *app_tile_lifecycle_find_slot(AppState *app,
                                                           TileLayerKind kind,
                                                           TileCoord coord,
                                                           TileZoomBand band,
                                                           bool create) {
    if (!app) {
        return NULL;
    }

    const uint32_t cap = APP_TILE_LIFECYCLE_CAPACITY;
    uint32_t index = app_tile_lifecycle_hash(kind, coord, band) & (cap - 1u);
    AppTileLifecycleEntry *fallback = NULL;
    for (uint32_t probe = 0u; probe < cap; ++probe) {
        AppTileLifecycleEntry *entry = &app->tile_state_bridge.lifecycle_entries[index];
        if (!entry->occupied) {
            if (!create) {
                return NULL;
            }
            if (!fallback) {
                fallback = entry;
            }
            break;
        }
        if (app_tile_lifecycle_key_equals(entry, kind, coord, band)) {
            return entry;
        }
        if (!fallback || entry->last_transition_frame < fallback->last_transition_frame) {
            fallback = entry;
        }
        index = (index + 1u) & (cap - 1u);
    }

    if (!create || !fallback) {
        return NULL;
    }

    memset(fallback, 0, sizeof(*fallback));
    fallback->occupied = true;
    fallback->coord = coord;
    fallback->kind = kind;
    fallback->band = band;
    fallback->state = APP_TILE_LIFECYCLE_ABSENT;
    return fallback;
}

static bool app_tile_lifecycle_transition_allowed(AppTileLifecycleState from, AppTileLifecycleState to) {
    if (from == to) {
        return true;
    }
    switch (from) {
        case APP_TILE_LIFECYCLE_ABSENT:
            return to == APP_TILE_LIFECYCLE_REQUESTED;
        case APP_TILE_LIFECYCLE_REQUESTED:
            return to == APP_TILE_LIFECYCLE_DECODED_CPU || to == APP_TILE_LIFECYCLE_STALE;
        case APP_TILE_LIFECYCLE_DECODED_CPU:
            return to == APP_TILE_LIFECYCLE_UPLOADED_GPU || to == APP_TILE_LIFECYCLE_RENDERABLE || to == APP_TILE_LIFECYCLE_STALE;
        case APP_TILE_LIFECYCLE_UPLOADED_GPU:
            return to == APP_TILE_LIFECYCLE_RENDERABLE || to == APP_TILE_LIFECYCLE_STALE;
        case APP_TILE_LIFECYCLE_RENDERABLE:
            return to == APP_TILE_LIFECYCLE_STALE || to == APP_TILE_LIFECYCLE_REQUESTED || to == APP_TILE_LIFECYCLE_UPLOADED_GPU;
        case APP_TILE_LIFECYCLE_STALE:
            return to == APP_TILE_LIFECYCLE_REQUESTED || to == APP_TILE_LIFECYCLE_RENDERABLE;
        default:
            break;
    }
    return false;
}

void app_tile_lifecycle_begin_frame(AppState *app) {
    if (!app) {
        return;
    }
    app->tile_state_bridge.lifecycle_frame_index += 1u;
    app->tile_state_bridge.lifecycle_transition_count = 0u;
    app->tile_state_bridge.lifecycle_invalid_transition_count = 0u;
    memset(app->tile_state_bridge.lifecycle_transition_to_state, 0, sizeof(app->tile_state_bridge.lifecycle_transition_to_state));
    app->tile_state_bridge.lifecycle_renderable_ideal_count = 0u;
    app->tile_state_bridge.lifecycle_renderable_fallback_count = 0u;
}

void app_tile_lifecycle_transition(AppState *app,
                                   TileLayerKind kind,
                                   TileCoord coord,
                                   TileZoomBand band,
                                   AppTileLifecycleState next_state,
                                   bool has_cpu,
                                   bool has_gpu,
                                   bool is_fallback_renderable,
                                   bool is_ideal_renderable) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT || (uint32_t)next_state >= APP_TILE_LIFECYCLE_STATE_COUNT) {
        return;
    }

    AppTileLifecycleEntry *entry = app_tile_lifecycle_find_slot(app, kind, coord, band, true);
    if (!entry) {
        return;
    }

    AppTileLifecycleState prev_state = entry->state;
    if (!app_tile_lifecycle_transition_allowed(prev_state, next_state)) {
        app->tile_state_bridge.lifecycle_invalid_transition_count += 1u;
        app->tile_state_bridge.lifecycle_invalid_transition_total += 1u;
        return;
    }

    bool state_changed = prev_state != next_state;
    entry->state = next_state;
    entry->has_cpu = has_cpu;
    entry->has_gpu = has_gpu;
    entry->is_fallback_renderable = is_fallback_renderable;
    entry->is_ideal_renderable = is_ideal_renderable;
    entry->last_transition_frame = app->tile_state_bridge.lifecycle_frame_index;
    entry->last_transition_time = time_now_seconds();

    if (state_changed) {
        app->tile_state_bridge.lifecycle_transition_count += 1u;
        app->tile_state_bridge.lifecycle_transition_to_state[next_state] += 1u;
    }
    if (next_state == APP_TILE_LIFECYCLE_STALE) {
        entry->visible_drop_pending = false;
    }
    if (next_state == APP_TILE_LIFECYCLE_RENDERABLE) {
        if (is_ideal_renderable) {
            app->tile_state_bridge.lifecycle_renderable_ideal_count += 1u;
        } else if (is_fallback_renderable) {
            app->tile_state_bridge.lifecycle_renderable_fallback_count += 1u;
        }
    }
}

void app_tile_lifecycle_mark_visible_drop(AppState *app,
                                          TileLayerKind kind,
                                          TileCoord coord,
                                          TileZoomBand band) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return;
    }
    AppTileLifecycleEntry *entry = app_tile_lifecycle_find_slot(app, kind, coord, band, true);
    if (!entry) {
        return;
    }
    entry->visible_drop_pending = true;
}

bool app_tile_lifecycle_consume_visible_drop_retry(AppState *app,
                                                   TileLayerKind kind,
                                                   TileCoord coord,
                                                   TileZoomBand band) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return false;
    }
    AppTileLifecycleEntry *entry = app_tile_lifecycle_find_slot(app, kind, coord, band, false);
    if (!entry || !entry->visible_drop_pending) {
        return false;
    }
    entry->visible_drop_pending = false;
    return true;
}

void app_tile_lifecycle_mark_stale_outside_queue(AppState *app,
                                                 TileCoord queue_top_left,
                                                 TileCoord queue_bottom_right,
                                                 uint16_t queue_zoom) {
    if (!app) {
        return;
    }
    for (uint32_t i = 0u; i < APP_TILE_LIFECYCLE_CAPACITY; ++i) {
        AppTileLifecycleEntry *entry = &app->tile_state_bridge.lifecycle_entries[i];
        if (!entry->occupied) {
            continue;
        }
        if (entry->state == APP_TILE_LIFECYCLE_STALE || entry->state == APP_TILE_LIFECYCLE_ABSENT) {
            continue;
        }
        bool outside = entry->coord.z != queue_zoom ||
                       entry->coord.x < queue_top_left.x ||
                       entry->coord.x > queue_bottom_right.x ||
                       entry->coord.y < queue_top_left.y ||
                       entry->coord.y > queue_bottom_right.y;
        if (!outside) {
            continue;
        }
        app_tile_lifecycle_transition(app,
                                      entry->kind,
                                      entry->coord,
                                      entry->band,
                                      APP_TILE_LIFECYCLE_STALE,
                                      entry->has_cpu,
                                      entry->has_gpu,
                                      false,
                                      false);
    }
}
