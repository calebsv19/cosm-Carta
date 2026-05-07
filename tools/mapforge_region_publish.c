#include "mapforge_region_internal.h"

typedef struct SnapshotEntry {
    char path[512];
    time_t mtime;
} SnapshotEntry;

static bool ensure_dir(const char *path) {
    if (!path) {
        return false;
    }

    if (mkdir(path, 0755) == 0) {
        return true;
    }

    return errno == EEXIST;
}

static bool path_exists(const char *path) {
    return path && core_io_path_exists(path);
}

static bool path_is_dir(const char *path) {
    struct stat st;
    if (!path) {
        return false;
    }
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

bool mapforge_region_ensure_dir_recursive(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s", path);
    size_t len = strlen(buffer);
    if (len == 0u) {
        return false;
    }

    if (buffer[len - 1] == '/') {
        buffer[len - 1] = '\0';
    }

    for (char *p = buffer + 1; *p != '\0'; ++p) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (!ensure_dir(buffer)) {
            return false;
        }
        *p = '/';
    }

    return ensure_dir(buffer);
}

static bool split_parent_name(const char *path, char *out_parent, size_t parent_size, char *out_name, size_t name_size) {
    if (!path || !out_parent || !out_name || parent_size == 0u || name_size == 0u) {
        return false;
    }

    const char *slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out_parent, parent_size, ".");
        snprintf(out_name, name_size, "%s", path);
        return true;
    }

    size_t parent_len = (size_t)(slash - path);
    if (parent_len == 0u) {
        snprintf(out_parent, parent_size, "/");
    } else {
        if (parent_len >= parent_size) {
            return false;
        }
        memcpy(out_parent, path, parent_len);
        out_parent[parent_len] = '\0';
    }

    snprintf(out_name, name_size, "%s", slash + 1);
    return out_name[0] != '\0';
}

bool mapforge_region_remove_tree(const char *path) {
    struct stat st;
    if (!path || path[0] == '\0') {
        return false;
    }

    if (lstat(path, &st) != 0) {
        return errno == ENOENT;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) {
            return false;
        }

        struct dirent *entry = NULL;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char child[512];
            snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
            if (!mapforge_region_remove_tree(child)) {
                closedir(dir);
                return false;
            }
        }
        closedir(dir);
        return rmdir(path) == 0;
    }

    return unlink(path) == 0;
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

static int snapshot_entry_compare_desc(const void *a, const void *b) {
    const SnapshotEntry *left = (const SnapshotEntry *)a;
    const SnapshotEntry *right = (const SnapshotEntry *)b;
    if (left->mtime == right->mtime) {
        return strcmp(left->path, right->path);
    }
    return (left->mtime > right->mtime) ? -1 : 1;
}

static void prune_snapshot_dir(const char *snapshot_root, uint32_t keep_old, uint32_t prune_days, bool dry_run) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    SnapshotEntry *items = NULL;
    size_t count = 0u;
    size_t capacity = 0u;
    time_t now = time(NULL);
    time_t prune_seconds = (prune_days > 0u) ? (time_t)prune_days * 24 * 60 * 60 : 0;

    if (!snapshot_root || !path_is_dir(snapshot_root)) {
        return;
    }

    dir = opendir(snapshot_root);
    if (!dir) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char path[512];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", snapshot_root, entry->d_name);
        if (stat(path, &st) != 0) {
            continue;
        }
        if (!S_ISDIR(st.st_mode)) {
            continue;
        }

        if (count == capacity) {
            size_t next = (capacity == 0u) ? 16u : capacity * 2u;
            SnapshotEntry *next_items = (SnapshotEntry *)realloc(items, next * sizeof(SnapshotEntry));
            if (!next_items) {
                free(items);
                closedir(dir);
                return;
            }
            items = next_items;
            capacity = next;
        }

        snprintf(items[count].path, sizeof(items[count].path), "%s", path);
        items[count].mtime = st.st_mtime;
        count += 1u;
    }

    closedir(dir);

    if (count == 0u) {
        free(items);
        return;
    }

    qsort(items, count, sizeof(SnapshotEntry), snapshot_entry_compare_desc);

    for (size_t i = 0u; i < count; ++i) {
        bool remove_by_count = i >= keep_old;
        bool remove_by_age = false;
        if (prune_seconds > 0 && now >= items[i].mtime) {
            remove_by_age = (now - items[i].mtime) > prune_seconds;
        }
        if (!remove_by_count && !remove_by_age) {
            continue;
        }

        if (dry_run) {
            log_info("dry-run prune snapshot: %s", items[i].path);
            continue;
        }
        if (!mapforge_region_remove_tree(items[i].path)) {
            log_error("Failed to prune snapshot: %s", items[i].path);
        } else {
            log_info("Pruned snapshot: %s", items[i].path);
        }
    }

    free(items);
}

void mapforge_region_prune_staging_dirs(const char *staging_root, uint32_t prune_days, bool dry_run) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    time_t now = time(NULL);
    time_t prune_seconds = (prune_days > 0u) ? (time_t)prune_days * 24 * 60 * 60 : 0;
    if (!staging_root || prune_seconds == 0 || !path_is_dir(staging_root)) {
        return;
    }

    dir = opendir(staging_root);
    if (!dir) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char path[512];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", staging_root, entry->d_name);
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }
        if (now < st.st_mtime || (now - st.st_mtime) <= prune_seconds) {
            continue;
        }
        if (dry_run) {
            log_info("dry-run prune staging dir: %s", path);
            continue;
        }
        if (!mapforge_region_remove_tree(path)) {
            log_error("Failed to prune stale staging dir: %s", path);
        } else {
            log_info("Pruned stale staging dir: %s", path);
        }
    }

    closedir(dir);
}

bool mapforge_region_build_publish_paths(const char *active_dir,
                                         char *out_stage_dir,
                                         size_t stage_size,
                                         char *out_snapshot_root,
                                         size_t snapshot_size,
                                         char *out_staging_root,
                                         size_t staging_root_size) {
    char parent[512];
    char name[256];
    time_t now = time(NULL);
    long pid = (long)getpid();

    if (!active_dir || !out_stage_dir || !out_snapshot_root || !out_staging_root) {
        return false;
    }
    if (!split_parent_name(active_dir, parent, sizeof(parent), name, sizeof(name))) {
        return false;
    }
    snprintf(out_staging_root, staging_root_size, "%s/.staging", parent);
    snprintf(out_snapshot_root, snapshot_size, "%s/.snapshots/%s", parent, name);
    snprintf(out_stage_dir, stage_size, "%s/%s.%ld.%ld", out_staging_root, name, (long)now, pid);
    return true;
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
        return false;
    }
    if (stat(tiles_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        log_error("staged region missing tiles dir: %s", tiles_path);
        return false;
    }
    if (!tree_has_file_suffix(tiles_path, ".mft")) {
        log_error("staged region missing tile payload files: %s", tiles_path);
        return false;
    }
    if (options->emit_archive) {
        const char *archive_rel = (options->archive_path && options->archive_path[0] != '\0')
            ? options->archive_path
            : "tiles.mbtiles";
        snprintf(archive_path, sizeof(archive_path), "%s/%s", stage_dir, archive_rel);
        if (stat(archive_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            log_error("staged region missing archive payload: %s", archive_path);
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
    if (!split_parent_name(active_dir, active_parent, sizeof(active_parent), active_name, sizeof(active_name))) {
        return false;
    }

    snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s.%ld.%ld", snapshot_root, active_name, (long)now, pid);

    if (path_exists(active_dir)) {
        if (options->replace) {
            if (!mapforge_region_remove_tree(active_dir)) {
                log_error("Failed to remove existing region dir: %s", active_dir);
                return false;
            }
        } else {
            if (!mapforge_region_ensure_dir_recursive(snapshot_root)) {
                log_error("Failed to ensure snapshot root: %s", snapshot_root);
                return false;
            }
            if (rename(active_dir, snapshot_path) != 0) {
                log_error("Failed to move active region into snapshot: %s", strerror(errno));
                return false;
            }
            moved_active = true;
        }
    }

    if (rename(stage_dir, active_dir) != 0) {
        log_error("Failed to publish staged region: %s", strerror(errno));
        if (moved_active && rename(snapshot_path, active_dir) != 0) {
            log_error("Rollback failed for region publish: %s", strerror(errno));
        }
        return false;
    }

    if (!options->replace) {
        prune_snapshot_dir(snapshot_root, options->keep_old, options->prune_days, options->prune_dry_run);
    }
    return true;
}
