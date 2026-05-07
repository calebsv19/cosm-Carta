#include "mapforge_region_internal.h"

static const char *k_metric_band_names[METRIC_BAND_COUNT] = {
    "default",
    "coarse",
    "mid",
    "fine"
};

static const char *k_metric_layer_names[METRIC_LAYER_COUNT] = {
    "artery",
    "local",
    "water",
    "park",
    "landuse",
    "building",
    "contour"
};

static const char *osm_source_kind_label(OSMSourceKind kind) {
    switch (kind) {
        case OSM_SOURCE_KIND_PBF:
            return "pbf";
        case OSM_SOURCE_KIND_XML:
            return "xml";
        case OSM_SOURCE_KIND_UNKNOWN:
        default:
            return "unknown";
    }
}

static bool path_exists(const char *path) {
    return path && core_io_path_exists(path);
}

static bool ensure_dir(const char *path) {
    if (!path) {
        return false;
    }
    if (mkdir(path, 0755) == 0) {
        return true;
    }
    return errno == EEXIST;
}

static bool ensure_dir_recursive_local(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s", path);
    size_t len = strlen(buffer);
    if (len == 0u) {
        return false;
    }
    if (buffer[len - 1] == '/') {
        buffer[len - 1] = '\0';
    }
    for (char *p = buffer + 1; *p != '\0'; ++p) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (!ensure_dir(buffer)) {
            return false;
        }
        *p = '/';
    }
    return ensure_dir(buffer);
}

bool mapforge_region_archive_rel_path_valid(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    if (path[0] == '/' || strstr(path, "..")) {
        return false;
    }
    return true;
}

static bool ensure_parent_dir_recursive(const char *path) {
    char parent[512];
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
    return ensure_dir_recursive_local(parent);
}

static bool string_has_suffix(const char *value, const char *suffix) {
    if (!value || !suffix) {
        return false;
    }
    size_t value_len = strlen(value);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > value_len) {
        return false;
    }
    return strcmp(value + (value_len - suffix_len), suffix) == 0;
}

static const char *archive_layer_from_suffix(const char *suffix) {
    if (!suffix || suffix[0] == '\0') {
        return NULL;
    }
    if (strcmp(suffix, "artery.mft") == 0) {
        return "road_artery";
    }
    if (strcmp(suffix, "local.mft") == 0) {
        return "road_local";
    }
    if (strcmp(suffix, "water.mft") == 0) {
        return "water";
    }
    if (strcmp(suffix, "park.mft") == 0) {
        return "park";
    }
    if (strcmp(suffix, "landuse.mft") == 0) {
        return "landuse";
    }
    if (strcmp(suffix, "building.mft") == 0) {
        return "building";
    }
    if (strcmp(suffix, "contour.mft") == 0) {
        return "contour";
    }
    if (strcmp(suffix, "mft") == 0) {
        return "road_artery";
    }
    return NULL;
}

static int archive_metric_band_index(const char *band) {
    if (!band || band[0] == '\0' || strcmp(band, "default") == 0) {
        return METRIC_BAND_DEFAULT;
    }
    if (strcmp(band, "coarse") == 0) {
        return METRIC_BAND_COARSE;
    }
    if (strcmp(band, "mid") == 0) {
        return METRIC_BAND_MID;
    }
    if (strcmp(band, "fine") == 0) {
        return METRIC_BAND_FINE;
    }
    return -1;
}

static int archive_metric_layer_index(const char *layer) {
    if (!layer || layer[0] == '\0') {
        return -1;
    }
    if (strcmp(layer, "road_artery") == 0) {
        return METRIC_LAYER_ARTERY;
    }
    if (strcmp(layer, "road_local") == 0) {
        return METRIC_LAYER_LOCAL;
    }
    if (strcmp(layer, "water") == 0) {
        return METRIC_LAYER_WATER;
    }
    if (strcmp(layer, "park") == 0) {
        return METRIC_LAYER_PARK;
    }
    if (strcmp(layer, "landuse") == 0) {
        return METRIC_LAYER_LANDUSE;
    }
    if (strcmp(layer, "building") == 0) {
        return METRIC_LAYER_BUILDING;
    }
    if (strcmp(layer, "contour") == 0) {
        return METRIC_LAYER_CONTOUR;
    }
    return -1;
}

static bool archive_parse_tile_rel_path(const char *rel_path,
                                        int *out_z,
                                        int *out_x,
                                        int *out_y,
                                        char *out_band,
                                        size_t out_band_size,
                                        char *out_layer,
                                        size_t out_layer_size) {
    if (!rel_path || !out_z || !out_x || !out_y ||
        !out_band || out_band_size == 0u || !out_layer || out_layer_size == 0u) {
        return false;
    }

    char rel_copy[512];
    snprintf(rel_copy, sizeof(rel_copy), "%s", rel_path);

    char *save = NULL;
    char *tok0 = strtok_r(rel_copy, "/", &save);
    char *tok1 = tok0 ? strtok_r(NULL, "/", &save) : NULL;
    char *tok2 = tok1 ? strtok_r(NULL, "/", &save) : NULL;
    char *tok3 = tok2 ? strtok_r(NULL, "/", &save) : NULL;
    char *z_token = NULL;
    char *x_token = NULL;
    char *file_token = NULL;
    const char *band = "default";

    if (!tok0 || !tok1 || !tok2) {
        return false;
    }

    if (strcmp(tok0, "bands") == 0) {
        if (!tok3) {
            return false;
        }
        band = tok1;
        z_token = tok2;
        x_token = tok3;
        file_token = strtok_r(NULL, "/", &save);
    } else {
        z_token = tok0;
        x_token = tok1;
        file_token = tok2;
    }
    if (!file_token) {
        return false;
    }
    if (strtok_r(NULL, "/", &save) != NULL) {
        return false;
    }

    char file_copy[256];
    snprintf(file_copy, sizeof(file_copy), "%s", file_token);
    char *dot = strchr(file_copy, '.');
    if (!dot) {
        return false;
    }
    *dot = '\0';
    const char *suffix = dot + 1;
    if (file_copy[0] == '\0' || suffix[0] == '\0') {
        return false;
    }
    const char *layer = archive_layer_from_suffix(suffix);
    if (!layer) {
        return false;
    }

    *out_z = atoi(z_token);
    *out_x = atoi(x_token);
    *out_y = atoi(file_copy);
    if (*out_z < 0 || *out_x < 0 || *out_y < 0) {
        return false;
    }
    snprintf(out_band, out_band_size, "%s", band);
    snprintf(out_layer, out_layer_size, "%s", layer);
    return true;
}

#if defined(MAPFORGE_HAVE_SQLITE)
static bool archive_exec_sql(sqlite3 *db, const char *sql) {
    char *errmsg = NULL;
    if (!db || !sql) {
        return false;
    }
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        log_error("archive sqlite exec failed: %s", errmsg ? errmsg : "unknown error");
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

static bool archive_insert_tile(sqlite3_stmt *stmt,
                                const char *path,
                                const char *rel_path,
                                BuildContext *ctx) {
    int z = 0;
    int x = 0;
    int y = 0;
    char band[32];
    char layer[64];
    CoreBuffer tile_data = {0};
    if (!stmt || !path || !rel_path || !ctx) {
        return false;
    }
    if (!archive_parse_tile_rel_path(rel_path,
                                     &z, &x, &y,
                                     band, sizeof(band),
                                     layer, sizeof(layer))) {
        return true;
    }
    CoreResult read_res = core_io_read_all(path, &tile_data);
    if (read_res.code != CORE_OK || !tile_data.data || tile_data.size == 0u) {
        log_error("archive emit read failed: %s", path);
        core_io_buffer_free(&tile_data);
        return false;
    }

    sqlite3_bind_int(stmt, 1, z);
    sqlite3_bind_int(stmt, 2, x);
    sqlite3_bind_int(stmt, 3, y);
    sqlite3_bind_text(stmt, 4, layer, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, band, -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 6, tile_data.data, (int)tile_data.size, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    if (rc != SQLITE_DONE) {
        log_error("archive emit insert failed for %s: %s", path, sqlite3_errstr(rc));
        core_io_buffer_free(&tile_data);
        return false;
    }
    int band_index = archive_metric_band_index(band);
    int layer_index = archive_metric_layer_index(layer);
    if (band_index >= 0 && band_index < METRIC_BAND_COUNT &&
        layer_index >= 0 && layer_index < METRIC_LAYER_COUNT) {
        ctx->archive_rollup_rows[band_index][layer_index] += 1u;
        ctx->archive_rollup_bytes[band_index][layer_index] += (uint64_t)tile_data.size;
    }
    ctx->archive_tile_rows += 1u;
    ctx->archive_bytes_written += (uint64_t)tile_data.size;
    core_io_buffer_free(&tile_data);
    return true;
}

static bool archive_ingest_tree(sqlite3 *db,
                                sqlite3_stmt *insert_stmt,
                                const char *tiles_root,
                                const char *dir_path,
                                BuildContext *ctx) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    size_t root_len = 0u;
    if (!db || !insert_stmt || !tiles_root || !dir_path || !ctx) {
        return false;
    }
    root_len = strlen(tiles_root);
    dir = opendir(dir_path);
    if (!dir) {
        return false;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char path[512];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (lstat(path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (!archive_ingest_tree(db, insert_stmt, tiles_root, path, ctx)) {
                closedir(dir);
                return false;
            }
            continue;
        }
        if (!S_ISREG(st.st_mode) || !string_has_suffix(entry->d_name, ".mft")) {
            continue;
        }
        if (strncmp(path, tiles_root, root_len) != 0 || path[root_len] != '/') {
            continue;
        }
        const char *rel_path = path + root_len + 1u;
        if (!archive_insert_tile(insert_stmt, path, rel_path, ctx)) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);
    return true;
}
#endif

bool mapforge_region_write_tile_archive_sqlite(const BuildOptions *options, BuildContext *ctx) {
    if (!options || !ctx) {
        return false;
    }
    if (!options->emit_archive) {
        return true;
    }
#if !defined(MAPFORGE_HAVE_SQLITE)
    log_error("archive emit requested but build lacks sqlite support");
    return false;
#else
    const char *archive_rel = (options->archive_path && options->archive_path[0] != '\0')
        ? options->archive_path
        : "tiles.mbtiles";
    if (!mapforge_region_archive_rel_path_valid(archive_rel)) {
        log_error("archive path must be region-local and not contain '..': %s", archive_rel);
        return false;
    }

    char tiles_root[512];
    char archive_file[512];
    snprintf(tiles_root, sizeof(tiles_root), "%s/tiles", options->out_dir);
    snprintf(archive_file, sizeof(archive_file), "%s/%s", options->out_dir, archive_rel);
    if (!path_exists(tiles_root)) {
        log_error("archive emit missing tiles root: %s", tiles_root);
        return false;
    }
    if (!ensure_parent_dir_recursive(archive_file)) {
        log_error("archive emit failed to ensure parent dir: %s", archive_file);
        return false;
    }
    (void)unlink(archive_file);

    sqlite3 *db = NULL;
    sqlite3_stmt *insert_stmt = NULL;
    if (sqlite3_open_v2(archive_file, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK || !db) {
        log_error("archive emit open failed: %s", archive_file);
        if (db) {
            sqlite3_close(db);
        }
        return false;
    }
    bool ok = archive_exec_sql(db, "PRAGMA journal_mode=DELETE;") &&
              archive_exec_sql(db, "PRAGMA synchronous=NORMAL;") &&
              archive_exec_sql(db, "BEGIN IMMEDIATE;") &&
              archive_exec_sql(db,
                               "CREATE TABLE IF NOT EXISTS mapforge_tiles ("
                               "z INTEGER NOT NULL,"
                               "x INTEGER NOT NULL,"
                               "y INTEGER NOT NULL,"
                               "layer TEXT NOT NULL,"
                               "band TEXT NOT NULL,"
                               "tile_data BLOB NOT NULL,"
                               "PRIMARY KEY (z, x, y, layer, band));");
    if (!ok) {
        (void)archive_exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return false;
    }

    if (sqlite3_prepare_v2(db,
                           "INSERT OR REPLACE INTO mapforge_tiles "
                           "(z, x, y, layer, band, tile_data) "
                           "VALUES (?1, ?2, ?3, ?4, ?5, ?6);",
                           -1,
                           &insert_stmt,
                           NULL) != SQLITE_OK || !insert_stmt) {
        (void)archive_exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        log_error("archive emit prepare failed");
        return false;
    }

    ok = archive_ingest_tree(db, insert_stmt, tiles_root, tiles_root, ctx);
    sqlite3_finalize(insert_stmt);
    insert_stmt = NULL;
    if (!ok || ctx->archive_tile_rows == 0u) {
        (void)archive_exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        log_error("archive emit failed to ingest tile rows");
        return false;
    }
    ok = archive_exec_sql(db, "COMMIT;");
    sqlite3_close(db);
    if (!ok) {
        log_error("archive emit commit failed");
        return false;
    }
    log_info("archive emitted: %s rows=%llu bytes=%llu",
             archive_file,
             (unsigned long long)ctx->archive_tile_rows,
             (unsigned long long)ctx->archive_bytes_written);
    return true;
#endif
}

static bool format_archive_rollups_json(const BuildContext *ctx, char *out_json, size_t out_size) {
    if (!ctx || !out_json || out_size == 0u) {
        return false;
    }
    size_t off = 0u;
    int n = snprintf(out_json + off, out_size - off,
                     "{\n"
                     "            \"rows\": %llu,\n"
                     "            \"bytes\": %llu,\n"
                     "            \"bands\": {\n",
                     (unsigned long long)ctx->archive_tile_rows,
                     (unsigned long long)ctx->archive_bytes_written);
    if (n <= 0 || (size_t)n >= (out_size - off)) {
        return false;
    }
    off += (size_t)n;

    for (int b = 0; b < METRIC_BAND_COUNT; ++b) {
        n = snprintf(out_json + off, out_size - off, "                \"%s\": {\n", k_metric_band_names[b]);
        if (n <= 0 || (size_t)n >= (out_size - off)) {
            return false;
        }
        off += (size_t)n;
        for (int l = 0; l < METRIC_LAYER_COUNT; ++l) {
            const char *comma = (l + 1 < METRIC_LAYER_COUNT) ? "," : "";
            n = snprintf(out_json + off, out_size - off,
                         "                    \"%s\": {\"rows\": %llu, \"bytes\": %llu}%s\n",
                         k_metric_layer_names[l],
                         (unsigned long long)ctx->archive_rollup_rows[b][l],
                         (unsigned long long)ctx->archive_rollup_bytes[b][l],
                         comma);
            if (n <= 0 || (size_t)n >= (out_size - off)) {
                return false;
            }
            off += (size_t)n;
        }
        n = snprintf(out_json + off, out_size - off, "                }%s\n",
                     (b + 1 < METRIC_BAND_COUNT) ? "," : "");
        if (n <= 0 || (size_t)n >= (out_size - off)) {
            return false;
        }
        off += (size_t)n;
    }

    n = snprintf(out_json + off, out_size - off, "            }\n        }");
    if (n <= 0 || (size_t)n >= (out_size - off)) {
        return false;
    }
    off += (size_t)n;
    return off < out_size;
}

static bool format_tile_coverage_json(const BuildContext *ctx, char *out_json, size_t out_size) {
    if (!ctx || !out_json || out_size == 0u) {
        return false;
    }
    size_t off = 0u;
    int n = snprintf(out_json + off, out_size - off,
                     "{\n"
                     "            \"rows\": %llu,\n"
                     "            \"bands\": {\n",
                     (unsigned long long)ctx->coverage_total_tiles);
    if (n <= 0 || (size_t)n >= (out_size - off)) {
        return false;
    }
    off += (size_t)n;

    for (int b = 0; b < METRIC_BAND_COUNT; ++b) {
        n = snprintf(out_json + off, out_size - off, "                \"%s\": {\n", k_metric_band_names[b]);
        if (n <= 0 || (size_t)n >= (out_size - off)) {
            return false;
        }
        off += (size_t)n;

        bool wrote_layer = false;
        for (int l = 0; l < METRIC_LAYER_COUNT; ++l) {
            if (ctx->coverage_tiles[b][l] == 0u) {
                continue;
            }
            if (wrote_layer) {
                n = snprintf(out_json + off, out_size - off, ",\n");
                if (n <= 0 || (size_t)n >= (out_size - off)) {
                    return false;
                }
                off += (size_t)n;
            }

            n = snprintf(out_json + off, out_size - off,
                         "                    \"%s\": {\n"
                         "                        \"tile_count\": %llu,\n"
                         "                        \"zoom_bounds\": {\n",
                         k_metric_layer_names[l],
                         (unsigned long long)ctx->coverage_tiles[b][l]);
            if (n <= 0 || (size_t)n >= (out_size - off)) {
                return false;
            }
            off += (size_t)n;

            bool wrote_zoom = false;
            for (uint32_t z = 0u; z <= MAPFORGE_TILE_COVERAGE_MAX_ZOOM; ++z) {
                if (!ctx->coverage_has_zoom[b][l][z]) {
                    continue;
                }
                if (wrote_zoom) {
                    n = snprintf(out_json + off, out_size - off, ",\n");
                    if (n <= 0 || (size_t)n >= (out_size - off)) {
                        return false;
                    }
                    off += (size_t)n;
                }
                n = snprintf(out_json + off, out_size - off,
                             "                            \"%u\": {\"tiles\": %u, \"min_x\": %u, \"max_x\": %u, \"min_y\": %u, \"max_y\": %u}",
                             z,
                             ctx->coverage_zoom_tiles[b][l][z],
                             ctx->coverage_zoom_min_x[b][l][z],
                             ctx->coverage_zoom_max_x[b][l][z],
                             ctx->coverage_zoom_min_y[b][l][z],
                             ctx->coverage_zoom_max_y[b][l][z]);
                if (n <= 0 || (size_t)n >= (out_size - off)) {
                    return false;
                }
                off += (size_t)n;
                wrote_zoom = true;
            }

            n = snprintf(out_json + off, out_size - off,
                         "\n"
                         "                        }\n"
                         "                    }");
            if (n <= 0 || (size_t)n >= (out_size - off)) {
                return false;
            }
            off += (size_t)n;
            wrote_layer = true;
        }

        if (wrote_layer) {
            n = snprintf(out_json + off, out_size - off, "\n");
            if (n <= 0 || (size_t)n >= (out_size - off)) {
                return false;
            }
            off += (size_t)n;
        }

        n = snprintf(out_json + off, out_size - off, "                }%s\n",
                     (b + 1 < METRIC_BAND_COUNT) ? "," : "");
        if (n <= 0 || (size_t)n >= (out_size - off)) {
            return false;
        }
        off += (size_t)n;
    }

    n = snprintf(out_json + off, out_size - off, "            }\n        }");
    if (n <= 0 || (size_t)n >= (out_size - off)) {
        return false;
    }
    off += (size_t)n;
    return off < out_size;
}

bool mapforge_region_write_meta_json(const BuildOptions *options, const BuildContext *ctx) {
    if (!options || !ctx || !options->out_dir) {
        return false;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/meta.json", options->out_dir);

    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);
    char timestamp[64] = "";
    char terrain_source_json[1024];
    const char *archive_rel = (options->archive_path && options->archive_path[0] != '\0')
        ? options->archive_path
        : "tiles.mbtiles";
    const char *tile_store_kind = options->emit_archive ? "archive_indexed" : "filesystem_tree";
    const char *runtime_source_policy = options->emit_archive ? "archive_preferred" : "filesystem_only";
    const char *source_kind_label = osm_source_kind_label(ctx->source_kind_detected);
    const char *source_ingest_mode = ctx->source_is_canonical_pbf
        ? "canonical_pbf"
        : (ctx->source_compat_xml_mode ? "compat_xml" : "unknown");
    char tile_store_archive_line[1200];
    char archive_rollups_json[8192];
    char tile_coverage_json[65536];
    char source_hash_json[64];
    char source_mtime_json[64];
    char converter_program_json[MAPFORGE_SOURCE_PATH_CAPACITY + 4];
    if (utc) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc);
    }
    if (options->dem_path && options->dem_path[0] != '\0') {
        snprintf(terrain_source_json, sizeof(terrain_source_json), "\"%s\"", options->dem_path);
    } else {
        snprintf(terrain_source_json, sizeof(terrain_source_json), "null");
    }
    if (options->emit_archive) {
        snprintf(tile_store_archive_line,
                 sizeof(tile_store_archive_line),
                 ",\n"
                 "        \"archive_path\": \"%s\"",
                 archive_rel);
    } else {
        tile_store_archive_line[0] = '\0';
    }
    if (!format_archive_rollups_json(ctx, archive_rollups_json, sizeof(archive_rollups_json))) {
        return false;
    }
    if (!format_tile_coverage_json(ctx, tile_coverage_json, sizeof(tile_coverage_json))) {
        return false;
    }
    if (ctx->source_fingerprint_ok) {
        snprintf(source_hash_json, sizeof(source_hash_json), "\"0x%016llx\"", (unsigned long long)ctx->source_hash_fnv1a64);
    } else {
        snprintf(source_hash_json, sizeof(source_hash_json), "null");
    }
    if (ctx->source_mtime_ok) {
        snprintf(source_mtime_json, sizeof(source_mtime_json), "%llu", (unsigned long long)ctx->source_mtime_unix);
    } else {
        snprintf(source_mtime_json, sizeof(source_mtime_json), "null");
    }
    if (ctx->source_converter_program[0] != '\0') {
        snprintf(converter_program_json, sizeof(converter_program_json), "\"%s\"", ctx->source_converter_program);
    } else {
        snprintf(converter_program_json, sizeof(converter_program_json), "null");
    }

    char json[131072];
    int n = snprintf(
        json, sizeof(json),
        "{\n"
        "    \"region\": \"%s\",\n"
        "    \"package_contract\": {\n"
        "        \"family\": \"map_forge.region_package\",\n"
        "        \"version\": 1,\n"
        "        \"tile_store_contract\": \"map_forge.tile_store.v1\"\n"
        "    },\n"
        "    \"build_manifest\": {\n"
        "        \"family\": \"map_forge.region_build_manifest\",\n"
        "        \"version\": 1,\n"
        "        \"canonical_input_format\": \"osm.pbf\",\n"
        "        \"detected_source_kind\": \"%s\",\n"
        "        \"ingest_mode\": \"%s\",\n"
        "        \"canonical_source\": %s,\n"
        "        \"compat_source_mode\": %s,\n"
        "        \"source_size_bytes\": %llu,\n"
        "        \"source_hash_fnv1a64\": %s,\n"
        "        \"source_mtime_unix\": %s,\n"
        "        \"pbf_conversion_used\": %s,\n"
        "        \"converter_program\": %s,\n"
        "        \"archive_target_enabled\": %s,\n"
        "        \"archive_target_path\": \"%s\",\n"
        "        \"runtime_source_policy\": \"%s\"\n"
        "    },\n"
        "    \"city_source\": \"%s\",\n"
        "    \"terrain_source\": %s,\n"
        "    \"created_utc\": \"%s\",\n"
        "    \"bounds\": {\n"
        "        \"min_lat\": %.8f,\n"
        "        \"min_lon\": %.8f,\n"
        "        \"max_lat\": %.8f,\n"
        "        \"max_lon\": %.8f\n"
        "    },\n"
        "    \"tile\": {\n"
        "        \"min_z\": %u,\n"
        "        \"max_z\": %u,\n"
        "        \"extent\": %u\n"
        "    },\n"
        "    \"tile_store\": {\n"
        "        \"kind\": \"%s\",\n"
        "        \"root\": \"tiles\",\n"
        "        \"runtime_source_policy\": \"%s\"%s\n"
        "    },\n"
        "    \"tile_pyramid\": {\n"
        "        \"roads\": {\n"
        "            \"enabled\": true,\n"
        "            \"bands\": {\n"
        "                \"coarse\": {\"label\": \"coarse\"},\n"
        "                \"mid\": {\"label\": \"mid\"},\n"
        "                \"fine\": {\"label\": \"fine\"}\n"
        "            }\n"
        "        },\n"
        "        \"buildings\": {\n"
        "            \"enabled\": true,\n"
        "            \"bands\": {\n"
        "                \"coarse\": {\"label\": \"coarse\", \"point_step\": 1},\n"
        "                \"mid\": {\"label\": \"mid\", \"point_step\": 1},\n"
        "                \"fine\": {\"label\": \"fine\", \"point_step\": 1}\n"
        "            }\n"
        "        }\n"
        "    },\n"
        "    \"contours\": {\n"
        "        \"enabled\": %s,\n"
        "        \"phase\": \"A_scaffold\",\n"
        "        \"interval_m\": 10,\n"
        "        \"major_every\": 5\n"
        "    },\n"
        "    \"build_options\": {\n"
        "        \"pad_bounds\": %s,\n"
        "        \"emit_contour_empty\": %s,\n"
        "        \"emit_legacy_tiles\": %s,\n"
        "        \"emit_archive\": %s,\n"
        "        \"archive_path\": \"%s\",\n"
        "        \"replace\": %s,\n"
        "        \"keep_old\": %u,\n"
        "        \"prune_days\": %u\n"
        "    },\n"
        "    \"output_stats\": {\n"
        "        \"tile_count\": %zu,\n"
        "        \"files_written_total\": %llu,\n"
        "        \"files_written_legacy\": %llu,\n"
        "        \"files_written_banded\": %llu,\n"
        "        \"files_written_contour\": %llu,\n"
        "        \"archive_tile_rows\": %llu,\n"
        "        \"archive_bytes_written\": %llu,\n"
        "        \"layers\": {\n"
        "            \"artery\": %llu,\n"
        "            \"local\": %llu,\n"
        "            \"water\": %llu,\n"
        "            \"park\": %llu,\n"
        "            \"landuse\": %llu,\n"
        "            \"building\": %llu\n"
        "        },\n"
        "        \"bands\": {\n"
        "            \"coarse\": %llu,\n"
        "            \"mid\": %llu,\n"
        "            \"fine\": %llu\n"
        "        },\n"
        "        \"building_bands\": {\n"
        "            \"coarse\": %llu,\n"
        "            \"mid\": %llu,\n"
        "            \"fine\": %llu\n"
        "        },\n"
        "        \"archive_rollups\": %s,\n"
        "        \"tile_coverage\": %s\n"
        "    }\n"
        "}\n",
        options->region,
        source_kind_label,
        source_ingest_mode,
        ctx->source_is_canonical_pbf ? "true" : "false",
        ctx->source_compat_xml_mode ? "true" : "false",
        (unsigned long long)ctx->source_size_bytes,
        source_hash_json,
        source_mtime_json,
        ctx->source_pbf_conversion_used ? "true" : "false",
        converter_program_json,
        options->emit_archive ? "true" : "false",
        archive_rel,
        runtime_source_policy,
        options->osm_path,
        terrain_source_json,
        timestamp,
        ctx->min_lat,
        ctx->min_lon,
        ctx->max_lat,
        ctx->max_lon,
        options->min_z,
        options->max_z,
        (unsigned)TILE_EXTENT,
        tile_store_kind,
        runtime_source_policy,
        tile_store_archive_line,
        (options->dem_path && options->dem_path[0] != '\0') ? "true" : "false",
        options->pad_bounds ? "true" : "false",
        options->emit_contour_empty ? "true" : "false",
        options->emit_legacy_tiles ? "true" : "false",
        options->emit_archive ? "true" : "false",
        archive_rel,
        options->replace ? "true" : "false",
        options->keep_old,
        options->prune_days,
        ctx->tile_count,
        (unsigned long long)ctx->files_written_total,
        (unsigned long long)ctx->files_written_legacy,
        (unsigned long long)ctx->files_written_banded,
        (unsigned long long)ctx->files_written_contour,
        (unsigned long long)ctx->archive_tile_rows,
        (unsigned long long)ctx->archive_bytes_written,
        (unsigned long long)ctx->layer_artery_files,
        (unsigned long long)ctx->layer_local_files,
        (unsigned long long)ctx->layer_water_files,
        (unsigned long long)ctx->layer_park_files,
        (unsigned long long)ctx->layer_landuse_files,
        (unsigned long long)ctx->layer_building_files,
        (unsigned long long)ctx->band_coarse_files,
        (unsigned long long)ctx->band_mid_files,
        (unsigned long long)ctx->band_fine_files,
        (unsigned long long)ctx->building_band_coarse_files,
        (unsigned long long)ctx->building_band_mid_files,
        (unsigned long long)ctx->building_band_fine_files,
        archive_rollups_json,
        tile_coverage_json);
    if (n <= 0 || (size_t)n >= sizeof(json)) {
        return false;
    }

    return core_io_write_all(path, json, (size_t)n).code == CORE_OK;
}
