#ifndef MAPFORGE_APP_REGION_LOADER_H
#define MAPFORGE_APP_REGION_LOADER_H

#include "app/region.h"

#include <stdbool.h>
#include <stddef.h>

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
    char summary[MAPFORGE_REGION_VALIDATION_SUMMARY_CAPACITY];
} RegionPackageValidationResult;

// Loads region metadata from meta.json, if present.
bool region_load_meta(const RegionInfo *info, RegionInfo *out_info);

// Validates region package contract and required artifacts for runtime open flow.
bool region_validate_package(const RegionInfo *info, RegionPackageValidationResult *out_result);

// Logs compact region package archive rollup diagnostics, when available.
void region_log_archive_rollup_summary(const RegionInfo *info, const char *context);

#endif
