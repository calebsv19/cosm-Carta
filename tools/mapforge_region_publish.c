#include "mapforge_region_internal.h"
#include "mapforge_publish_support.h"

static bool require_region_publish_root_safe(const char *label, const char *path) {
    const char *reason = mapforge_publish_root_reject_reason(path);
    if (!reason) {
        return true;
    }
    log_error("Refusing unsafe region publish path for %s (%s): %s",
              label ? label : "path",
              reason,
              path ? path : "");
    mapforge_region_log_diagnostic("publish",
                                   "Choose a dedicated region output directory below the workspace or a temp directory.");
    return false;
}

bool mapforge_region_ensure_dir_recursive(const char *path) {
    return mapforge_publish_ensure_dir_recursive(path);
}

bool mapforge_region_remove_tree(const char *path) {
    if (!require_region_publish_root_safe("remove_tree", path)) {
        return false;
    }
    return mapforge_publish_remove_tree(path);
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

static bool tree_has_file_suffix(const char *root, const char *suffix) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    if (!root || !suffix) {
        return false;
    }

    dir = opendir(root);
    if (!dir) {
        return false;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char path[512];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", root, entry->d_name);
        if (lstat(path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (tree_has_file_suffix(path, suffix)) {
                closedir(dir);
                return true;
            }
            continue;
        }
        if (S_ISREG(st.st_mode) && string_has_suffix(entry->d_name, suffix)) {
            closedir(dir);
            return true;
        }
    }

    closedir(dir);
    return false;
}

static void prune_snapshot_dir(const char *snapshot_root, uint32_t keep_old, uint32_t prune_days, bool dry_run) {
    if (!require_region_publish_root_safe("snapshot_root", snapshot_root)) {
        return;
    }
    mapforge_publish_prune_snapshot_dir(snapshot_root,
                                        keep_old,
                                        prune_days,
                                        dry_run,
                                        "dry-run prune snapshot: %s",
                                        "Failed to prune snapshot: %s",
                                        "Pruned snapshot: %s");
}

void mapforge_region_prune_staging_dirs(const char *staging_root, uint32_t prune_days, bool dry_run) {
    if (!require_region_publish_root_safe("staging_root", staging_root)) {
        return;
    }
    mapforge_publish_prune_staging_dirs(staging_root,
                                        prune_days,
                                        dry_run,
                                        "dry-run prune staging dir: %s",
                                        "Failed to prune stale staging dir: %s",
                                        "Pruned stale staging dir: %s");
}

bool mapforge_region_build_publish_paths(const char *active_dir,
                                         char *out_stage_dir,
                                         size_t stage_size,
                                         char *out_snapshot_root,
                                         size_t snapshot_size,
                                         char *out_staging_root,
                                         size_t staging_root_size) {
    if (!require_region_publish_root_safe("active output", active_dir)) {
        return false;
    }
    return mapforge_publish_build_paths(active_dir,
                                        ".staging",
                                        ".snapshots",
                                        out_stage_dir,
                                        stage_size,
                                        out_snapshot_root,
                                        snapshot_size,
                                        out_staging_root,
                                        staging_root_size);
}

bool mapforge_region_validate_staged_region(const BuildOptions *options, const char *stage_dir) {
    char meta_path[512];
    char tiles_path[512];
    char archive_path[512];
    struct stat st;

    if (!options || !stage_dir) {
        return false;
    }

    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", stage_dir);
    snprintf(tiles_path, sizeof(tiles_path), "%s/tiles", stage_dir);

    if (stat(meta_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        log_error("staged region missing meta.json: %s", meta_path);
        mapforge_region_log_diagnostic("validation",
                                       "Rebuild the region; staged packages must contain meta.json before publish.");
        return false;
    }
    if (stat(tiles_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        log_error("staged region missing tiles dir: %s", tiles_path);
        mapforge_region_log_diagnostic("validation",
                                       "Rebuild the region; staged packages must contain a tiles directory before publish.");
        return false;
    }
    if (!tree_has_file_suffix(tiles_path, ".mft")) {
        log_error("staged region missing tile payload files: %s", tiles_path);
        mapforge_region_log_diagnostic("validation",
                                       "Verify source coverage and tile generation; staged tiles must include .mft payloads.");
        return false;
    }
    if (options->emit_archive) {
        const char *archive_rel = (options->archive_path && options->archive_path[0] != '\0')
            ? options->archive_path
            : "tiles.mbtiles";
        snprintf(archive_path, sizeof(archive_path), "%s/%s", stage_dir, archive_rel);
        if (stat(archive_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            log_error("staged region missing archive payload: %s", archive_path);
            mapforge_region_log_diagnostic("validation",
                                           "Rebuild with --emit-archive or remove archive_indexed metadata before publish.");
            return false;
        }
    }
    return true;
}

bool mapforge_region_publish_region_pack(const BuildOptions *options,
                                         const char *stage_dir,
                                         const char *active_dir,
                                         const char *snapshot_root) {
    char snapshot_path[512];
    char active_parent[512];
    char active_name[256];
    bool moved_active = false;
    time_t now = time(NULL);
    long pid = (long)getpid();

    if (!options || !stage_dir || !active_dir || !snapshot_root) {
        return false;
    }
    if (!require_region_publish_root_safe("stage_dir", stage_dir) ||
        !require_region_publish_root_safe("active output", active_dir) ||
        !require_region_publish_root_safe("snapshot_root", snapshot_root)) {
        return false;
    }
    if (!mapforge_publish_split_parent_name(active_dir, active_parent, sizeof(active_parent), active_name, sizeof(active_name))) {
        return false;
    }

    snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s.%ld.%ld", snapshot_root, active_name, (long)now, pid);

    if (mapforge_publish_path_exists(active_dir)) {
        if (options->replace) {
            if (!mapforge_region_remove_tree(active_dir)) {
                log_error("Failed to remove existing region dir: %s", active_dir);
                mapforge_region_log_diagnostic("publish",
                                               "Check permissions for the active region directory or publish to a writable output root.");
                return false;
            }
        } else {
            if (!mapforge_region_ensure_dir_recursive(snapshot_root)) {
                log_error("Failed to ensure snapshot root: %s", snapshot_root);
                mapforge_region_log_diagnostic("publish",
                                               "Check permissions for the snapshot root or rerun with --replace.");
                return false;
            }
            if (rename(active_dir, snapshot_path) != 0) {
                log_error("Failed to move active region into snapshot: %s", strerror(errno));
                mapforge_region_log_diagnostic("publish",
                                               "Check permissions for the active output root or rerun with --replace.");
                return false;
            }
            moved_active = true;
        }
    }

    if (rename(stage_dir, active_dir) != 0) {
        log_error("Failed to publish staged region: %s", strerror(errno));
        mapforge_region_log_diagnostic("publish",
                                       "Check write permissions for the output root and keep the staging directory for inspection.");
        if (moved_active && rename(snapshot_path, active_dir) != 0) {
            log_error("Rollback failed for region publish: %s", strerror(errno));
            mapforge_region_log_diagnostic("rollback",
                                           "Restore the newest snapshot manually from the .snapshots directory.");
        }
        return false;
    }

    if (!options->replace) {
        prune_snapshot_dir(snapshot_root, options->keep_old, options->prune_days, options->prune_dry_run);
    }
    return true;
}
