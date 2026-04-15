#include "map/tile_source.h"

#include "core_io.h"

#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(MAPFORGE_HAVE_SQLITE)
#include <sqlite3.h>
#endif

static bool ensure_dir_recursive(const char *path) {
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
            *p = '\0';
            if (mkdir(tmp, 0755) != 0) {
                struct stat st = {0};
                if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
                    return false;
                }
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0) {
        struct stat st = {0};
        if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
            return false;
        }
    }
    return true;
}

static bool remove_tree(const char *path) {
    struct stat st = {0};
    if (!path || path[0] == '\0') {
        return false;
    }
    if (lstat(path, &st) != 0) {
        return true;
    }
    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) {
            return false;
        }
        struct dirent *entry = NULL;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char child[512];
            snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
            if (!remove_tree(child)) {
                closedir(dir);
                return false;
            }
        }
        closedir(dir);
        return rmdir(path) == 0;
    }
    return unlink(path) == 0;
}

#if !defined(MAPFORGE_HAVE_SQLITE)
int main(void) {
    printf("tile_source_archive_test: skipped (sqlite disabled)\n");
    return 0;
}
#else

typedef struct ArchiveLayerCase {
    TileLayerKind kind;
    const char *layer_key;
    uint8_t marker;
} ArchiveLayerCase;

static const ArchiveLayerCase k_cases[] = {
    {TILE_LAYER_ROAD_ARTERY, "road_artery", 0x11},
    {TILE_LAYER_ROAD_LOCAL, "road_local", 0x22},
    {TILE_LAYER_CONTOUR, "contour", 0x33},
    {TILE_LAYER_POLY_WATER, "water", 0x44},
    {TILE_LAYER_POLY_PARK, "park", 0x55},
    {TILE_LAYER_POLY_LANDUSE, "landuse", 0x66},
    {TILE_LAYER_POLY_BUILDING, "building", 0x77}
};

static size_t build_minimal_tile_blob(TileCoord coord, uint8_t marker, uint8_t *out, size_t cap) {
    const char magic[4] = {'M', 'F', 'T', '1'};
    uint16_t version = 1u;
    uint16_t z = coord.z;
    uint32_t x = coord.x;
    uint32_t y = coord.y;
    uint32_t polyline_count = 0u;
    const size_t needed = 4u + 2u + 2u + 4u + 4u + 4u + 1u;
    if (!out || cap < needed) {
        return 0u;
    }
    size_t pos = 0u;
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

static bool populate_archive_db(const char *db_path, TileCoord coord) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK || !db) {
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

    for (size_t i = 0u; i < sizeof(k_cases) / sizeof(k_cases[0]); ++i) {
        uint8_t blob[64];
        size_t blob_size = build_minimal_tile_blob(coord, k_cases[i].marker, blob, sizeof(blob));
        if (blob_size == 0u) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }
        sqlite3_bind_int(stmt, 1, (int)coord.z);
        sqlite3_bind_int(stmt, 2, (int)coord.x);
        sqlite3_bind_int(stmt, 3, (int)coord.y);
        sqlite3_bind_text(stmt, 4, k_cases[i].layer_key, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, "default", -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 6, blob, (int)blob_size, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
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
}

int main(void) {
    char tmp_root[] = "/tmp/mapforge_tile_source_archive_test.XXXXXX";
    char cache_dir[512];
    char archive_path[512];
    char missing_tiles_root[512];
    TileCoord coord = {12u, 655u, 1582u};
    if (!mkdtemp(tmp_root)) {
        fprintf(stderr, "mkdtemp failed\n");
        return 1;
    }
    snprintf(cache_dir, sizeof(cache_dir), "%s/cache", tmp_root);
    snprintf(archive_path, sizeof(archive_path), "%s/tiles.mbtiles", tmp_root);
    snprintf(missing_tiles_root, sizeof(missing_tiles_root), "%s/no_tree_tiles", tmp_root);
    if (!ensure_dir_recursive(cache_dir)) {
        fprintf(stderr, "failed to create cache dir\n");
        remove_tree(tmp_root);
        return 1;
    }
    if (!populate_archive_db(archive_path, coord)) {
        fprintf(stderr, "failed to seed archive db\n");
        remove_tree(tmp_root);
        return 1;
    }
    if (setenv("MAPFORGE_TILE_ARCHIVE_CACHE_DIR", cache_dir, 1) != 0) {
        fprintf(stderr, "setenv failed\n");
        remove_tree(tmp_root);
        return 1;
    }

    TileSourceConfig source = {0};
    assert(tile_source_config_set_archive(&source, missing_tiles_root, archive_path));
    tile_source_runtime_stats_reset();

    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0u; i < sizeof(k_cases) / sizeof(k_cases[0]); ++i) {
            char resolved[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
            assert(tile_source_resolve_path(&source,
                                            coord,
                                            k_cases[i].kind,
                                            TILE_BAND_DEFAULT,
                                            resolved,
                                            sizeof(resolved)));
            assert(core_io_path_exists(resolved));
            CoreBuffer readback = {0};
            CoreResult rr = core_io_read_all(resolved, &readback);
            assert(rr.code == CORE_OK);
            assert(readback.size > 0u);
            const uint8_t *bytes = (const uint8_t *)readback.data;
            assert(bytes[readback.size - 1u] == k_cases[i].marker);
            core_io_buffer_free(&readback);
        }
    }

    TileSourceRuntimeStats stats = {0};
    tile_source_runtime_stats_get(&stats);
    const uint64_t case_count = (uint64_t)(sizeof(k_cases) / sizeof(k_cases[0]));
    assert(stats.archive_request_count == case_count * 2u);
    assert(stats.archive_hit_count == case_count * 2u);
    assert(stats.archive_extract_count == case_count);
    assert(stats.archive_extract_fail_count == 0u);
    assert(stats.archive_fallback_tree_count == 0u);

    remove_tree(tmp_root);
    printf("tile_source_archive_test: success\n");
    return 0;
}
#endif
