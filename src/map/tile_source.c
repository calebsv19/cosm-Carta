#include "map/tile_source.h"

#include "core_io.h"

#include <errno.h>
#include <pthread.h>
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

typedef struct TileSourceStatsState {
    TileSourceRuntimeStats stats;
    pthread_mutex_t mutex;
} TileSourceStatsState;

static TileSourceStatsState g_tile_source_stats = {
    {0u, 0u, 0u, 0u, 0u, 0u},
    PTHREAD_MUTEX_INITIALIZER
};

static const char *tile_source_suffix(TileLayerKind kind) {
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

static const char *tile_source_layer_key(TileLayerKind kind) {
    switch (kind) {
        case TILE_LAYER_ROAD_ARTERY:
            return "road_artery";
        case TILE_LAYER_ROAD_LOCAL:
            return "road_local";
        case TILE_LAYER_CONTOUR:
            return "contour";
        case TILE_LAYER_POLY_WATER:
            return "water";
        case TILE_LAYER_POLY_PARK:
            return "park";
        case TILE_LAYER_POLY_LANDUSE:
            return "landuse";
        case TILE_LAYER_POLY_BUILDING:
            return "building";
        default:
            return "unknown";
    }
}

static const char *tile_source_band_dir(TileZoomBand band) {
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

static const char *tile_source_band_key(TileZoomBand band) {
    const char *dir = tile_source_band_dir(band);
    return dir ? dir : "default";
}

static bool tile_source_archive_layer_supported(TileLayerKind kind) {
    switch (kind) {
        case TILE_LAYER_ROAD_ARTERY:
        case TILE_LAYER_ROAD_LOCAL:
        case TILE_LAYER_CONTOUR:
        case TILE_LAYER_POLY_WATER:
        case TILE_LAYER_POLY_PARK:
        case TILE_LAYER_POLY_LANDUSE:
        case TILE_LAYER_POLY_BUILDING:
            return true;
        default:
            return false;
    }
}

static void tile_source_stats_note_archive_request(void) {
    pthread_mutex_lock(&g_tile_source_stats.mutex);
    g_tile_source_stats.stats.archive_request_count += 1u;
    pthread_mutex_unlock(&g_tile_source_stats.mutex);
}

static void tile_source_stats_note_archive_hit(bool extracted) {
    pthread_mutex_lock(&g_tile_source_stats.mutex);
    g_tile_source_stats.stats.archive_hit_count += 1u;
    if (extracted) {
        g_tile_source_stats.stats.archive_extract_count += 1u;
    }
    pthread_mutex_unlock(&g_tile_source_stats.mutex);
}

static void tile_source_stats_note_archive_fail(void) {
    pthread_mutex_lock(&g_tile_source_stats.mutex);
    g_tile_source_stats.stats.archive_extract_fail_count += 1u;
    pthread_mutex_unlock(&g_tile_source_stats.mutex);
}

static void tile_source_stats_note_fallback_tree(void) {
    pthread_mutex_lock(&g_tile_source_stats.mutex);
    g_tile_source_stats.stats.archive_fallback_tree_count += 1u;
    pthread_mutex_unlock(&g_tile_source_stats.mutex);
}

static void tile_source_stats_note_policy_block(void) {
    pthread_mutex_lock(&g_tile_source_stats.mutex);
    g_tile_source_stats.stats.archive_policy_block_count += 1u;
    pthread_mutex_unlock(&g_tile_source_stats.mutex);
}

static bool tile_source_path_copy(char *out_path, size_t out_size, const char *value) {
    int n = 0;
    if (!out_path || out_size == 0u || !value || value[0] == '\0') {
        return false;
    }
    n = snprintf(out_path, out_size, "%s", value);
    if (n < 0 || (size_t)n >= out_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

static uint64_t tile_source_hash_u64(const char *value) {
    uint64_t hash = 1469598103934665603ull;
    if (!value) {
        return hash;
    }
    for (size_t i = 0; value[i] != '\0'; ++i) {
        hash ^= (uint64_t)(uint8_t)value[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

static bool tile_source_ensure_dir_recursive(const char *path) {
    char tmp[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
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
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static bool tile_source_ensure_parent_dir(const char *path) {
    char parent[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
    char *slash = NULL;
    if (!path || path[0] == '\0') {
        return false;
    }
    snprintf(parent, sizeof(parent), "%s", path);
    slash = strrchr(parent, '/');
    if (!slash) {
        return true;
    }
    *slash = '\0';
    if (parent[0] == '\0') {
        return true;
    }
    return tile_source_ensure_dir_recursive(parent);
}

static bool tile_source_resolve_tree_path(const char *tiles_root,
                                          TileCoord coord,
                                          TileLayerKind kind,
                                          TileZoomBand band,
                                          bool legacy,
                                          char *out_path,
                                          size_t out_size) {
    const char *band_dir = NULL;
    if (!tiles_root || tiles_root[0] == '\0' || !out_path || out_size == 0u) {
        return false;
    }

    band_dir = tile_source_band_dir(band);
    if (band_dir) {
        if (legacy) {
            snprintf(out_path, out_size, "%s/bands/%s/%u/%u/%u.mft",
                     tiles_root, band_dir, coord.z, coord.x, coord.y);
        } else {
            snprintf(out_path, out_size, "%s/bands/%s/%u/%u/%u.%s",
                     tiles_root, band_dir, coord.z, coord.x, coord.y, tile_source_suffix(kind));
        }
        if (core_io_path_exists(out_path)) {
            return true;
        }
    }

    if (legacy) {
        snprintf(out_path, out_size, "%s/%u/%u/%u.mft", tiles_root, coord.z, coord.x, coord.y);
    } else {
        snprintf(out_path, out_size, "%s/%u/%u/%u.%s",
                 tiles_root, coord.z, coord.x, coord.y, tile_source_suffix(kind));
    }
    return core_io_path_exists(out_path);
}

static bool tile_source_build_archive_cache_path(const TileSourceConfig *config,
                                                 TileCoord coord,
                                                 TileLayerKind kind,
                                                 TileZoomBand band,
                                                 char *out_path,
                                                 size_t out_size) {
    const char *cache_root_env = getenv("MAPFORGE_TILE_ARCHIVE_CACHE_DIR");
    const char *cache_root = (cache_root_env && cache_root_env[0] != '\0')
        ? cache_root_env
        : "/tmp/mapforge_tile_archive_cache";
    uint64_t archive_hash = 0u;
    int n = 0;
    if (!config || !out_path || out_size == 0u || config->archive_path[0] == '\0') {
        return false;
    }
    archive_hash = tile_source_hash_u64(config->archive_path);
    n = snprintf(out_path,
                 out_size,
                 "%s/%016llx/%s/%s/%u/%u/%u.mft",
                 cache_root,
                 (unsigned long long)archive_hash,
                 tile_source_layer_key(kind),
                 tile_source_band_key(band),
                 coord.z,
                 coord.x,
                 coord.y);
    if (n < 0 || (size_t)n >= out_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

#if defined(MAPFORGE_HAVE_SQLITE)
static bool tile_source_sqlite_query_blob(sqlite3 *db,
                                          const char *sql,
                                          int z,
                                          int x,
                                          int y,
                                          const char *layer,
                                          const char *band,
                                          bool bind_layer,
                                          bool bind_band,
                                          uint8_t **out_blob,
                                          size_t *out_size) {
    sqlite3_stmt *stmt = NULL;
    int rc = 0;
    int idx = 1;
    if (!db || !sql || !out_blob || !out_size) {
        return false;
    }
    *out_blob = NULL;
    *out_size = 0u;

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK || !stmt) {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        return false;
    }

    sqlite3_bind_int(stmt, idx++, z);
    sqlite3_bind_int(stmt, idx++, x);
    sqlite3_bind_int(stmt, idx++, y);
    if (bind_layer) {
        sqlite3_bind_text(stmt, idx++, layer, -1, SQLITE_STATIC);
    }
    if (bind_band) {
        sqlite3_bind_text(stmt, idx++, band, -1, SQLITE_STATIC);
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int bytes = sqlite3_column_bytes(stmt, 0);
        const void *blob = sqlite3_column_blob(stmt, 0);
        if (bytes > 0 && blob) {
            uint8_t *copy = (uint8_t *)malloc((size_t)bytes);
            if (copy) {
                memcpy(copy, blob, (size_t)bytes);
                *out_blob = copy;
                *out_size = (size_t)bytes;
            }
        }
    }

    sqlite3_finalize(stmt);
    return *out_blob != NULL && *out_size > 0u;
}

static bool tile_source_sqlite_fetch_blob(sqlite3 *db,
                                          TileCoord coord,
                                          TileLayerKind kind,
                                          TileZoomBand band,
                                          uint8_t **out_blob,
                                          size_t *out_size) {
    const char *layer = tile_source_layer_key(kind);
    const char *band_key = tile_source_band_key(band);
    int z = (int)coord.z;
    int x = (int)coord.x;
    int y_values[2];
    size_t y_count = 0u;
    if (!db || !out_blob || !out_size) {
        return false;
    }
    *out_blob = NULL;
    *out_size = 0u;

    y_values[y_count++] = (int)coord.y;
    if (coord.z < 31u) {
        int tms_y = ((1 << coord.z) - 1) - (int)coord.y;
        if (tms_y != y_values[0]) {
            y_values[y_count++] = tms_y;
        }
    }

    for (size_t yi = 0u; yi < y_count; ++yi) {
        int y = y_values[yi];
        if (tile_source_sqlite_query_blob(db,
                                          "SELECT tile_data FROM mapforge_tiles "
                                          "WHERE z=?1 AND x=?2 AND y=?3 AND layer=?4 AND band=?5 LIMIT 1",
                                          z, x, y, layer, band_key, true, true, out_blob, out_size) ||
            tile_source_sqlite_query_blob(db,
                                          "SELECT tile_data FROM mapforge_tiles "
                                          "WHERE z=?1 AND x=?2 AND y=?3 AND layer=?4 LIMIT 1",
                                          z, x, y, layer, band_key, true, false, out_blob, out_size) ||
            tile_source_sqlite_query_blob(db,
                                          "SELECT tile_data FROM tiles "
                                          "WHERE zoom_level=?1 AND tile_column=?2 AND tile_row=?3 AND layer=?4 AND band=?5 LIMIT 1",
                                          z, x, y, layer, band_key, true, true, out_blob, out_size) ||
            tile_source_sqlite_query_blob(db,
                                          "SELECT tile_data FROM tiles "
                                          "WHERE zoom_level=?1 AND tile_column=?2 AND tile_row=?3 AND layer=?4 LIMIT 1",
                                          z, x, y, layer, band_key, true, false, out_blob, out_size) ||
            tile_source_sqlite_query_blob(db,
                                          "SELECT tile_data FROM tiles "
                                          "WHERE zoom_level=?1 AND tile_column=?2 AND tile_row=?3 LIMIT 1",
                                          z, x, y, layer, band_key, false, false, out_blob, out_size)) {
            return true;
        }
    }
    return false;
}

static bool tile_source_try_materialize_archive(const TileSourceConfig *config,
                                                TileCoord coord,
                                                TileLayerKind kind,
                                                TileZoomBand band,
                                                char *out_path,
                                                size_t out_size) {
    sqlite3 *db = NULL;
    uint8_t *blob = NULL;
    size_t blob_size = 0u;
    char cache_path[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
    char tmp_path[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
    CoreResult write_res;
    if (!config || !out_path || out_size == 0u || config->archive_path[0] == '\0') {
        return false;
    }
    if (!tile_source_build_archive_cache_path(config, coord, kind, band, cache_path, sizeof(cache_path))) {
        return false;
    }
    if (core_io_path_exists(cache_path)) {
        if (!tile_source_path_copy(out_path, out_size, cache_path)) {
            return false;
        }
        tile_source_stats_note_archive_hit(false);
        return true;
    }
    if (!tile_source_ensure_parent_dir(cache_path)) {
        return false;
    }
    if (sqlite3_open_v2(config->archive_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK || !db) {
        if (db) {
            sqlite3_close(db);
        }
        return false;
    }
    if (!tile_source_sqlite_fetch_blob(db, coord, kind, band, &blob, &blob_size)) {
        sqlite3_close(db);
        return false;
    }
    sqlite3_close(db);
    db = NULL;

    snprintf(tmp_path,
             sizeof(tmp_path),
             "%s.tmp.%llu.%llu",
             cache_path,
             (unsigned long long)getpid(),
             (unsigned long long)(uintptr_t)pthread_self());
    write_res = core_io_write_all(tmp_path, blob, blob_size);
    free(blob);
    blob = NULL;
    if (write_res.code != CORE_OK) {
        (void)unlink(tmp_path);
        return false;
    }
    if (rename(tmp_path, cache_path) != 0) {
        (void)unlink(tmp_path);
        if (!core_io_path_exists(cache_path)) {
            return false;
        }
    }
    if (!tile_source_path_copy(out_path, out_size, cache_path)) {
        return false;
    }
    tile_source_stats_note_archive_hit(true);
    return true;
}
#endif

void tile_source_config_init(TileSourceConfig *config) {
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->storage_kind = TILE_STORAGE_FILESYSTEM_TREE;
    config->policy_mode = TILE_SOURCE_POLICY_FILESYSTEM_ONLY;
}

bool tile_source_config_set_filesystem(TileSourceConfig *config, const char *tiles_root) {
    if (!config || !tiles_root || tiles_root[0] == '\0') {
        return false;
    }
    tile_source_config_init(config);
    config->storage_kind = TILE_STORAGE_FILESYSTEM_TREE;
    config->policy_mode = TILE_SOURCE_POLICY_FILESYSTEM_ONLY;
    return tile_source_path_copy(config->tiles_root, sizeof(config->tiles_root), tiles_root);
}

bool tile_source_config_set_archive(TileSourceConfig *config, const char *tiles_root, const char *archive_path) {
    return tile_source_config_set_archive_with_policy(
        config,
        tiles_root,
        archive_path,
        TILE_SOURCE_POLICY_ARCHIVE_PREFERRED);
}

bool tile_source_config_set_archive_with_policy(TileSourceConfig *config,
                                                const char *tiles_root,
                                                const char *archive_path,
                                                TileSourcePolicyMode policy_mode) {
    if (!config) {
        return false;
    }
    tile_source_config_init(config);
    config->storage_kind = TILE_STORAGE_ARCHIVE_INDEXED;
    config->policy_mode = policy_mode;
    if (tiles_root && tiles_root[0] != '\0') {
        if (!tile_source_path_copy(config->tiles_root, sizeof(config->tiles_root), tiles_root)) {
            return false;
        }
    }
    if (archive_path && archive_path[0] != '\0') {
        if (!tile_source_path_copy(config->archive_path, sizeof(config->archive_path), archive_path)) {
            return false;
        }
    }
    return true;
}

bool tile_source_config_set_policy(TileSourceConfig *config, TileSourcePolicyMode policy_mode) {
    if (!config) {
        return false;
    }
    config->policy_mode = policy_mode;
    return true;
}

const char *tile_storage_kind_label(TileStorageKind kind) {
    switch (kind) {
        case TILE_STORAGE_FILESYSTEM_TREE:
            return "filesystem_tree";
        case TILE_STORAGE_ARCHIVE_INDEXED:
            return "archive_indexed";
        default:
            return "filesystem_tree";
    }
}

TileStorageKind tile_storage_kind_from_string(const char *value) {
    if (!value || value[0] == '\0') {
        return TILE_STORAGE_FILESYSTEM_TREE;
    }
    if (strcmp(value, "archive_indexed") == 0 ||
        strcmp(value, "pmtiles") == 0 ||
        strcmp(value, "mbtiles") == 0) {
        return TILE_STORAGE_ARCHIVE_INDEXED;
    }
    return TILE_STORAGE_FILESYSTEM_TREE;
}

const char *tile_source_policy_mode_label(TileSourcePolicyMode mode) {
    switch (mode) {
        case TILE_SOURCE_POLICY_ARCHIVE_REQUIRED:
            return "archive_required";
        case TILE_SOURCE_POLICY_ARCHIVE_PREFERRED:
            return "archive_preferred";
        case TILE_SOURCE_POLICY_FILESYSTEM_ONLY:
            return "filesystem_only";
        default:
            return "archive_preferred";
    }
}

bool tile_source_policy_mode_from_string(const char *value, TileSourcePolicyMode *out_mode) {
    if (!out_mode) {
        return false;
    }
    if (!value || value[0] == '\0') {
        return false;
    }
    if (strcmp(value, "archive_required") == 0) {
        *out_mode = TILE_SOURCE_POLICY_ARCHIVE_REQUIRED;
        return true;
    }
    if (strcmp(value, "archive_preferred") == 0) {
        *out_mode = TILE_SOURCE_POLICY_ARCHIVE_PREFERRED;
        return true;
    }
    if (strcmp(value, "filesystem_only") == 0) {
        *out_mode = TILE_SOURCE_POLICY_FILESYSTEM_ONLY;
        return true;
    }
    return false;
}

TileSourcePolicyMode tile_source_policy_mode_default(TileStorageKind storage_kind) {
    if (storage_kind == TILE_STORAGE_ARCHIVE_INDEXED) {
        return TILE_SOURCE_POLICY_ARCHIVE_PREFERRED;
    }
    return TILE_SOURCE_POLICY_FILESYSTEM_ONLY;
}

bool tile_source_archive_reader_supported(void) {
#if defined(MAPFORGE_HAVE_SQLITE)
    return true;
#else
    return false;
#endif
}

void tile_source_runtime_stats_reset(void) {
    pthread_mutex_lock(&g_tile_source_stats.mutex);
    memset(&g_tile_source_stats.stats, 0, sizeof(g_tile_source_stats.stats));
    pthread_mutex_unlock(&g_tile_source_stats.mutex);
}

void tile_source_runtime_stats_get(TileSourceRuntimeStats *out_stats) {
    if (!out_stats) {
        return;
    }
    pthread_mutex_lock(&g_tile_source_stats.mutex);
    *out_stats = g_tile_source_stats.stats;
    pthread_mutex_unlock(&g_tile_source_stats.mutex);
}

bool tile_source_resolve_path(const TileSourceConfig *config,
                              TileCoord coord,
                              TileLayerKind kind,
                              TileZoomBand band,
                              char *out_path,
                              size_t out_size) {
    bool archive_capable = false;
    bool archive_attempted = false;
    if (!config || !out_path || out_size == 0u) {
        return false;
    }
    out_path[0] = '\0';

    archive_capable = config->storage_kind == TILE_STORAGE_ARCHIVE_INDEXED &&
                      config->archive_path[0] != '\0' &&
                      tile_source_archive_layer_supported(kind);

    if (config->policy_mode == TILE_SOURCE_POLICY_ARCHIVE_REQUIRED &&
        (!archive_capable || !tile_source_archive_reader_supported())) {
        tile_source_stats_note_policy_block();
        out_path[0] = '\0';
        return false;
    }

    if (archive_capable &&
        config->policy_mode != TILE_SOURCE_POLICY_FILESYSTEM_ONLY &&
        tile_source_archive_reader_supported()) {
        archive_attempted = true;
        tile_source_stats_note_archive_request();
#if defined(MAPFORGE_HAVE_SQLITE)
        if (tile_source_try_materialize_archive(config, coord, kind, band, out_path, out_size)) {
            return true;
        }
#endif
        tile_source_stats_note_archive_fail();
        if (config->policy_mode == TILE_SOURCE_POLICY_ARCHIVE_REQUIRED) {
            tile_source_stats_note_policy_block();
            out_path[0] = '\0';
            return false;
        }
    }

    if (tile_source_resolve_tree_path(config->tiles_root, coord, kind, band, false, out_path, out_size)) {
        if (archive_capable &&
            config->policy_mode == TILE_SOURCE_POLICY_ARCHIVE_PREFERRED &&
            (archive_attempted || !tile_source_archive_reader_supported())) {
            tile_source_stats_note_fallback_tree();
        }
        return true;
    }

    if (config->policy_mode == TILE_SOURCE_POLICY_ARCHIVE_REQUIRED) {
        tile_source_stats_note_policy_block();
    }

    out_path[0] = '\0';
    return false;
}

bool tile_source_resolve_legacy_path(const TileSourceConfig *config,
                                     TileCoord coord,
                                     TileZoomBand band,
                                     char *out_path,
                                     size_t out_size) {
    if (!config || !out_path || out_size == 0u) {
        return false;
    }
    out_path[0] = '\0';
    return tile_source_resolve_tree_path(config->tiles_root, coord, TILE_LAYER_ROAD_ARTERY, band, true, out_path, out_size);
}
