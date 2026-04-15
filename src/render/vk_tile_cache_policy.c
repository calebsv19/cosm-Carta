#include "vk_tile_cache_policy.h"

static bool coord_equals(TileCoord a, TileCoord b) {
    return a.z == b.z && a.x == b.x && a.y == b.y;
}

static bool kind_has_floor(TileLayerKind kind) {
    return kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL;
}

static bool entry_is_protected(const VkTileCache *cache, TileLayerKind incoming_kind, const VkTileCacheEntry *entry) {
    if (!cache || !entry || !entry->occupied) {
        return false;
    }
    TileLayerKind resident_kind = entry->kind;
    if (!kind_has_floor(resident_kind)) {
        return false;
    }
    if (incoming_kind == resident_kind) {
        return false;
    }
    uint32_t resident = cache->resident_by_kind[resident_kind];
    uint32_t floor = cache->min_resident_by_kind[resident_kind];
    return resident <= floor;
}

VkTileCacheEntry *vk_tile_cache_find_entry(VkTileCache *cache,
                                           TileLayerKind kind,
                                           TileCoord coord,
                                           TileZoomBand band) {
    if (!cache || !cache->entries) {
        return NULL;
    }

    for (uint32_t i = 0; i < cache->capacity; ++i) {
        VkTileCacheEntry *entry = &cache->entries[i];
        if (!entry->occupied) {
            continue;
        }
        if (entry->kind == kind && entry->band == band && coord_equals(entry->coord, coord)) {
            return entry;
        }
    }
    return NULL;
}

VkTileCacheEntry *vk_tile_cache_pick_slot(VkTileCache *cache, TileLayerKind incoming_kind) {
    if (!cache || !cache->entries) {
        return NULL;
    }

    VkTileCacheEntry *empty = NULL;
    VkTileCacheEntry *oldest = NULL;
    VkTileCacheEntry *fallback_oldest = NULL;
    for (uint32_t i = 0; i < cache->capacity; ++i) {
        VkTileCacheEntry *entry = &cache->entries[i];
        if (!entry->occupied) {
            empty = entry;
            break;
        }
        if (!fallback_oldest || entry->last_used < fallback_oldest->last_used) {
            fallback_oldest = entry;
        }
        if (entry_is_protected(cache, incoming_kind, entry)) {
            continue;
        }
        if (!oldest || entry->last_used < oldest->last_used) {
            oldest = entry;
        }
    }
    if (empty) {
        return empty;
    }
    if (oldest) {
        return oldest;
    }
    return fallback_oldest;
}
