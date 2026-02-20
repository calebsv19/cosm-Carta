#include "map/tile_manager.h"

#include "core/log.h"
#include "core_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool tile_coord_equals(TileCoord a, TileCoord b) {
    return a.z == b.z && a.x == b.x && a.y == b.y;
}

static const char *tile_band_dir(TileZoomBand band) {
    switch (band) {
        case TILE_BAND_COARSE:
            return "coarse";
        case TILE_BAND_MID:
            return "mid";
        case TILE_BAND_FINE:
            return "fine";
        case TILE_BAND_DEFAULT:
        default:
            return NULL;
    }
}

static bool tile_resolve_path(const char *base_dir, TileCoord coord, TileZoomBand band, char *path, size_t path_size) {
    if (!base_dir || !path || path_size == 0u) {
        return false;
    }

    const char *band_dir = tile_band_dir(band);
    if (band_dir) {
        char band_path[512];
        snprintf(band_path, sizeof(band_path), "%s/bands/%s/%u/%u/%u.mft",
                 base_dir, band_dir, coord.z, coord.x, coord.y);
        if (core_io_path_exists(band_path)) {
            snprintf(path, path_size, "%s", band_path);
            return true;
        }
    }

    snprintf(path, path_size, "%s/%u/%u/%u.mft", base_dir, coord.z, coord.x, coord.y);
    if (!core_io_path_exists(path)) {
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
    if (!manager || capacity == 0 || !base_dir) {
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
    snprintf(manager->base_dir, sizeof(manager->base_dir), "%s", base_dir);
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
        if (!oldest || entry->last_used < oldest->last_used) {
            oldest = entry;
        }
    }

    return empty ? empty : oldest;
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
    if (!tile_resolve_path(manager->base_dir, coord, band, path, sizeof(path))) {
        return NULL;
    }

    entry = tile_manager_pick_slot(manager);
    if (!entry) {
        return NULL;
    }

    if (entry->occupied) {
        tile_entry_reset(entry);
        if (manager->count > 0) {
            manager->count -= 1;
        }
    }

    MftTile tile;
    if (!mft_load_tile(path, &tile)) {
        log_error("Failed to load tile: %s", path);
        return NULL;
    }

    entry->coord = coord;
    entry->band = band;
    entry->tile = tile;
    entry->occupied = true;
    entry->last_used = manager->tick++;
    manager->count += 1;

    return &entry->tile;
}

bool tile_manager_put_tile(TileManager *manager, TileCoord coord, TileZoomBand band, MftTile *tile) {
    if (!manager || !tile) {
        return false;
    }

    TileEntry *entry = tile_manager_find(manager, coord, band);
    if (entry) {
        mft_free_tile(tile);
        return false;
    }

    entry = tile_manager_pick_slot(manager);
    if (!entry) {
        mft_free_tile(tile);
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
