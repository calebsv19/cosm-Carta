#include "map/tile_manager.h"

#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool tile_coord_equals(TileCoord a, TileCoord b) {
    return a.z == b.z && a.x == b.x && a.y == b.y;
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

static TileEntry *tile_manager_find(TileManager *manager, TileCoord coord) {
    if (!manager) {
        return NULL;
    }

    for (uint32_t i = 0; i < manager->capacity; ++i) {
        TileEntry *entry = &manager->entries[i];
        if (entry->occupied && tile_coord_equals(entry->coord, coord)) {
            return entry;
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

const MftTile *tile_manager_get_tile(TileManager *manager, TileCoord coord) {
    if (!manager) {
        return NULL;
    }

    TileEntry *entry = tile_manager_find(manager, coord);
    if (entry) {
        entry->last_used = manager->tick++;
        return &entry->tile;
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

    char path[512];
    snprintf(path, sizeof(path), "%s/%u/%u/%u.mft", manager->base_dir, coord.z, coord.x, coord.y);

    MftTile tile;
    if (!mft_load_tile(path, &tile)) {
        return NULL;
    }

    entry->coord = coord;
    entry->tile = tile;
    entry->occupied = true;
    entry->last_used = manager->tick++;
    manager->count += 1;

    return &entry->tile;
}

uint32_t tile_manager_count(const TileManager *manager) {
    return manager ? manager->count : 0;
}

uint32_t tile_manager_capacity(const TileManager *manager) {
    return manager ? manager->capacity : 0;
}
