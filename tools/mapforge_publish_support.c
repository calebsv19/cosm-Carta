#include "mapforge_publish_support.h"

#include "core/log.h"
#include "core_io.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct PublishSnapshotEntry {
    char path[512];
    time_t mtime;
} PublishSnapshotEntry;

static bool publish_ensure_dir(const char *path) {
    if (!path) {
        return false;
    }
    if (mkdir(path, 0755) == 0) {
        return true;
    }
    return errno == EEXIST;
}

bool mapforge_publish_path_exists(const char *path) {
    return path && core_io_path_exists(path);
}

bool mapforge_publish_path_is_dir(const char *path) {
    struct stat st;
    if (!path) {
        return false;
    }
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

bool mapforge_publish_ensure_dir_recursive(const char *path) {
    char buffer[512];
    size_t len = 0u;
    if (!path || path[0] == '\0') {
        return false;
    }
    snprintf(buffer, sizeof(buffer), "%s", path);
    len = strlen(buffer);
    if (len == 0u) {
        return false;
    }
    if (buffer[len - 1u] == '/') {
        buffer[len - 1u] = '\0';
    }
    for (char *p = buffer + 1; *p != '\0'; ++p) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (!publish_ensure_dir(buffer)) {
            return false;
        }
        *p = '/';
    }
    return publish_ensure_dir(buffer);
}

static void publish_trim_trailing_slashes(const char *path, char *out, size_t out_size) {
    size_t len = 0u;
    if (!out || out_size == 0u) {
        return;
    }
    if (!path) {
        out[0] = '\0';
        return;
    }
    snprintf(out, out_size, "%s", path);
    len = strlen(out);
    while (len > 1u && out[len - 1u] == '/') {
        out[len - 1u] = '\0';
        len -= 1u;
    }
}

static bool publish_path_has_parent_component(const char *path) {
    const char *p = path;
    if (!path) {
        return false;
    }
    while (*p != '\0') {
        const char *start = NULL;
        size_t len = 0u;
        while (*p == '/') {
            ++p;
        }
        start = p;
        while (*p != '\0' && *p != '/') {
            ++p;
        }
        len = (size_t)(p - start);
        if (len == 2u && start[0] == '.' && start[1] == '.') {
            return true;
        }
    }
    return false;
}

static bool publish_path_is_home_parent(const char *path) {
    return strcmp(path, "/Users") == 0 || strcmp(path, "/home") == 0;
}

static bool publish_path_is_user_home_root(const char *path, const char *prefix) {
    size_t prefix_len = strlen(prefix);
    const char *name = NULL;
    const char *after_name = NULL;
    if (strncmp(path, prefix, prefix_len) != 0 || path[prefix_len] != '/') {
        return false;
    }
    name = path + prefix_len + 1u;
    if (name[0] == '\0') {
        return false;
    }
    after_name = strchr(name, '/');
    return after_name == NULL || after_name[1] == '\0';
}

const char *mapforge_publish_root_reject_reason(const char *path) {
    char normalized[512];
    if (!path || path[0] == '\0') {
        return "empty";
    }
    publish_trim_trailing_slashes(path, normalized, sizeof(normalized));
    if (strcmp(normalized, "/") == 0) {
        return "root";
    }
    if (strcmp(normalized, ".") == 0) {
        return "current_directory";
    }
    if (publish_path_has_parent_component(normalized)) {
        return "parent_traversal";
    }
    if (strcmp(normalized, "~") == 0 || strncmp(normalized, "~/", 2) == 0) {
        return "home_root";
    }
    const char *home = getenv("HOME");
    if (home && home[0] != '\0') {
        char normalized_home[512];
        publish_trim_trailing_slashes(home, normalized_home, sizeof(normalized_home));
        if (strcmp(normalized, normalized_home) == 0) {
            return "home_root";
        }
    }
    if (publish_path_is_home_parent(normalized)) {
        return "home_parent";
    }
    if (publish_path_is_user_home_root(normalized, "/Users") ||
        publish_path_is_user_home_root(normalized, "/home")) {
        return "home_root";
    }
    return NULL;
}

bool mapforge_publish_root_is_safe(const char *path) {
    return mapforge_publish_root_reject_reason(path) == NULL;
}

bool mapforge_publish_split_parent_name(const char *path,
                                        char *out_parent,
                                        size_t parent_size,
                                        char *out_name,
                                        size_t name_size) {
    const char *slash = NULL;
    size_t parent_len = 0u;
    if (!path || !out_parent || !out_name || parent_size == 0u || name_size == 0u) {
        return false;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out_parent, parent_size, ".");
        snprintf(out_name, name_size, "%s", path);
        return true;
    }
    parent_len = (size_t)(slash - path);
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

bool mapforge_publish_remove_tree(const char *path) {
    struct stat st;
    if (!path || path[0] == '\0') {
        return false;
    }
    if (lstat(path, &st) != 0) {
        return errno == ENOENT;
    }
    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *entry = NULL;
        if (!dir) {
            return false;
        }
        while ((entry = readdir(dir)) != NULL) {
            char child[512];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
            if (!mapforge_publish_remove_tree(child)) {
                closedir(dir);
                return false;
            }
        }
        closedir(dir);
        return rmdir(path) == 0;
    }
    return unlink(path) == 0;
}

static int snapshot_entry_compare_desc(const void *a, const void *b) {
    const PublishSnapshotEntry *left = (const PublishSnapshotEntry *)a;
    const PublishSnapshotEntry *right = (const PublishSnapshotEntry *)b;
    if (left->mtime == right->mtime) {
        return strcmp(left->path, right->path);
    }
    return (left->mtime > right->mtime) ? -1 : 1;
}

static void publish_log_path(const char *format, const char *path) {
    if (format && format[0] != '\0') {
        log_info(format, path ? path : "");
    }
}

static void publish_log_path_error(const char *format, const char *path) {
    if (format && format[0] != '\0') {
        log_error(format, path ? path : "");
    }
}

void mapforge_publish_prune_snapshot_dir(const char *snapshot_root,
                                         uint32_t keep_old,
                                         uint32_t prune_days,
                                         bool dry_run,
                                         const char *dry_run_format,
                                         const char *failure_format,
                                         const char *success_format) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    PublishSnapshotEntry *items = NULL;
    size_t count = 0u;
    size_t capacity = 0u;
    time_t now = time(NULL);
    time_t prune_seconds = (prune_days > 0u) ? (time_t)prune_days * 24 * 60 * 60 : 0;

    if (!snapshot_root || !mapforge_publish_path_is_dir(snapshot_root)) {
        return;
    }
    dir = opendir(snapshot_root);
    if (!dir) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[512];
        struct stat st;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", snapshot_root, entry->d_name);
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }
        if (count == capacity) {
            size_t next = (capacity == 0u) ? 16u : capacity * 2u;
            PublishSnapshotEntry *next_items = (PublishSnapshotEntry *)realloc(items, next * sizeof(PublishSnapshotEntry));
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

    qsort(items, count, sizeof(PublishSnapshotEntry), snapshot_entry_compare_desc);
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
            publish_log_path(dry_run_format, items[i].path);
            continue;
        }
        if (!mapforge_publish_remove_tree(items[i].path)) {
            publish_log_path_error(failure_format, items[i].path);
        } else {
            publish_log_path(success_format, items[i].path);
        }
    }
    free(items);
}

void mapforge_publish_prune_staging_dirs(const char *staging_root,
                                         uint32_t prune_days,
                                         bool dry_run,
                                         const char *dry_run_format,
                                         const char *failure_format,
                                         const char *success_format) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    time_t now = time(NULL);
    time_t prune_seconds = (prune_days > 0u) ? (time_t)prune_days * 24 * 60 * 60 : 0;
    if (!staging_root || prune_seconds == 0 || !mapforge_publish_path_is_dir(staging_root)) {
        return;
    }
    dir = opendir(staging_root);
    if (!dir) {
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        char path[512];
        struct stat st;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", staging_root, entry->d_name);
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }
        if (now < st.st_mtime || (now - st.st_mtime) <= prune_seconds) {
            continue;
        }
        if (dry_run) {
            publish_log_path(dry_run_format, path);
            continue;
        }
        if (!mapforge_publish_remove_tree(path)) {
            publish_log_path_error(failure_format, path);
        } else {
            publish_log_path(success_format, path);
        }
    }
    closedir(dir);
}

bool mapforge_publish_build_paths(const char *active_root,
                                  const char *staging_dir_name,
                                  const char *snapshot_dir_name,
                                  char *out_stage_root,
                                  size_t stage_size,
                                  char *out_snapshot_root,
                                  size_t snapshot_size,
                                  char *out_staging_root,
                                  size_t staging_root_size) {
    char parent[512];
    char name[256];
    time_t now = time(NULL);
    long pid = (long)getpid();
    if (!active_root || !staging_dir_name || !snapshot_dir_name ||
        !out_stage_root || !out_snapshot_root || !out_staging_root) {
        return false;
    }
    if (!mapforge_publish_split_parent_name(active_root, parent, sizeof(parent), name, sizeof(name))) {
        return false;
    }
    snprintf(out_staging_root, staging_root_size, "%s/%s", parent, staging_dir_name);
    snprintf(out_snapshot_root, snapshot_size, "%s/%s/%s", parent, snapshot_dir_name, name);
    snprintf(out_stage_root, stage_size, "%s/%s.%ld.%ld", out_staging_root, name, (long)now, pid);
    return true;
}
