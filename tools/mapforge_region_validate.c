#include "app/region.h"
#include "app/region_loader.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void print_usage(const char *argv0) {
    printf("Usage: %s [--region <name>] [--strict] [--strict-contract]\n", argv0 ? argv0 : "mapforge_region_validate");
    printf("  --region <name>   validate one region only\n");
    printf("  --strict          treat archive->tree fallback as failure\n");
    printf("  --strict-contract require package_contract v1 metadata\n");
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
        return 1;
    }

    printf("Validated %d region(s); %d failed.\n", checked, failed);
    return failed == 0 ? 0 : 1;
}
