#include "tile_source_test_fixture.h"

#include "core_io.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(MAPFORGE_HAVE_SQLITE)
#include <sqlite3.h>
#endif

static const MapForgeTileSourceLayerCase k_layer_cases[] = {
    {TILE_LAYER_ROAD_ARTERY, "road_artery", 0x11},
    {TILE_LAYER_ROAD_LOCAL, "road_local", 0x22},
    {TILE_LAYER_CONTOUR, "contour", 0x33},
    {TILE_LAYER_POLY_WATER, "water", 0x44},
    {TILE_LAYER_POLY_PARK, "park", 0x55},
    {TILE_LAYER_POLY_LANDUSE, "landuse", 0x66},
    {TILE_LAYER_POLY_BUILDING, "building", 0x77}
};

static bool fixture_ensure_dir_recursive(const char *path) {
    char tmp[512];
    size_t len = 0u;
    if (!path || path[0] == '\0') {
        return false;
    }
    len = strlen(path);
    if (len >= sizeof(tmp)) {
        return false;
    }
    snprintf(tmp, sizeof(tmp), "%s", path);
    if (tmp[len - 1u] == '/') {
        tmp[len - 1u] = '\0';
    }
    for (char *p = tmp + 1; *p != '\0'; ++p) {
        if (*p == '/') {
            struct stat st = {0};
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode))) {
                return false;
            }
            *p = '/';
        }
    }
    {
        struct stat st = {0};
        if (mkdir(tmp, 0755) != 0 && (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode))) {
            return false;
        }
    }
    return true;
}

static bool fixture_remove_tree(const char *path) {
    struct stat st = {0};
    if (!path || path[0] == '\0') {
        return false;
    }
    if (lstat(path, &st) != 0) {
        return true;
    }
    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *entry = NULL;
        if (!dir) {
            return false;
        }
        while ((entry = readdir(dir)) != NULL) {
            char child[512];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
            if (!fixture_remove_tree(child)) {
                closedir(dir);
                return false;
            }
        }
        closedir(dir);
        return rmdir(path) == 0;
    }
    return unlink(path) == 0;
}

static const char *fixture_layer_suffix(TileLayerKind kind) {
    switch (kind) {
        case TILE_LAYER_ROAD_ARTERY:
            return "artery.mft";
        case TILE_LAYER_ROAD_LOCAL:
            return "local.mft";
        case TILE_LAYER_CONTOUR:
            return "contour.mft";
        case TILE_LAYER_POLY_WATER:
            return "water.mft";
        case TILE_LAYER_POLY_PARK:
            return "park.mft";
        case TILE_LAYER_POLY_LANDUSE:
            return "landuse.mft";
        case TILE_LAYER_POLY_BUILDING:
            return "building.mft";
        default:
            return "mft";
    }
}

static size_t fixture_build_minimal_tile_blob(TileCoord coord, uint8_t marker, uint8_t *out, size_t cap) {
    const char magic[4] = {'M', 'F', 'T', '1'};
    uint16_t version = 1u;
    uint16_t z = coord.z;
    uint32_t x = coord.x;
    uint32_t y = coord.y;
    uint32_t polyline_count = 0u;
    const size_t needed = 4u + 2u + 2u + 4u + 4u + 4u + 1u;
    size_t pos = 0u;
    if (!out || cap < needed) {
        return 0u;
    }
    memcpy(out + pos, magic, sizeof(magic));
    pos += sizeof(magic);
    memcpy(out + pos, &version, sizeof(version));
    pos += sizeof(version);
    memcpy(out + pos, &z, sizeof(z));
    pos += sizeof(z);
    memcpy(out + pos, &x, sizeof(x));
    pos += sizeof(x);
    memcpy(out + pos, &y, sizeof(y));
    pos += sizeof(y);
    memcpy(out + pos, &polyline_count, sizeof(polyline_count));
    pos += sizeof(polyline_count);
    out[pos++] = marker;
    return pos;
}

const MapForgeTileSourceLayerCase *mapforge_tile_source_fixture_layer_cases(size_t *out_count) {
    if (out_count) {
        *out_count = sizeof(k_layer_cases) / sizeof(k_layer_cases[0]);
    }
    return k_layer_cases;
}

bool mapforge_tile_source_fixture_init(MapForgeTileSourceFixture *fixture) {
    char tmp_root[] = "/tmp/mapforge_tile_source_fixture.XXXXXX";
    const char *previous_cache_dir = NULL;
    if (!fixture) {
        return false;
    }
    memset(fixture, 0, sizeof(*fixture));
    previous_cache_dir = getenv("MAPFORGE_TILE_ARCHIVE_CACHE_DIR");
    if (previous_cache_dir && previous_cache_dir[0] != '\0') {
        snprintf(fixture->previous_cache_dir, sizeof(fixture->previous_cache_dir), "%s", previous_cache_dir);
        fixture->had_previous_cache_dir = true;
    }
    if (!mkdtemp(tmp_root)) {
        return false;
    }
    snprintf(fixture->root, sizeof(fixture->root), "%s", tmp_root);
    snprintf(fixture->cache_dir, sizeof(fixture->cache_dir), "%s/cache", fixture->root);
    snprintf(fixture->archive_path, sizeof(fixture->archive_path), "%s/tiles.mbtiles", fixture->root);
    snprintf(fixture->missing_tiles_root, sizeof(fixture->missing_tiles_root), "%s/no_tree_tiles", fixture->root);
    snprintf(fixture->fallback_tiles_root, sizeof(fixture->fallback_tiles_root), "%s/fallback_tiles", fixture->root);
    snprintf(fixture->missing_archive_path, sizeof(fixture->missing_archive_path), "%s/missing.mbtiles", fixture->root);
    fixture->coord = (TileCoord){12u, 655u, 1582u};
    if (!fixture_ensure_dir_recursive(fixture->cache_dir)) {
        mapforge_tile_source_fixture_cleanup(fixture);
        return false;
    }
    if (setenv("MAPFORGE_TILE_ARCHIVE_CACHE_DIR", fixture->cache_dir, 1) != 0) {
        mapforge_tile_source_fixture_cleanup(fixture);
        return false;
    }
    return true;
}

void mapforge_tile_source_fixture_cleanup(MapForgeTileSourceFixture *fixture) {
    if (!fixture || fixture->root[0] == '\0') {
        return;
    }
    if (fixture->had_previous_cache_dir) {
        (void)setenv("MAPFORGE_TILE_ARCHIVE_CACHE_DIR", fixture->previous_cache_dir, 1);
    } else {
        (void)unsetenv("MAPFORGE_TILE_ARCHIVE_CACHE_DIR");
    }
    (void)fixture_remove_tree(fixture->root);
    memset(fixture, 0, sizeof(*fixture));
}

bool mapforge_tile_source_fixture_seed_archive(MapForgeTileSourceFixture *fixture) {
#if defined(MAPFORGE_HAVE_SQLITE)
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    if (!fixture) {
        return false;
    }
    if (sqlite3_open_v2(fixture->archive_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK || !db) {
        if (db) {
            sqlite3_close(db);
        }
        return false;
    }
    if (sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS mapforge_tiles ("
                     "z INTEGER NOT NULL, x INTEGER NOT NULL, y INTEGER NOT NULL, "
                     "layer TEXT NOT NULL, band TEXT NOT NULL, tile_data BLOB NOT NULL, "
                     "PRIMARY KEY (z, x, y, layer, band));",
                     NULL,
                     NULL,
                     NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }
    if (sqlite3_prepare_v2(db,
                           "INSERT OR REPLACE INTO mapforge_tiles "
                           "(z, x, y, layer, band, tile_data) VALUES (?1, ?2, ?3, ?4, ?5, ?6);",
                           -1,
                           &stmt,
                           NULL) != SQLITE_OK || !stmt) {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        sqlite3_close(db);
        return false;
    }

    for (size_t i = 0u; i < sizeof(k_layer_cases) / sizeof(k_layer_cases[0]); ++i) {
        uint8_t blob[64];
        size_t blob_size = fixture_build_minimal_tile_blob(fixture->coord, k_layer_cases[i].marker, blob, sizeof(blob));
        int rc = 0;
        if (blob_size == 0u) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }
        sqlite3_bind_int(stmt, 1, (int)fixture->coord.z);
        sqlite3_bind_int(stmt, 2, (int)fixture->coord.x);
        sqlite3_bind_int(stmt, 3, (int)fixture->coord.y);
        sqlite3_bind_text(stmt, 4, k_layer_cases[i].layer_key, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, "default", -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 6, blob, (int)blob_size, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return true;
#else
    (void)fixture;
    return false;
#endif
}

bool mapforge_tile_source_fixture_write_tree_tile(const char *tiles_root,
                                                  TileCoord coord,
                                                  TileLayerKind kind,
                                                  uint8_t marker) {
    char dir_path[512];
    char tile_path[512];
    uint8_t blob[64];
    size_t blob_size = 0u;
    CoreResult write_res;
    if (!tiles_root || tiles_root[0] == '\0') {
        return false;
    }
    snprintf(dir_path, sizeof(dir_path), "%s/%u/%u", tiles_root, coord.z, coord.x);
    if (!fixture_ensure_dir_recursive(dir_path)) {
        return false;
    }
    snprintf(tile_path, sizeof(tile_path), "%s/%u.%s", dir_path, coord.y, fixture_layer_suffix(kind));
    blob_size = fixture_build_minimal_tile_blob(coord, marker, blob, sizeof(blob));
    if (blob_size == 0u) {
        return false;
    }
    write_res = core_io_write_all(tile_path, blob, blob_size);
    return write_res.code == CORE_OK;
}
