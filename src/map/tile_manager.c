#include "map/tile_manager.h"

#include "core/log.h"

#include <stdlib.h>
#include <string.h>

static bool tile_coord_equals(TileCoord a, TileCoord b) {
    return a.z == b.z && a.x == b.x && a.y == b.y;
}

static bool tile_coord_in_bounds(TileCoord coord,
                                 uint16_t zoom,
                                 TileCoord top_left,
                                 TileCoord bottom_right) {
    if (coord.z != zoom) {
        return false;
    }
    if (coord.x < top_left.x || coord.y < top_left.y) {
        return false;
    }
    if (coord.x > bottom_right.x || coord.y > bottom_right.y) {
        return false;
    }
    return true;
}

static void tile_entry_reset(TileEntry *entry) {
    if (!entry) {
        return;
    }

    if (entry->occupied) {
        mft_free_tile(&entry->tile);
    }

    memset(entry, 0, sizeof(*entry));
}

bool tile_manager_init(TileManager *manager, uint32_t capacity, const char *base_dir) {
    TileSourceConfig config;
    if (!base_dir) {
        return false;
    }
    tile_source_config_init(&config);
    if (!tile_source_config_set_filesystem(&config, base_dir)) {
        return false;
    }
    return tile_manager_init_with_source(manager, capacity, &config);
}

bool tile_manager_init_with_source(TileManager *manager, uint32_t capacity, const TileSourceConfig *source) {
    return tile_manager_init_with_source_for_layer(manager, capacity, source, TILE_LAYER_ROAD_ARTERY);
}

bool tile_manager_init_with_source_for_layer(TileManager *manager,
                                             uint32_t capacity,
                                             const TileSourceConfig *source,
                                             TileLayerKind kind) {
    if (!manager || capacity == 0 || !source || source->tiles_root[0] == '\0') {
        return false;
    }

    memset(manager, 0, sizeof(*manager));
    manager->entries = (TileEntry *)calloc(capacity, sizeof(TileEntry));
    if (!manager->entries) {
        return false;
    }

    manager->capacity = capacity;
    manager->count = 0;
    manager->tick = 1;
    manager->byte_limit = (kind <= TILE_LAYER_ROAD_LOCAL ? 64ull : 32ull) * 1024 * 1024;
    manager->source = *source;
    manager->layer_kind = kind;
    return true;
}

void tile_manager_shutdown(TileManager *manager) {
    if (!manager) {
        return;
    }

    for (uint32_t i = 0; i < manager->capacity; ++i) {
        tile_entry_reset(&manager->entries[i]);
    }

    free(manager->entries);
    memset(manager, 0, sizeof(*manager));
}

static TileEntry *tile_manager_find(TileManager *manager, TileCoord coord, TileZoomBand band) {
    if (!manager) {
        return NULL;
    }

    for (uint32_t i = 0; i < manager->capacity; ++i) {
        TileEntry *entry = &manager->entries[i];
        if (entry->occupied && entry->band == band && tile_coord_equals(entry->coord, coord)) {
            return entry;
        }
    }

    return NULL;
}

const MftTile *tile_manager_peek_tile(const TileManager *manager, TileCoord coord, TileZoomBand band) {
    if (!manager || !manager->entries) {
        return NULL;
    }

    for (uint32_t i = 0; i < manager->capacity; ++i) {
        const TileEntry *entry = &manager->entries[i];
        if (entry->occupied && entry->band == band && tile_coord_equals(entry->coord, coord)) {
            return &entry->tile;
        }
    }

    return NULL;
}

static TileEntry *tile_manager_pick_slot(TileManager *manager) {
    if (!manager) {
        return NULL;
    }

    TileEntry *empty = NULL;
    TileEntry *oldest = NULL;

    for (uint32_t i = 0; i < manager->capacity; ++i) {
        TileEntry *entry = &manager->entries[i];
        if (!entry->occupied) {
            empty = entry;
            break;
        }
        if (entry->presentation_pinned) continue;
        if (!oldest || entry->last_used < oldest->last_used) {
            oldest = entry;
        }
    }

    return empty ? empty : oldest;
}

static uint32_t tile_manager_entry_policy_score(const TileEntry *entry,
                                                const TileManagerTrimPolicy *policy) {
    if (!entry || !entry->occupied || !policy) {
        return 0u;
    }

    uint32_t score = 0u;
    if (policy->protect_visible_bounds &&
        tile_coord_in_bounds(entry->coord,
                             policy->visible_zoom,
                             policy->visible_top_left,
                             policy->visible_bottom_right)) {
        score += 1000u;
    }
    if (policy->protect_queue_bounds &&
        tile_coord_in_bounds(entry->coord,
                             policy->queue_zoom,
                             policy->queue_top_left,
                             policy->queue_bottom_right)) {
        score += 200u;
    }
    if (policy->prefer_band && entry->band == policy->preferred_band) {
        score += 100u;
    }
    return score;
}

static TileEntry *tile_manager_pick_trim_candidate(TileManager *manager,
                                                   const TileManagerTrimPolicy *policy) {
    if (!manager || !manager->entries || manager->count == 0u) {
        return NULL;
    }

    TileEntry *best = NULL;
    uint32_t best_score = 0u;
    for (uint32_t i = 0u; i < manager->capacity; ++i) {
        TileEntry *entry = &manager->entries[i];
        if (!entry->occupied || entry->presentation_pinned) {
            continue;
        }
        uint32_t score = tile_manager_entry_policy_score(entry, policy);
        if (!best ||
            score < best_score ||
            (score == best_score && entry->last_used < best->last_used)) {
            best = entry;
            best_score = score;
        }
    }
    return best;
}

const MftTile *tile_manager_get_tile(TileManager *manager, TileCoord coord, TileZoomBand band) {
    if (!manager) {
        return NULL;
    }

    TileEntry *entry = tile_manager_find(manager, coord, band);
    if (entry) {
        entry->last_used = manager->tick++;
        return &entry->tile;
    }

    char path[512];
    if (!tile_source_resolve_path(&manager->source, coord, manager->layer_kind, band, path, sizeof(path))) {
        return NULL;
    }

    MftTile tile;
    if (!mft_load_tile(path, &tile)) {
        log_error("Failed to load tile: %s", path);
        return NULL;
    }
    if (!tile_manager_put_tile(manager, coord, band, &tile)) {
        mft_free_tile(&tile);
        return NULL;
    }
    entry = tile_manager_find(manager, coord, band);
    return entry ? &entry->tile : NULL;
}

bool tile_manager_put_tile(TileManager *manager, TileCoord coord, TileZoomBand band, MftTile *tile) {
    if (!manager || !tile) {
        return false;
    }

    TileEntry *entry = tile_manager_find(manager, coord, band);
    if (entry) {
        return false;
    }

    TileEntry incoming_entry = {0};
    incoming_entry.occupied = true;
    incoming_entry.tile = *tile;
    TileManager incoming = {.entries = &incoming_entry, .capacity = 1u};
    uint64_t bytes = tile_manager_payload_bytes(&incoming) - sizeof(TileEntry);
    if (manager->byte_limit && bytes > manager->byte_limit) { manager->budget_refusals++; return false; }
    while (manager->byte_limit && tile_manager_payload_bytes(manager) > manager->byte_limit - bytes) {
        TileEntry *victim = tile_manager_pick_trim_candidate(manager, NULL);
        if (!victim) { manager->budget_refusals++; return false; }
        tile_entry_reset(victim);
        manager->count--;
    }
    entry = tile_manager_pick_slot(manager);
    if (!entry) {
        return false;
    }

    if (entry->occupied) {
        tile_entry_reset(entry);
        if (manager->count > 0) {
            manager->count -= 1;
        }
    }

    entry->coord = coord;
    entry->band = band;
    entry->tile = *tile;
    entry->occupied = true;
    entry->last_used = manager->tick++;
    manager->count += 1;
    memset(tile, 0, sizeof(*tile));
    return true;
}

uint32_t tile_manager_count(const TileManager *manager) {
    return manager ? manager->count : 0;
}

uint32_t tile_manager_capacity(const TileManager *manager) {
    return manager ? manager->capacity : 0;
}

bool tile_manager_ensure_capacity(TileManager *manager, uint32_t capacity) {
    if (!manager || capacity == 0) {
        return false;
    }
    if (capacity <= manager->capacity) {
        return true;
    }

    TileEntry *entries = (TileEntry *)calloc(capacity, sizeof(TileEntry));
    if (!entries) {
        return false;
    }

    for (uint32_t i = 0; i < manager->capacity; ++i) {
        entries[i] = manager->entries[i];
    }

    free(manager->entries);
    manager->entries = entries;
    manager->capacity = capacity;
    return true;
}

uint32_t tile_manager_trim_to_count(TileManager *manager,
                                    uint32_t target_count,
                                    const TileManagerTrimPolicy *policy) {
    if (!manager || !manager->entries || manager->count <= target_count) {
        return 0u;
    }

    uint32_t evicted = 0u;
    while (manager->count > target_count) {
        TileEntry *candidate = tile_manager_pick_trim_candidate(manager, policy);
        if (!candidate) {
            break;
        }
        tile_entry_reset(candidate);
        if (manager->count > 0u) {
            manager->count -= 1u;
        }
        evicted += 1u;
    }
    return evicted;
}

// Counts decoded allocations, including triangulation and water display variants.
uint64_t tile_manager_payload_bytes(const TileManager *manager) {
    if (!manager) return 0;
    uint64_t bytes = (uint64_t)manager->capacity * sizeof(TileEntry);
    for (uint32_t i = 0; i < manager->capacity; ++i) {
        if (!manager->entries[i].occupied) continue;
        const MftTile *t = &manager->entries[i].tile;
        bytes += (uint64_t)t->polyline_count * sizeof(MftPolyline) + (uint64_t)t->point_total * 4;
        bytes += (uint64_t)t->polygon_count * sizeof(MftPolygon) + (uint64_t)t->polygon_ring_total * 4 + (uint64_t)t->polygon_point_total * 4;
        if (t->polygon_tri_cached) bytes += (uint64_t)t->polygon_ring_total * 8 + (uint64_t)t->polygon_tri_index_total * 4;
        for (unsigned lod = 0; lod < 3; ++lod) if (t->water_variant_points[lod]) bytes += (uint64_t)t->water_variant_point_total[lod] * 4 + (uint64_t)t->polygon_ring_total * 4;
    }
    return bytes;
}
