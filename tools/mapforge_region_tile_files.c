#include "mapforge_region_internal.h"

static TileZoomBand road_band_for_tile_z(uint16_t z, uint16_t min_z, uint16_t max_z) {
    if (max_z <= min_z) {
        return TILE_BAND_FINE;
    }
    uint16_t span = (uint16_t)(max_z - min_z);
    uint16_t coarse_max = (uint16_t)(min_z + (span / 3u));
    uint16_t mid_max = (uint16_t)(min_z + ((2u * span) / 3u));
    if (z <= coarse_max) {
        return TILE_BAND_COARSE;
    }
    if (z <= mid_max) {
        return TILE_BAND_MID;
    }
    return TILE_BAND_FINE;
}

static const char *road_band_label(TileZoomBand band) {
    switch (band) {
        case TILE_BAND_COARSE:
            return "coarse";
        case TILE_BAND_MID:
            return "mid";
        case TILE_BAND_FINE:
            return "fine";
        case TILE_BAND_DEFAULT:
        default:
            return "default";
    }
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

static bool ensure_tile_path_from_root(const char *root_dir, TileCoord coord, const char *suffix, char *out_path, size_t out_size) {
    if (!root_dir || !out_path || out_size == 0) {
        return false;
    }
    if (!suffix) {
        suffix = "mft";
    }

    char z_dir[512];
    snprintf(z_dir, sizeof(z_dir), "%s/%u", root_dir, coord.z);
    if (!ensure_dir(z_dir)) {
        return false;
    }

    char x_dir[512];
    snprintf(x_dir, sizeof(x_dir), "%s/%u", z_dir, coord.x);
    if (!ensure_dir(x_dir)) {
        return false;
    }

    snprintf(out_path, out_size, "%s/%u.%s", x_dir, coord.y, suffix);
    return true;
}

static bool ensure_tile_path(const char *base_dir, TileCoord coord, const char *suffix, char *out_path, size_t out_size) {
    if (!base_dir || !out_path || out_size == 0) {
        return false;
    }
    char tiles_dir[512];
    snprintf(tiles_dir, sizeof(tiles_dir), "%s/tiles", base_dir);
    if (!ensure_dir(base_dir) || !ensure_dir(tiles_dir)) {
        return false;
    }
    return ensure_tile_path_from_root(tiles_dir, coord, suffix, out_path, out_size);
}

static bool ensure_band_root_dir(const char *base_dir, const char *band_label, char *out_dir, size_t out_size) {
    if (!base_dir || !band_label || !out_dir || out_size == 0u) {
        return false;
    }
    char tiles_dir[512];
    char bands_dir[512];
    snprintf(tiles_dir, sizeof(tiles_dir), "%s/tiles", base_dir);
    snprintf(bands_dir, sizeof(bands_dir), "%s/bands", tiles_dir);
    snprintf(out_dir, out_size, "%s/%s", bands_dir, band_label);
    return ensure_dir(base_dir) && ensure_dir(tiles_dir) && ensure_dir(bands_dir) && ensure_dir(out_dir);
}

static bool road_class_is_artery(RoadClass road_class) {
    return road_class == ROAD_CLASS_MOTORWAY ||
        road_class == ROAD_CLASS_TRUNK ||
        road_class == ROAD_CLASS_PRIMARY ||
        road_class == ROAD_CLASS_SECONDARY;
}

static int metric_band_index_from_tile_band(TileZoomBand band) {
    if ((int)band < 0 || band >= TILE_BAND_COUNT) {
        return -1;
    }
    return (int)band;
}

static void record_tile_coverage(BuildContext *ctx,
                                 const char *suffix,
                                 TileZoomBand band,
                                 TileCoord coord) {
    if (!ctx || !suffix) {
        return;
    }
    if (coord.z > MAPFORGE_TILE_COVERAGE_MAX_ZOOM) {
        return;
    }
    int band_index = metric_band_index_from_tile_band(band);
    int layer_index = archive_metric_layer_index(archive_layer_from_suffix(suffix));
    if (band_index < 0 || layer_index < 0) {
        return;
    }

    ctx->coverage_total_tiles += 1u;
    ctx->coverage_tiles[band_index][layer_index] += 1u;

    if (!ctx->coverage_has_zoom[band_index][layer_index][coord.z]) {
        ctx->coverage_has_zoom[band_index][layer_index][coord.z] = true;
        ctx->coverage_zoom_min_x[band_index][layer_index][coord.z] = coord.x;
        ctx->coverage_zoom_max_x[band_index][layer_index][coord.z] = coord.x;
        ctx->coverage_zoom_min_y[band_index][layer_index][coord.z] = coord.y;
        ctx->coverage_zoom_max_y[band_index][layer_index][coord.z] = coord.y;
        ctx->coverage_zoom_tiles[band_index][layer_index][coord.z] = 1u;
        return;
    }

    if (coord.x < ctx->coverage_zoom_min_x[band_index][layer_index][coord.z]) {
        ctx->coverage_zoom_min_x[band_index][layer_index][coord.z] = coord.x;
    }
    if (coord.x > ctx->coverage_zoom_max_x[band_index][layer_index][coord.z]) {
        ctx->coverage_zoom_max_x[band_index][layer_index][coord.z] = coord.x;
    }
    if (coord.y < ctx->coverage_zoom_min_y[band_index][layer_index][coord.z]) {
        ctx->coverage_zoom_min_y[band_index][layer_index][coord.z] = coord.y;
    }
    if (coord.y > ctx->coverage_zoom_max_y[band_index][layer_index][coord.z]) {
        ctx->coverage_zoom_max_y[band_index][layer_index][coord.z] = coord.y;
    }
    if (ctx->coverage_zoom_tiles[band_index][layer_index][coord.z] < UINT32_MAX) {
        ctx->coverage_zoom_tiles[band_index][layer_index][coord.z] += 1u;
    }
}

static void record_file_write(BuildContext *ctx,
                              const char *suffix,
                              bool is_legacy,
                              TileZoomBand band,
                              TileCoord coord) {
    if (!ctx || !suffix) {
        return;
    }

    ctx->files_written_total += 1u;
    if (is_legacy) {
        ctx->files_written_legacy += 1u;
    } else {
        ctx->files_written_banded += 1u;
    }

    if (strcmp(suffix, "artery.mft") == 0) {
        ctx->layer_artery_files += 1u;
    } else if (strcmp(suffix, "local.mft") == 0) {
        ctx->layer_local_files += 1u;
    } else if (strcmp(suffix, "water.mft") == 0) {
        ctx->layer_water_files += 1u;
    } else if (strcmp(suffix, "park.mft") == 0) {
        ctx->layer_park_files += 1u;
    } else if (strcmp(suffix, "landuse.mft") == 0) {
        ctx->layer_landuse_files += 1u;
    } else if (strcmp(suffix, "building.mft") == 0) {
        ctx->layer_building_files += 1u;
    } else if (strcmp(suffix, "contour.mft") == 0) {
        ctx->files_written_contour += 1u;
    }

    if (!is_legacy) {
        if (band == TILE_BAND_COARSE) {
            ctx->band_coarse_files += 1u;
        } else if (band == TILE_BAND_MID) {
            ctx->band_mid_files += 1u;
        } else if (band == TILE_BAND_FINE) {
            ctx->band_fine_files += 1u;
        }
        if (strcmp(suffix, "building.mft") == 0) {
            if (band == TILE_BAND_COARSE) {
                ctx->building_band_coarse_files += 1u;
            } else if (band == TILE_BAND_MID) {
                ctx->building_band_mid_files += 1u;
            } else if (band == TILE_BAND_FINE) {
                ctx->building_band_fine_files += 1u;
            }
        }
    }
    record_tile_coverage(ctx, suffix, is_legacy ? TILE_BAND_DEFAULT : band, coord);
}

static bool write_tile_file_roads_at_root(const char *root_dir, const TileOutput *tile, const char *suffix, bool want_artery) {
    if (!root_dir || !tile || !suffix) {
        return false;
    }

    char path[512];
    if (!ensure_tile_path_from_root(root_dir, tile->coord, suffix, path, sizeof(path))) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    uint32_t polyline_count = 0;
    for (uint32_t i = 0; i < tile->polyline_count; ++i) {
        bool is_artery = road_class_is_artery(tile->polylines[i].road_class);
        if (is_artery == want_artery) {
            polyline_count += 1;
        }
    }

    const char magic[4] = {'M', 'F', 'T', '1'};
    uint16_t version = 2;
    uint16_t z = tile->coord.z;
    uint32_t x = tile->coord.x;
    uint32_t y = tile->coord.y;
    uint32_t polygon_count = 0;

    fwrite(magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(uint16_t), 1, file);
    fwrite(&z, sizeof(uint16_t), 1, file);
    fwrite(&x, sizeof(uint32_t), 1, file);
    fwrite(&y, sizeof(uint32_t), 1, file);
    fwrite(&polyline_count, sizeof(uint32_t), 1, file);

    for (uint32_t i = 0; i < tile->polyline_count; ++i) {
        const TilePolyline *polyline = &tile->polylines[i];
        bool is_artery = road_class_is_artery(polyline->road_class);
        if (is_artery != want_artery) {
            continue;
        }
        uint8_t road_class = (uint8_t)polyline->road_class;
        fwrite(&road_class, sizeof(uint8_t), 1, file);
        fwrite(&polyline->point_count, sizeof(uint32_t), 1, file);
    }

    for (uint32_t i = 0; i < tile->polyline_count; ++i) {
        const TilePolyline *polyline = &tile->polylines[i];
        bool is_artery = road_class_is_artery(polyline->road_class);
        if (is_artery != want_artery) {
            continue;
        }
        fwrite(polyline->points, sizeof(uint16_t), polyline->point_count * 2, file);
    }

    fwrite(&polygon_count, sizeof(uint32_t), 1, file);
    fclose(file);
    return true;
}

static bool write_tile_file_roads(const char *base_dir,
                                  BuildContext *ctx,
                                  const TileOutput *tile,
                                  const BuildOptions *options,
                                  const char *suffix,
                                  bool want_artery) {
    if (!base_dir || !tile || !options || !suffix) {
        return false;
    }
    char legacy_root[512];
    snprintf(legacy_root, sizeof(legacy_root), "%s/tiles", base_dir);
    if (!ensure_dir(base_dir)) {
        return false;
    }
    if (options->emit_legacy_tiles) {
        if (!ensure_dir(legacy_root)) {
            return false;
        }
        if (!write_tile_file_roads_at_root(legacy_root, tile, suffix, want_artery)) {
            return false;
        }
        record_file_write(ctx, suffix, true, TILE_BAND_DEFAULT, tile->coord);
    }
    TileZoomBand band = road_band_for_tile_z(tile->coord.z, options->min_z, options->max_z);
    const char *band_label = road_band_label(band);
    char band_root[512];
    if (!ensure_band_root_dir(base_dir, band_label, band_root, sizeof(band_root))) {
        return false;
    }
    if (!write_tile_file_roads_at_root(band_root, tile, suffix, want_artery)) {
        return false;
    }
    record_file_write(ctx, suffix, false, band, tile->coord);
    return true;
}

static bool write_empty_tile_file(const char *base_dir, TileCoord coord, const char *suffix) {
    if (!base_dir || !suffix) {
        return false;
    }

    char path[512];
    if (!ensure_tile_path(base_dir, coord, suffix, path, sizeof(path))) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    const char magic[4] = {'M', 'F', 'T', '1'};
    uint16_t version = 2;
    uint16_t z = coord.z;
    uint32_t x = coord.x;
    uint32_t y = coord.y;
    uint32_t polyline_count = 0;
    uint32_t polygon_count = 0;

    fwrite(magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(uint16_t), 1, file);
    fwrite(&z, sizeof(uint16_t), 1, file);
    fwrite(&x, sizeof(uint32_t), 1, file);
    fwrite(&y, sizeof(uint32_t), 1, file);
    fwrite(&polyline_count, sizeof(uint32_t), 1, file);
    fwrite(&polygon_count, sizeof(uint32_t), 1, file);

    fclose(file);
    return true;
}

static uint32_t polygon_point_step_for_band(PolygonClass polygon_class, TileZoomBand band) {
    if (polygon_class != POLYGON_CLASS_BUILDING) {
        return 1u;
    }
    (void)band;
    return 1u;
}

static uint32_t polygon_reduced_point_count(const TilePolygon *polygon, PolygonClass polygon_class, TileZoomBand band) {
    if (!polygon) {
        return 0u;
    }
    uint32_t count = polygon->point_count;
    uint32_t step = polygon_point_step_for_band(polygon_class, band);
    if (step <= 1u || count <= 3u) {
        return count;
    }
    uint32_t reduced = (count + step - 1u) / step;
    if (reduced < 3u) {
        reduced = 3u;
    }
    if (reduced > count) {
        reduced = count;
    }
    return reduced;
}

static bool write_tile_file_polygons_at_root(const char *root_dir,
                                             const TileOutput *tile,
                                             const char *suffix,
                                             PolygonClass polygon_class,
                                             TileZoomBand band) {
    if (!root_dir || !tile || !suffix) {
        return false;
    }

    char path[512];
    if (!ensure_tile_path_from_root(root_dir, tile->coord, suffix, path, sizeof(path))) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    const char magic[4] = {'M', 'F', 'T', '1'};
    uint16_t version = (polygon_class == POLYGON_CLASS_WATER) ? 3 : 2;
    uint16_t z = tile->coord.z;
    uint32_t x = tile->coord.x;
    uint32_t y = tile->coord.y;
    uint32_t polyline_count = 0;
    uint32_t polygon_count = 0;
    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        if (tile->polygons[i].polygon_class == polygon_class) {
            polygon_count += 1;
        }
    }

    fwrite(magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(uint16_t), 1, file);
    fwrite(&z, sizeof(uint16_t), 1, file);
    fwrite(&x, sizeof(uint32_t), 1, file);
    fwrite(&y, sizeof(uint32_t), 1, file);
    fwrite(&polyline_count, sizeof(uint32_t), 1, file);

    fwrite(&polygon_count, sizeof(uint32_t), 1, file);
    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const TilePolygon *polygon = &tile->polygons[i];
        if (polygon->polygon_class != polygon_class) {
            continue;
        }
        uint8_t polygon_class_value = (uint8_t)polygon->polygon_class;
        uint16_t ring_count = 1;
        fwrite(&polygon_class_value, sizeof(uint8_t), 1, file);
        fwrite(&ring_count, sizeof(uint16_t), 1, file);
    }

    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const TilePolygon *polygon = &tile->polygons[i];
        if (polygon->polygon_class != polygon_class) {
            continue;
        }
        uint32_t reduced = polygon_reduced_point_count(polygon, polygon_class, band);
        fwrite(&reduced, sizeof(uint32_t), 1, file);
    }

    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const TilePolygon *polygon = &tile->polygons[i];
        if (polygon->polygon_class != polygon_class) {
            continue;
        }
        uint32_t count = polygon->point_count;
        uint32_t reduced = polygon_reduced_point_count(polygon, polygon_class, band);
        uint32_t step = polygon_point_step_for_band(polygon_class, band);
        if (step <= 1u || count <= 3u || reduced >= count) {
            fwrite(polygon->points, sizeof(uint16_t), count * 2u, file);
            continue;
        }
        uint32_t written = 0u;
        for (uint32_t p = 0u; p < count && written < reduced; p += step) {
            const uint16_t *src = &polygon->points[p * 2u];
            fwrite(src, sizeof(uint16_t), 2u, file);
            written += 1u;
        }
        while (written < reduced) {
            const uint16_t *src = &polygon->points[(count - 1u) * 2u];
            fwrite(src, sizeof(uint16_t), 2u, file);
            written += 1u;
        }
    }

    if (polygon_class == POLYGON_CLASS_WATER && polygon_count > 0u) {
        static const uint32_t kVariantSteps[MFT_WATER_VARIANT_COUNT] = {4u, 2u, 1u};
        const char ext_magic[4] = {'W', 'S', 'M', 'P'};
        uint8_t variant_count = MFT_WATER_VARIANT_COUNT;
        fwrite(ext_magic, sizeof(ext_magic), 1, file);
        fwrite(&variant_count, sizeof(uint8_t), 1, file);

        for (uint32_t v = 0; v < MFT_WATER_VARIANT_COUNT; ++v) {
            uint32_t step = kVariantSteps[v];
            for (uint32_t i = 0; i < tile->polygon_count; ++i) {
                const TilePolygon *polygon = &tile->polygons[i];
                if (polygon->polygon_class != polygon_class) {
                    continue;
                }
                uint32_t count = polygon->point_count;
                uint32_t reduced = count;
                if (step > 1u && count > 3u) {
                    reduced = (count + step - 1u) / step;
                    if (reduced < 3u) {
                        reduced = 3u;
                    }
                }
                fwrite(&reduced, sizeof(uint32_t), 1, file);
            }
            for (uint32_t i = 0; i < tile->polygon_count; ++i) {
                const TilePolygon *polygon = &tile->polygons[i];
                if (polygon->polygon_class != polygon_class) {
                    continue;
                }
                uint32_t count = polygon->point_count;
                uint32_t reduced = count;
                if (step > 1u && count > 3u) {
                    reduced = (count + step - 1u) / step;
                    if (reduced < 3u) {
                        reduced = 3u;
                    }
                }
                if (step <= 1u || count <= 3u || reduced >= count) {
                    fwrite(polygon->points, sizeof(uint16_t), count * 2u, file);
                    continue;
                }
                uint32_t written = 0u;
                for (uint32_t p = 0u; p < count && written < reduced; p += step) {
                    const uint16_t *src = &polygon->points[p * 2u];
                    fwrite(src, sizeof(uint16_t), 2u, file);
                    written += 1u;
                }
                while (written < reduced) {
                    const uint16_t *src = &polygon->points[(count - 1u) * 2u];
                    fwrite(src, sizeof(uint16_t), 2u, file);
                    written += 1u;
                }
            }
        }
    }

    fclose(file);
    return true;
}

static bool write_tile_file_polygons(const char *base_dir,
                                     BuildContext *ctx,
                                     const TileOutput *tile,
                                     const BuildOptions *options,
                                     const char *suffix,
                                     PolygonClass polygon_class) {
    if (!base_dir || !tile || !options || !suffix) {
        return false;
    }
    char legacy_root[512];
    snprintf(legacy_root, sizeof(legacy_root), "%s/tiles", base_dir);
    if (!ensure_dir(base_dir)) {
        return false;
    }
    if (options->emit_legacy_tiles) {
        if (!ensure_dir(legacy_root)) {
            return false;
        }
        if (!write_tile_file_polygons_at_root(legacy_root, tile, suffix, polygon_class, TILE_BAND_DEFAULT)) {
            return false;
        }
        record_file_write(ctx, suffix, true, TILE_BAND_DEFAULT, tile->coord);
    }

    bool band_enabled = (polygon_class == POLYGON_CLASS_WATER ||
                         polygon_class == POLYGON_CLASS_PARK ||
                         polygon_class == POLYGON_CLASS_LANDUSE ||
                         polygon_class == POLYGON_CLASS_BUILDING);
    if (!band_enabled) {
        return true;
    }

    TileZoomBand band = road_band_for_tile_z(tile->coord.z, options->min_z, options->max_z);
    const char *band_label = road_band_label(band);
    char band_root[512];
    if (!ensure_band_root_dir(base_dir, band_label, band_root, sizeof(band_root))) {
        return false;
    }
    if (!write_tile_file_polygons_at_root(band_root, tile, suffix, polygon_class, band)) {
        return false;
    }
    record_file_write(ctx, suffix, false, band, tile->coord);
    return true;
}

bool mapforge_region_write_tile_file(const BuildOptions *options, BuildContext *ctx, const TileOutput *tile) {
    if (!options || !options->out_dir || !ctx || !tile) {
        return false;
    }

    if (!write_tile_file_roads(options->out_dir, ctx, tile, options, "artery.mft", true)) {
        return false;
    }
    if (!write_tile_file_roads(options->out_dir, ctx, tile, options, "local.mft", false)) {
        return false;
    }
    if (!write_tile_file_polygons(options->out_dir, ctx, tile, options, "water.mft", POLYGON_CLASS_WATER)) {
        return false;
    }
    if (!write_tile_file_polygons(options->out_dir, ctx, tile, options, "park.mft", POLYGON_CLASS_PARK)) {
        return false;
    }
    if (!write_tile_file_polygons(options->out_dir, ctx, tile, options, "landuse.mft", POLYGON_CLASS_LANDUSE)) {
        return false;
    }
    if (!write_tile_file_polygons(options->out_dir, ctx, tile, options, "building.mft", POLYGON_CLASS_BUILDING)) {
        return false;
    }
    if (options->emit_contour_empty) {
        if (!write_empty_tile_file(options->out_dir, tile->coord, "contour.mft")) {
            return false;
        }
        record_file_write(ctx, "contour.mft", true, TILE_BAND_DEFAULT, tile->coord);
    }
    return true;
}
