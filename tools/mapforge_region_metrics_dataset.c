#include "mapforge_region_internal.h"

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

bool mapforge_region_write_metrics_dataset_json(const BuildOptions *options, const BuildContext *ctx) {
    if (!options || !ctx || !options->out_dir) {
        return false;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/meta.dataset.json", options->out_dir);

    CoreDataset dataset;
    core_dataset_init(&dataset);
    char source_hash_hex[32];
    char source_mtime_json[64];
    const char *source_kind = osm_source_kind_label(ctx->source_kind_detected);

    CoreResult r = core_dataset_add_metadata_string(&dataset, "profile", "map_forge_tile_layer_feature_dataset_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "dataset_schema", "map_forge.tile_layer_feature_metrics");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "schema_family", "map_forge_diagnostics");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "schema_variant", "tile_layer_feature_metrics_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(&dataset, "dataset_contract_version", 1);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(&dataset, "schema_version", 1);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "summary_table", "map_forge_summary_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "layers_table", "map_forge_layers_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "bands_table", "map_forge_bands_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "archive_rollups_table", "map_forge_archive_rollups_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "region", options->region ? options->region : "");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "canonical_input_format", "osm.pbf");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "detected_source_kind", source_kind);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "ingest_mode",
                                         ctx->source_is_canonical_pbf ? "canonical_pbf"
                                                                      : (ctx->source_compat_xml_mode ? "compat_xml" : "unknown"));
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "pbf_conversion_used", ctx->source_pbf_conversion_used ? "true" : "false");
    if (r.code != CORE_OK) goto fail;
    if (ctx->source_fingerprint_ok) {
        snprintf(source_hash_hex, sizeof(source_hash_hex), "0x%016llx", (unsigned long long)ctx->source_hash_fnv1a64);
    } else {
        snprintf(source_hash_hex, sizeof(source_hash_hex), "unknown");
    }
    if (ctx->source_mtime_ok) {
        snprintf(source_mtime_json, sizeof(source_mtime_json), "%llu", (unsigned long long)ctx->source_mtime_unix);
    } else {
        snprintf(source_mtime_json, sizeof(source_mtime_json), "null");
    }
    r = core_dataset_add_metadata_string(&dataset, "source_hash_fnv1a64", source_hash_hex);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(&dataset, "source_size_bytes", (int64_t)ctx->source_size_bytes);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(&dataset, "source_mtime_unix",
                                      ctx->source_mtime_ok ? (int64_t)ctx->source_mtime_unix : -1);
    if (r.code != CORE_OK) goto fail;

    r = core_dataset_add_scalar_f64(&dataset, "tile_count", (double)ctx->tile_count);
    if (r.code != CORE_OK) goto fail;

    {
        const char *cols[] = {
            "tile_count", "files_written_total", "files_written_legacy", "files_written_banded",
            "files_written_contour", "archive_tile_rows", "archive_bytes_written",
            "emit_legacy_tiles", "emit_contour_empty", "min_z", "max_z"
        };
        CoreTableColumnType types[] = {
            CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64,
            CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64,
            CORE_TABLE_COL_BOOL, CORE_TABLE_COL_BOOL, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64
        };
        int64_t tile_count_col[] = {(int64_t)ctx->tile_count};
        int64_t files_total_col[] = {(int64_t)ctx->files_written_total};
        int64_t files_legacy_col[] = {(int64_t)ctx->files_written_legacy};
        int64_t files_banded_col[] = {(int64_t)ctx->files_written_banded};
        int64_t files_contour_col[] = {(int64_t)ctx->files_written_contour};
        int64_t archive_rows_col[] = {(int64_t)ctx->archive_tile_rows};
        int64_t archive_bytes_col[] = {(int64_t)ctx->archive_bytes_written};
        bool emit_legacy_col[] = {options->emit_legacy_tiles};
        bool emit_contour_col[] = {options->emit_contour_empty};
        int64_t min_z_col[] = {(int64_t)options->min_z};
        int64_t max_z_col[] = {(int64_t)options->max_z};
        const void *col_data[] = {
            tile_count_col, files_total_col, files_legacy_col, files_banded_col, files_contour_col,
            archive_rows_col, archive_bytes_col,
            emit_legacy_col, emit_contour_col, min_z_col, max_z_col
        };
        r = core_dataset_add_table_typed(&dataset, "map_forge_summary_v1", cols, types,
                                         (uint32_t)(sizeof(cols) / sizeof(cols[0])), 1u, col_data);
        if (r.code != CORE_OK) goto fail;
    }

    {
        const char *cols[] = {"layer_id", "files_written"};
        CoreTableColumnType types[] = {CORE_TABLE_COL_I64, CORE_TABLE_COL_I64};
        int64_t layer_id_col[] = {1, 2, 3, 4, 5, 6};
        int64_t files_col[] = {
            (int64_t)ctx->layer_artery_files,
            (int64_t)ctx->layer_local_files,
            (int64_t)ctx->layer_water_files,
            (int64_t)ctx->layer_park_files,
            (int64_t)ctx->layer_landuse_files,
            (int64_t)ctx->layer_building_files};
        const void *col_data[] = {layer_id_col, files_col};
        r = core_dataset_add_table_typed(&dataset, "map_forge_layers_v1", cols, types,
                                         (uint32_t)(sizeof(cols) / sizeof(cols[0])), 6u, col_data);
        if (r.code != CORE_OK) goto fail;
    }

    {
        const char *cols[] = {"band_id", "files_written"};
        CoreTableColumnType types[] = {CORE_TABLE_COL_I64, CORE_TABLE_COL_I64};
        int64_t band_id_col[] = {1, 2, 3};
        int64_t files_col[] = {
            (int64_t)ctx->band_coarse_files,
            (int64_t)ctx->band_mid_files,
            (int64_t)ctx->band_fine_files};
        const void *col_data[] = {band_id_col, files_col};
        r = core_dataset_add_table_typed(&dataset, "map_forge_bands_v1", cols, types,
                                         (uint32_t)(sizeof(cols) / sizeof(cols[0])), 3u, col_data);
        if (r.code != CORE_OK) goto fail;
    }

    {
        const uint32_t rows = (uint32_t)(METRIC_BAND_COUNT * METRIC_LAYER_COUNT);
        const char *cols[] = {"band_id", "layer_id", "tile_rows", "bytes"};
        CoreTableColumnType types[] = {CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64};
        int64_t band_id_col[METRIC_BAND_COUNT * METRIC_LAYER_COUNT];
        int64_t layer_id_col[METRIC_BAND_COUNT * METRIC_LAYER_COUNT];
        int64_t tile_rows_col[METRIC_BAND_COUNT * METRIC_LAYER_COUNT];
        int64_t bytes_col[METRIC_BAND_COUNT * METRIC_LAYER_COUNT];
        uint32_t idx = 0u;
        for (int b = 0; b < METRIC_BAND_COUNT; ++b) {
            for (int l = 0; l < METRIC_LAYER_COUNT; ++l) {
                band_id_col[idx] = (int64_t)(b + 1);
                layer_id_col[idx] = (int64_t)(l + 1);
                tile_rows_col[idx] = (int64_t)ctx->archive_rollup_rows[b][l];
                bytes_col[idx] = (int64_t)ctx->archive_rollup_bytes[b][l];
                idx += 1u;
            }
        }
        const void *col_data[] = {band_id_col, layer_id_col, tile_rows_col, bytes_col};
        r = core_dataset_add_table_typed(&dataset, "map_forge_archive_rollups_v1", cols, types,
                                         (uint32_t)(sizeof(cols) / sizeof(cols[0])), rows, col_data);
        if (r.code != CORE_OK) goto fail;
    }

    if (!core_dataset_find(&dataset, "map_forge_summary_v1") ||
        !core_dataset_find(&dataset, "map_forge_layers_v1") ||
        !core_dataset_find(&dataset, "map_forge_bands_v1") ||
        !core_dataset_find(&dataset, "map_forge_archive_rollups_v1")) {
        goto fail;
    }

    char json[16384];
    int n = snprintf(
        json, sizeof(json),
        "{\n"
        "  \"profile\": \"map_forge_tile_layer_feature_dataset_v1\",\n"
        "  \"dataset_schema\": \"map_forge.tile_layer_feature_metrics\",\n"
        "  \"schema_family\": \"map_forge_diagnostics\",\n"
        "  \"schema_variant\": \"tile_layer_feature_metrics_v1\",\n"
        "  \"dataset_contract_version\": 1,\n"
        "  \"schema_version\": 1,\n"
        "  \"metadata\": {\n"
        "    \"region\": \"%s\",\n"
        "    \"summary_table\": \"map_forge_summary_v1\",\n"
        "    \"layers_table\": \"map_forge_layers_v1\",\n"
        "    \"bands_table\": \"map_forge_bands_v1\",\n"
        "    \"archive_rollups_table\": \"map_forge_archive_rollups_v1\",\n"
        "    \"canonical_input_format\": \"osm.pbf\",\n"
        "    \"detected_source_kind\": \"%s\",\n"
        "    \"ingest_mode\": \"%s\",\n"
        "    \"pbf_conversion_used\": %s,\n"
        "    \"source_hash_fnv1a64\": \"%s\",\n"
        "    \"source_size_bytes\": %llu,\n"
        "    \"source_mtime_unix\": %s\n"
        "  },\n"
        "  \"items\": [\n"
        "    {\n"
        "      \"name\": \"map_forge_summary_v1\",\n"
        "      \"kind\": \"table_typed\",\n"
        "      \"rows\": 1,\n"
        "      \"columns\": 11,\n"
        "      \"row0\": {\n"
        "        \"tile_count\": %lld,\n"
        "        \"files_written_total\": %lld,\n"
        "        \"files_written_legacy\": %lld,\n"
        "        \"files_written_banded\": %lld,\n"
        "        \"files_written_contour\": %lld,\n"
        "        \"archive_tile_rows\": %lld,\n"
        "        \"archive_bytes_written\": %lld,\n"
        "        \"emit_legacy_tiles\": %s,\n"
        "        \"emit_contour_empty\": %s,\n"
        "        \"min_z\": %u,\n"
        "        \"max_z\": %u\n"
        "      }\n"
        "    },\n"
        "    {\n"
        "      \"name\": \"map_forge_layers_v1\",\n"
        "      \"kind\": \"table_typed\",\n"
        "      \"rows\": 6,\n"
        "      \"columns\": 2\n"
        "    },\n"
        "    {\n"
        "      \"name\": \"map_forge_bands_v1\",\n"
        "      \"kind\": \"table_typed\",\n"
        "      \"rows\": 3,\n"
        "      \"columns\": 2\n"
        "    },\n"
        "    {\n"
        "      \"name\": \"map_forge_archive_rollups_v1\",\n"
        "      \"kind\": \"table_typed\",\n"
        "      \"rows\": %u,\n"
        "      \"columns\": 4\n"
        "    }\n"
        "  ]\n"
        "}\n",
        options->region ? options->region : "",
        source_kind,
        ctx->source_is_canonical_pbf ? "canonical_pbf" : (ctx->source_compat_xml_mode ? "compat_xml" : "unknown"),
        ctx->source_pbf_conversion_used ? "true" : "false",
        source_hash_hex,
        (unsigned long long)ctx->source_size_bytes,
        ctx->source_mtime_ok ? source_mtime_json : "null",
        (long long)ctx->tile_count,
        (long long)ctx->files_written_total,
        (long long)ctx->files_written_legacy,
        (long long)ctx->files_written_banded,
        (long long)ctx->files_written_contour,
        (long long)ctx->archive_tile_rows,
        (long long)ctx->archive_bytes_written,
        options->emit_legacy_tiles ? "true" : "false",
        options->emit_contour_empty ? "true" : "false",
        (unsigned)options->min_z,
        (unsigned)options->max_z,
        (unsigned)(METRIC_BAND_COUNT * METRIC_LAYER_COUNT));
    if (n <= 0 || (size_t)n >= sizeof(json)) {
        goto fail;
    }

    r = core_io_write_all(path, json, (size_t)n);
    core_dataset_free(&dataset);
    return r.code == CORE_OK;

fail:
    core_dataset_free(&dataset);
    return false;
}
