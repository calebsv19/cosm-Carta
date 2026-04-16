#ifndef MAPFORGE_MAP_TILE_SOURCE_H
#define MAPFORGE_MAP_TILE_SOURCE_H

#include "map/tile_layers.h"
#include "map/tile_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAPFORGE_TILE_SOURCE_PATH_CAPACITY 512u

// Defines the region tile storage backend contract.
typedef enum TileStorageKind {
    TILE_STORAGE_FILESYSTEM_TREE = 0,
    TILE_STORAGE_ARCHIVE_INDEXED = 1
} TileStorageKind;

// Defines runtime source selection policy for archive-backed packages.
typedef enum TileSourcePolicyMode {
    TILE_SOURCE_POLICY_ARCHIVE_REQUIRED = 0,
    TILE_SOURCE_POLICY_ARCHIVE_PREFERRED = 1,
    TILE_SOURCE_POLICY_FILESYSTEM_ONLY = 2
} TileSourcePolicyMode;

// Carries runtime tile storage configuration for one region.
typedef struct TileSourceConfig {
    TileStorageKind storage_kind;
    TileSourcePolicyMode policy_mode;
    char tiles_root[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
    char archive_path[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
} TileSourceConfig;

// Captures runtime archive/fallback diagnostics across tile source usage.
typedef struct TileSourceRuntimeStats {
    uint64_t archive_request_count;
    uint64_t archive_hit_count;
    uint64_t archive_extract_count;
    uint64_t archive_extract_fail_count;
    uint64_t archive_fallback_tree_count;
    uint64_t archive_policy_block_count;
} TileSourceRuntimeStats;

// Initializes a tile source config to filesystem defaults.
void tile_source_config_init(TileSourceConfig *config);

// Assigns filesystem tree settings.
bool tile_source_config_set_filesystem(TileSourceConfig *config, const char *tiles_root);

// Assigns archive-backed settings with optional tree fallback root.
bool tile_source_config_set_archive(TileSourceConfig *config, const char *tiles_root, const char *archive_path);

// Assigns archive-backed settings with explicit runtime source policy.
bool tile_source_config_set_archive_with_policy(TileSourceConfig *config,
                                                const char *tiles_root,
                                                const char *archive_path,
                                                TileSourcePolicyMode policy_mode);

// Updates policy mode for an existing tile source config.
bool tile_source_config_set_policy(TileSourceConfig *config, TileSourcePolicyMode policy_mode);

// Returns a stable string label for storage kind.
const char *tile_storage_kind_label(TileStorageKind kind);

// Parses storage kind from metadata string.
TileStorageKind tile_storage_kind_from_string(const char *value);

// Returns a stable string label for source policy mode.
const char *tile_source_policy_mode_label(TileSourcePolicyMode mode);

// Parses source policy mode from metadata string.
bool tile_source_policy_mode_from_string(const char *value, TileSourcePolicyMode *out_mode);

// Returns default policy mode for a storage kind when metadata omits policy.
TileSourcePolicyMode tile_source_policy_mode_default(TileStorageKind storage_kind);

// Reports whether archive-backed reads are enabled in this build/runtime.
bool tile_source_archive_reader_supported(void);

// Clears aggregate runtime archive/fallback counters.
void tile_source_runtime_stats_reset(void);

// Reads aggregate runtime archive/fallback counters.
void tile_source_runtime_stats_get(TileSourceRuntimeStats *out_stats);

// Resolves one tile path to load for this source config.
bool tile_source_resolve_path(const TileSourceConfig *config,
                              TileCoord coord,
                              TileLayerKind kind,
                              TileZoomBand band,
                              char *out_path,
                              size_t out_size);

// Resolves a legacy unsuffixed MFT tile path (<z>/<x>/<y>.mft).
bool tile_source_resolve_legacy_path(const TileSourceConfig *config,
                                     TileCoord coord,
                                     TileZoomBand band,
                                     char *out_path,
                                     size_t out_size);

#endif
