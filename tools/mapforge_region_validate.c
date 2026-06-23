#include "app/region.h"
#include "app/region_loader.h"
#include "core/log.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void print_usage(const char *argv0) {
    printf("Usage: %s [--region <name>] [--strict] [--strict-contract]\n", argv0 ? argv0 : "mapforge_region_validate");
    printf("  --region <name>   validate one region only\n");
    printf("  --strict          treat archive->tree fallback as failure\n");
    printf("  --strict-contract require package_contract v1 metadata\n");
}

static void print_validation_diagnostic(const RegionInfo *info,
                                        const RegionPackageValidationResult *result,
                                        bool strict_archive_fallback) {
    const char *region_name = (info && info->name) ? info->name : "(unknown)";
    const char *hint = NULL;

    log_error("diagnostic_stage=validation region=%s", region_name);
    if (!result) {
        return;
    }
    if (result->summary[0] != '\0') {
        log_error("validation_summary=%s", result->summary);
    }

    if (!result->has_meta) {
        hint = "Create meta.json by rebuilding the region package.";
    } else if (result->has_tile_store_runtime_policy && !result->tile_store_runtime_policy_valid) {
        hint = "Set tile_store.runtime_source_policy to archive_required, archive_preferred, or filesystem_only.";
    } else if (result->has_tile_store_runtime_policy && result->runtime_policy_mode != TILE_SOURCE_POLICY_FILESYSTEM_ONLY &&
               !result->archive_storage) {
        hint = "Use tile_store.kind=archive_indexed for archive runtime policies or switch policy to filesystem_only.";
    } else if (!result->has_tile_store_kind || !result->has_tile_store_root) {
        hint = "Add tile_store.kind and tile_store.root to meta.json.";
    } else if (result->archive_storage && !result->has_archive_path) {
        hint = "Add tile_store.archive_path for archive_indexed packages.";
    } else if (result->archive_storage && !result->has_archive_file) {
        hint = "Rebuild with --emit-archive or restore the archive payload named by tile_store.archive_path.";
    } else if (strict_archive_fallback) {
        hint = "Provide a readable archive payload or run without --strict when tree fallback is acceptable.";
    } else if (result->package_contract_v1 && !result->has_output_stats_object) {
        hint = "Rebuild with the current region tool to emit output_stats metadata.";
    } else if (result->package_contract_v1 && !result->has_tile_coverage_object) {
        hint = "Rebuild with the current region tool to emit output_stats.tile_coverage metadata.";
    } else if (result->has_tile_coverage_object && !result->tile_coverage_contract_valid) {
        hint = "Regenerate tile coverage metadata; zoom_bounds entries must be well formed.";
    } else if (result->has_tile_coverage_object && !result->has_tile_coverage_zoom_entries) {
        hint = "Regenerate tile coverage metadata with at least one zoom_bounds entry.";
    } else if (result->has_package_contract && !result->package_contract_v1) {
        hint = "Set package_contract.version to 1 or rebuild with the current region tool.";
    } else if (!result->has_package_contract) {
        hint = "Rebuild with the current region tool to emit package_contract metadata.";
    } else if (result->archive_storage && !result->archive_reader_supported && !result->has_tiles_root) {
        hint = "Provide extracted tile_store.root fallback or build with archive reader support.";
    } else if (!result->has_tiles_root) {
        hint = "Restore the tile_store.root directory or rebuild the region package.";
    }

    if (hint) {
        log_error("repair_hint=%s", hint);
    }
}

int main(int argc, char **argv) {
    const char *target_region = NULL;
    bool strict = false;
    bool strict_contract = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--region") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--region requires a value\n");
                return 2;
            }
            target_region = argv[++i];
        } else if (strcmp(argv[i], "--strict") == 0) {
            strict = true;
        } else if (strcmp(argv[i], "--strict-contract") == 0) {
            strict_contract = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        }
    }

    int total = region_count();
    if (total <= 0) {
        printf("No regions discovered under %s\n", region_data_root());
        return 1;
    }

    int checked = 0;
    int failed = 0;
    for (int i = 0; i < total; ++i) {
        const RegionInfo *info = region_get(i);
        if (!info || !info->name) {
            continue;
        }
        if (target_region && strcmp(info->name, target_region) != 0) {
            continue;
        }
        checked += 1;

        RegionPackageValidationResult result = {0};
        bool ok = region_validate_package_with_policy(info, strict_contract, &result);
        bool strict_fail = strict && result.archive_storage && result.archive_fallback_tree;
        if (!ok || strict_fail) {
            failed += 1;
            print_validation_diagnostic(info, &result, strict_fail);
        }

        printf("[%s] %s: %s\n",
               (!ok || strict_fail) ? "FAIL" : "OK",
               info->name,
               result.summary[0] != '\0' ? result.summary : (ok ? "valid" : "invalid"));
        if (result.archive_storage) {
            printf("  archive: path=%s file=%s reader_supported=%s fallback_tree=%s policy=%s\n",
                   result.has_archive_path ? "yes" : "no",
                   result.has_archive_file ? "yes" : "no",
                   result.archive_reader_supported ? "yes" : "no",
                   result.archive_fallback_tree ? "yes" : "no",
                   tile_source_policy_mode_label(result.runtime_policy_mode));
        }
        printf("  contract: tile_store=%s kind=%s root=%s policy=%s policy_valid=%s package=%s v1=%s legacy=%s\n",
               result.has_tile_store_object ? "yes" : "no",
               result.has_tile_store_kind ? "yes" : "no",
               result.has_tile_store_root ? "yes" : "no",
               result.has_tile_store_runtime_policy ? "yes" : "no",
               result.tile_store_runtime_policy_valid ? "yes" : "no",
               result.has_package_contract ? "yes" : "no",
               result.package_contract_v1 ? "yes" : "no",
               result.package_contract_legacy ? "yes" : "no");
        printf("  coverage: output_stats=%s tile_coverage=%s bands=%s zoom_entries=%u valid=%s\n",
               result.has_output_stats_object ? "yes" : "no",
               result.has_tile_coverage_object ? "yes" : "no",
               result.has_tile_coverage_bands_object ? "yes" : "no",
               result.tile_coverage_zoom_entry_count,
               result.tile_coverage_contract_valid ? "yes" : "no");
        printf("  tiles_root=%s graph=%s\n",
               result.has_tiles_root ? "yes" : "no",
               result.has_graph ? "yes" : "no");
    }

    if (target_region && checked == 0) {
        fprintf(stderr, "region not found: %s\n", target_region);
        log_error("diagnostic_stage=validation region=%s", target_region);
        log_error("repair_hint=Check MAPFORGE_REGIONS_DIR and the --region name.");
        return 1;
    }

    printf("Validated %d region(s); %d failed.\n", checked, failed);
    return failed == 0 ? 0 : 1;
}
