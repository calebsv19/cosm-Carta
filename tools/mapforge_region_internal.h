#ifndef MAPFORGE_REGION_INTERNAL_H
#define MAPFORGE_REGION_INTERNAL_H

#include "core/log.h"
#include "core_data.h"
#include "core_io.h"
#include "map/mercator.h"
#include "map/mft_loader.h"
#include "map/tile_layers.h"
#include "map/tile_math.h"

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(MAPFORGE_HAVE_SQLITE)
#include <sqlite3.h>
#endif

#define TILE_EXTENT 4096.0
#define MFT_WATER_VARIANT_COUNT 3
#define MAPFORGE_SOURCE_PATH_CAPACITY 1024
#define MAPFORGE_TILE_COVERAGE_MAX_ZOOM 30u

extern char **environ;

typedef enum OSMSourceKind {
    OSM_SOURCE_KIND_XML = 0,
    OSM_SOURCE_KIND_PBF = 1,
    OSM_SOURCE_KIND_UNKNOWN = 2
} OSMSourceKind;

typedef struct NodeEntry {
    int64_t id;
    double lat;
    double lon;
    bool used;
} NodeEntry;

typedef struct NodeMap {
    NodeEntry *entries;
    size_t capacity;
    size_t count;
} NodeMap;

typedef struct WayNodes {
    int64_t *items;
    size_t count;
    size_t capacity;
} WayNodes;

typedef struct TilePolyline {
    RoadClass road_class;
    uint32_t point_count;
    uint16_t *points;
} TilePolyline;

typedef struct TilePolygon {
    PolygonClass polygon_class;
    uint32_t point_count;
    uint16_t *points;
} TilePolygon;

typedef struct TileOutput {
    TileCoord coord;
    TilePolyline *polylines;
    uint32_t polyline_count;
    uint32_t polyline_capacity;
    TilePolygon *polygons;
    uint32_t polygon_count;
    uint32_t polygon_capacity;
} TileOutput;

enum {
    METRIC_BAND_DEFAULT = 0,
    METRIC_BAND_COARSE = 1,
    METRIC_BAND_MID = 2,
    METRIC_BAND_FINE = 3,
    METRIC_BAND_COUNT = 4
};

enum {
    METRIC_LAYER_ARTERY = 0,
    METRIC_LAYER_LOCAL = 1,
    METRIC_LAYER_WATER = 2,
    METRIC_LAYER_PARK = 3,
    METRIC_LAYER_LANDUSE = 4,
    METRIC_LAYER_BUILDING = 5,
    METRIC_LAYER_CONTOUR = 6,
    METRIC_LAYER_COUNT = 7
};

typedef struct BuildContext {
    TileOutput *tiles;
    size_t tile_count;
    size_t tile_capacity;
    double min_lat;
    double min_lon;
    double max_lat;
    double max_lon;
    bool has_bounds;
    uint64_t files_written_total;
    uint64_t files_written_legacy;
    uint64_t files_written_banded;
    uint64_t files_written_contour;
    uint64_t layer_artery_files;
    uint64_t layer_local_files;
    uint64_t layer_water_files;
    uint64_t layer_park_files;
    uint64_t layer_landuse_files;
    uint64_t layer_building_files;
    uint64_t building_band_coarse_files;
    uint64_t building_band_mid_files;
    uint64_t building_band_fine_files;
    uint64_t band_coarse_files;
    uint64_t band_mid_files;
    uint64_t band_fine_files;
    uint64_t archive_tile_rows;
    uint64_t archive_bytes_written;
    uint64_t archive_rollup_rows[METRIC_BAND_COUNT][METRIC_LAYER_COUNT];
    uint64_t archive_rollup_bytes[METRIC_BAND_COUNT][METRIC_LAYER_COUNT];
    uint64_t coverage_total_tiles;
    uint64_t coverage_tiles[METRIC_BAND_COUNT][METRIC_LAYER_COUNT];
    bool coverage_has_zoom[METRIC_BAND_COUNT][METRIC_LAYER_COUNT][MAPFORGE_TILE_COVERAGE_MAX_ZOOM + 1u];
    uint32_t coverage_zoom_min_x[METRIC_BAND_COUNT][METRIC_LAYER_COUNT][MAPFORGE_TILE_COVERAGE_MAX_ZOOM + 1u];
    uint32_t coverage_zoom_max_x[METRIC_BAND_COUNT][METRIC_LAYER_COUNT][MAPFORGE_TILE_COVERAGE_MAX_ZOOM + 1u];
    uint32_t coverage_zoom_min_y[METRIC_BAND_COUNT][METRIC_LAYER_COUNT][MAPFORGE_TILE_COVERAGE_MAX_ZOOM + 1u];
    uint32_t coverage_zoom_max_y[METRIC_BAND_COUNT][METRIC_LAYER_COUNT][MAPFORGE_TILE_COVERAGE_MAX_ZOOM + 1u];
    uint32_t coverage_zoom_tiles[METRIC_BAND_COUNT][METRIC_LAYER_COUNT][MAPFORGE_TILE_COVERAGE_MAX_ZOOM + 1u];
    OSMSourceKind source_kind_detected;
    bool source_is_canonical_pbf;
    bool source_compat_xml_mode;
    bool source_pbf_conversion_used;
    bool source_fingerprint_ok;
    bool source_mtime_ok;
    uint64_t source_size_bytes;
    uint64_t source_hash_fnv1a64;
    uint64_t source_mtime_unix;
    char source_converter_program[MAPFORGE_SOURCE_PATH_CAPACITY];
} BuildContext;

typedef struct BuildOptions {
    const char *region;
    const char *osm_path;
    const char *dem_path;
    const char *out_dir;
    uint16_t min_z;
    uint16_t max_z;
    bool replace;
    uint32_t keep_old;
    uint32_t prune_days;
    bool prune_dry_run;
    bool pad_bounds;
    bool emit_contour_empty;
    bool emit_legacy_tiles;
    bool emit_archive;
    const char *archive_path;
} BuildOptions;

bool mapforge_region_parse_osm(const BuildOptions *options, BuildContext *ctx);
bool mapforge_region_add_way_to_tiles(BuildContext *ctx,
                                      const MercatorMeters *points,
                                      size_t count,
                                      RoadClass road_class,
                                      uint16_t z);
bool mapforge_region_add_polygon_to_tiles(BuildContext *ctx,
                                          const MercatorMeters *points,
                                          size_t count,
                                          PolygonClass polygon_class,
                                          uint16_t z);
void mapforge_region_ensure_tiles_for_bounds(BuildContext *ctx, const BuildOptions *options);
void mapforge_region_sort_tiles(BuildContext *ctx);
bool mapforge_region_write_tile_file(const BuildOptions *options,
                                     BuildContext *ctx,
                                     const TileOutput *tile);
bool mapforge_region_archive_rel_path_valid(const char *path);
bool mapforge_region_write_tile_archive_sqlite(const BuildOptions *options, BuildContext *ctx);
bool mapforge_region_write_meta_json(const BuildOptions *options, const BuildContext *ctx);
bool mapforge_region_write_metrics_dataset_json(const BuildOptions *options, const BuildContext *ctx);
const char *mapforge_region_metric_band_name(int band);
const char *mapforge_region_metric_layer_name(int layer);
const char *mapforge_region_archive_layer_from_suffix(const char *suffix);
int mapforge_region_archive_metric_band_index(const char *band);
int mapforge_region_archive_metric_layer_index(const char *layer);
bool mapforge_region_build_publish_paths(const char *active_dir,
                                         char *out_stage_dir,
                                         size_t stage_size,
                                         char *out_snapshot_root,
                                         size_t snapshot_size,
                                         char *out_staging_root,
                                         size_t staging_root_size);
bool mapforge_region_ensure_dir_recursive(const char *path);
bool mapforge_region_remove_tree(const char *path);
bool mapforge_region_validate_staged_region(const BuildOptions *options, const char *stage_dir);
bool mapforge_region_publish_region_pack(const BuildOptions *options,
                                         const char *stage_dir,
                                         const char *active_dir,
                                         const char *snapshot_root);
void mapforge_region_prune_staging_dirs(const char *staging_root, uint32_t prune_days, bool dry_run);

static inline void mapforge_region_log_diagnostic(const char *stage, const char *hint) {
    if (stage && stage[0] != '\0') {
        log_error("diagnostic_stage=%s", stage);
    }
    if (hint && hint[0] != '\0') {
        log_error("repair_hint=%s", hint);
    }
}

#endif
