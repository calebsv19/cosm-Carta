#include "mapforge_region_internal.h"

static void build_context_free(BuildContext *ctx) {
    if (!ctx) {
        return;
    }

    for (size_t i = 0; i < ctx->tile_count; ++i) {
        TileOutput *tile = &ctx->tiles[i];
        for (uint32_t p = 0; p < tile->polyline_count; ++p) {
            free(tile->polylines[p].points);
        }
        free(tile->polylines);
        for (uint32_t p = 0; p < tile->polygon_count; ++p) {
            free(tile->polygons[p].points);
        }
        free(tile->polygons);
    }

    free(ctx->tiles);
    memset(ctx, 0, sizeof(*ctx));
}

static void usage(void) {
    printf("mapforge_region --region <name> --osm <file.osm|file.osm.xml|file.osm.pbf|file.pbf> [--dem <file.dem>] --out <dir> [--min-z N] [--max-z N] [--replace] [--keep-old N] [--prune-days N] [--prune-dry-run] [--pad-bounds] [--emit-contour-empty] [--emit-legacy-tiles] [--emit-archive] [--archive-path <relpath>]\n");
}

static bool parse_args(int argc, char **argv, BuildOptions *options) {
    if (!options) {
        return false;
    }

    memset(options, 0, sizeof(*options));
    options->min_z = 12;
    options->max_z = 12;
    options->keep_old = 1u;
    options->prune_days = 30u;
    options->emit_legacy_tiles = true;
    options->archive_path = "tiles.mbtiles";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--region") == 0 && i + 1 < argc) {
            options->region = argv[++i];
        } else if (strcmp(argv[i], "--osm") == 0 && i + 1 < argc) {
            options->osm_path = argv[++i];
        } else if (strcmp(argv[i], "--dem") == 0 && i + 1 < argc) {
            options->dem_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            options->out_dir = argv[++i];
        } else if (strcmp(argv[i], "--min-z") == 0 && i + 1 < argc) {
            options->min_z = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--max-z") == 0 && i + 1 < argc) {
            options->max_z = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--replace") == 0) {
            options->replace = true;
        } else if (strcmp(argv[i], "--keep-old") == 0 && i + 1 < argc) {
            options->keep_old = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--prune-days") == 0 && i + 1 < argc) {
            options->prune_days = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--prune-dry-run") == 0) {
            options->prune_dry_run = true;
        } else if (strcmp(argv[i], "--pad-bounds") == 0) {
            options->pad_bounds = true;
        } else if (strcmp(argv[i], "--emit-contour-empty") == 0) {
            options->emit_contour_empty = true;
        } else if (strcmp(argv[i], "--emit-legacy-tiles") == 0) {
            options->emit_legacy_tiles = true;
        } else if (strcmp(argv[i], "--no-legacy-tiles") == 0) {
            options->emit_legacy_tiles = false;
        } else if (strcmp(argv[i], "--emit-archive") == 0) {
            options->emit_archive = true;
        } else if (strcmp(argv[i], "--archive-path") == 0 && i + 1 < argc) {
            options->archive_path = argv[++i];
            options->emit_archive = true;
        } else {
            return false;
        }
    }

    if (!options->region || !options->osm_path || !options->out_dir) {
        return false;
    }
    if (options->emit_archive && !mapforge_region_archive_rel_path_valid(options->archive_path)) {
        log_error("--archive-path must be a region-local relative path (no leading '/' and no '..'): %s",
                  options->archive_path ? options->archive_path : "");
        mapforge_region_log_diagnostic("archive_emit",
                                       "Use a region-local relative archive path such as tiles.mbtiles.");
        return false;
    }

    return true;
}

int main(int argc, char **argv) {
    BuildOptions options;
    char active_out_dir[512];
    char stage_dir[512];
    char snapshot_root[512];
    char staging_root[512];
    bool stage_created = false;

    if (!parse_args(argc, argv, &options)) {
        usage();
        return 1;
    }

    snprintf(active_out_dir, sizeof(active_out_dir), "%s", options.out_dir);
    if (!mapforge_region_build_publish_paths(active_out_dir,
                                             stage_dir, sizeof(stage_dir),
                                             snapshot_root, sizeof(snapshot_root),
                                             staging_root, sizeof(staging_root))) {
        log_error("Failed to build publish paths for output: %s", active_out_dir);
        return 1;
    }
    if (!mapforge_region_ensure_dir_recursive(staging_root)) {
        log_error("Failed to create staging root: %s", staging_root);
        return 1;
    }
    if (!options.replace && !mapforge_region_ensure_dir_recursive(snapshot_root)) {
        log_error("Failed to create snapshot root: %s", snapshot_root);
        return 1;
    }
    if (!mapforge_region_remove_tree(stage_dir)) {
        log_error("Failed to remove previous staging dir: %s", stage_dir);
        return 1;
    }
    if (!mapforge_region_ensure_dir_recursive(stage_dir)) {
        log_error("Failed to create stage dir: %s", stage_dir);
        return 1;
    }
    stage_created = true;
    options.out_dir = stage_dir;

    BuildContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    if (!mapforge_region_parse_osm(&options, &ctx)) {
        mapforge_region_log_diagnostic("source_ingest", NULL);
        build_context_free(&ctx);
        return 1;
    }

    if (options.pad_bounds) {
        mapforge_region_ensure_tiles_for_bounds(&ctx, &options);
    }

    if (ctx.tile_count == 0) {
        log_info("No tiles produced.");
        build_context_free(&ctx);
        if (stage_created) {
            mapforge_region_remove_tree(stage_dir);
        }
        return 0;
    }

    mapforge_region_sort_tiles(&ctx);

    for (size_t i = 0; i < ctx.tile_count; ++i) {
        if (!mapforge_region_write_tile_file(&options, &ctx, &ctx.tiles[i])) {
            log_error("Failed to write tile %u/%u/%u", ctx.tiles[i].coord.z, ctx.tiles[i].coord.x, ctx.tiles[i].coord.y);
        }
    }
    if (!mapforge_region_write_tile_archive_sqlite(&options, &ctx)) {
        log_error("Failed to emit staged archive payload for region: %s", options.region);
        mapforge_region_log_diagnostic("archive_emit", NULL);
        build_context_free(&ctx);
        if (stage_created) {
            mapforge_region_remove_tree(stage_dir);
        }
        return 1;
    }

    if (!mapforge_region_write_meta_json(&options, &ctx)) {
        log_error("Failed to write staged meta.json for region: %s", options.region);
        build_context_free(&ctx);
        if (stage_created) {
            mapforge_region_remove_tree(stage_dir);
        }
        return 1;
    }
    if (!mapforge_region_write_metrics_dataset_json(&options, &ctx)) {
        log_error("Failed to write staged meta.dataset.json for region: %s", options.region);
        build_context_free(&ctx);
        if (stage_created) {
            mapforge_region_remove_tree(stage_dir);
        }
        return 1;
    }
    build_context_free(&ctx);

    if (!mapforge_region_validate_staged_region(&options, stage_dir)) {
        mapforge_region_log_diagnostic("validation", NULL);
        if (stage_created) {
            mapforge_region_remove_tree(stage_dir);
        }
        return 1;
    }

    log_info("Publishing staged region pack: %s -> %s", stage_dir, active_out_dir);
    if (!mapforge_region_publish_region_pack(&options, stage_dir, active_out_dir, snapshot_root)) {
        mapforge_region_log_diagnostic("publish", NULL);
        if (stage_created) {
            mapforge_region_remove_tree(stage_dir);
        }
        return 1;
    }

    mapforge_region_prune_staging_dirs(staging_root, options.prune_days, options.prune_dry_run);
    log_info("Region pack generated at %s", active_out_dir);
    return 0;
}
