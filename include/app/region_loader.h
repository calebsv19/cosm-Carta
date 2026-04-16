#ifndef MAPFORGE_APP_REGION_LOADER_H
#define MAPFORGE_APP_REGION_LOADER_H

#include "app/region.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAPFORGE_REGION_VALIDATION_SUMMARY_CAPACITY 256u

typedef struct RegionPackageValidationResult {
    bool ok;
    bool has_meta;
    bool has_tiles_root;
    bool has_archive_path;
    bool has_archive_file;
    bool has_graph;
    bool archive_storage;
    bool archive_reader_supported;
    bool archive_fallback_tree;
    bool has_tile_store_object;
    bool has_tile_store_kind;
    bool has_tile_store_root;
    bool has_tile_store_runtime_policy;
    bool tile_store_runtime_policy_valid;
    bool has_output_stats_object;
    bool has_tile_coverage_object;
    bool has_tile_coverage_bands_object;
    bool has_tile_coverage_zoom_entries;
    bool tile_coverage_contract_valid;
    uint32_t tile_coverage_zoom_entry_count;
    bool has_package_contract;
    bool has_package_contract_family;
    bool package_contract_family_valid;
    bool has_package_contract_version;
    uint32_t package_contract_version;
    bool package_contract_v1;
    bool package_contract_legacy;
    TileSourcePolicyMode runtime_policy_mode;
    char summary[MAPFORGE_REGION_VALIDATION_SUMMARY_CAPACITY];
} RegionPackageValidationResult;

// Loads region metadata from meta.json, if present.
bool region_load_meta(const RegionInfo *info, RegionInfo *out_info);

// Validates region package contract and required artifacts for runtime open flow.
bool region_validate_package(const RegionInfo *info, RegionPackageValidationResult *out_result);

// Validates region package contract with optional strict v1 contract enforcement.
bool region_validate_package_with_policy(const RegionInfo *info,
                                         bool require_contract_v1,
                                         RegionPackageValidationResult *out_result);

// Logs compact region package archive rollup diagnostics, when available.
void region_log_archive_rollup_summary(const RegionInfo *info, const char *context);

#endif
