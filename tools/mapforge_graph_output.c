#include "mapforge_graph_output.h"

#include "core/log.h"
#include "core_io.h"
#include "map/mercator.h"

#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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

static bool path_exists(const char *path) {
    return path && core_io_path_exists(path);
}

bool ensure_dir_recursive(const char *path) {
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

bool remove_tree(const char *path) {
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
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char child[512];
            snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
            if (!remove_tree(child)) {
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
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
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
            log_info("dry-run prune graph snapshot: %s", items[i].path);
            continue;
        }
        if (!remove_tree(items[i].path)) {
            log_error("Failed to prune graph snapshot: %s", items[i].path);
        }
    }
    free(items);
}

void prune_staging_dirs(const char *staging_root, uint32_t prune_days, bool dry_run) {
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
            log_info("dry-run prune graph staging dir: %s", path);
            continue;
        }
        if (!remove_tree(path)) {
            log_error("Failed to prune graph staging dir: %s", path);
        }
    }
    closedir(dir);
}

bool build_publish_paths(const char *active_root,
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
    if (!active_root || !out_stage_root || !out_snapshot_root || !out_staging_root) {
        return false;
    }
    if (!split_parent_name(active_root, parent, sizeof(parent), name, sizeof(name))) {
        return false;
    }
    snprintf(out_staging_root, staging_root_size, "%s/.graph_staging", parent);
    snprintf(out_snapshot_root, snapshot_size, "%s/.graph_snapshots/%s", parent, name);
    snprintf(out_stage_root, stage_size, "%s/%s.%ld.%ld", out_staging_root, name, (long)now, pid);
    return true;
}

bool validate_staged_graph(const char *stage_root) {
    char path[512];
    CoreBuffer buffer = {0};
    char magic[4] = {0};
    uint32_t version = 0u;
    snprintf(path, sizeof(path), "%s/graph/graph.bin", stage_root);
    CoreResult r = core_io_read_all(path, &buffer);
    if (r.code != CORE_OK || !buffer.data || buffer.size < 8u) {
        core_io_buffer_free(&buffer);
        log_error("missing staged graph file: %s", path);
        return false;
    }
    memcpy(magic, buffer.data, 4u);
    memcpy(&version, buffer.data + 4u, sizeof(uint32_t));
    core_io_buffer_free(&buffer);
    if (memcmp(magic, GRAPH_MAGIC, 4) != 0 ||
        (version != GRAPH_VERSION_V1 && version != GRAPH_VERSION_V2)) {
        log_error("unexpected staged graph magic/version: %s", path);
        return false;
    }
    return true;
}

bool publish_staged_graph(const GraphOptions *options,
                          const char *stage_root,
                          const char *active_root,
                          const char *snapshot_root) {
    char active_graph[512];
    char stage_graph[512];
    char snapshot_path[512];
    bool moved_active = false;
    time_t now = time(NULL);
    long pid = (long)getpid();

    if (!options || !stage_root || !active_root || !snapshot_root) {
        return false;
    }

    snprintf(active_graph, sizeof(active_graph), "%s/graph", active_root);
    snprintf(stage_graph, sizeof(stage_graph), "%s/graph", stage_root);
    snprintf(snapshot_path, sizeof(snapshot_path), "%s/graph.%ld.%ld", snapshot_root, (long)now, pid);

    if (!ensure_dir_recursive(active_root)) {
        return false;
    }

    if (path_exists(active_graph)) {
        if (options->replace) {
            if (!remove_tree(active_graph)) {
                return false;
            }
        } else {
            if (!ensure_dir_recursive(snapshot_root)) {
                return false;
            }
            if (rename(active_graph, snapshot_path) != 0) {
                return false;
            }
            moved_active = true;
        }
    }

    if (rename(stage_graph, active_graph) != 0) {
        if (moved_active) {
            rename(snapshot_path, active_graph);
        }
        return false;
    }

    if (!options->replace) {
        prune_snapshot_dir(snapshot_root, options->keep_old, options->prune_days, options->prune_dry_run);
    }
    return true;
}

bool write_graph(const GraphBuild *build, const char *out_dir) {
    if (!build || !out_dir || build->node_count == 0) {
        return false;
    }

    char graph_dir[512];
    snprintf(graph_dir, sizeof(graph_dir), "%s/graph", out_dir);
    if (!ensure_dir(out_dir)) {
        return false;
    }
    if (!ensure_dir(graph_dir)) {
        return false;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/graph.bin", graph_dir);

    double *node_x = (double *)malloc(sizeof(double) * build->node_count);
    double *node_y = (double *)malloc(sizeof(double) * build->node_count);
    uint32_t *edge_start = (uint32_t *)calloc(build->node_count + 1, sizeof(uint32_t));
    uint32_t *edge_to = NULL;
    float *edge_length = NULL;
    float *edge_speed = NULL;
    float *edge_speed_limit = NULL;
    float *edge_grade = NULL;
    float *edge_penalty = NULL;
    uint8_t *edge_class = NULL;

    if (!node_x || !node_y || !edge_start) {
        free(node_x);
        free(node_y);
        free(edge_start);
        return false;
    }

    for (size_t i = 0; i < build->node_count; ++i) {
        double lat = 0.0;
        double lon = 0.0;
        int64_t id = build->node_ids[i];
        if (!node_map_get(&build->nodes, id, &lat, &lon)) {
            lat = 0.0;
            lon = 0.0;
        }
        MercatorMeters meters = mercator_from_latlon((LatLon){lat, lon});
        node_x[i] = meters.x;
        node_y[i] = meters.y;
    }

    Edge *edges = (Edge *)malloc(sizeof(Edge) * build->edge_count);
    if (!edges) {
        free(node_x);
        free(node_y);
        free(edge_start);
        return false;
    }

    size_t edge_count = 0;
    for (size_t i = 0; i < build->edge_count; ++i) {
        Edge edge = build->edges[i];
        uint32_t from_index = 0;
        uint32_t to_index = 0;
        if (!index_map_get(&build->index_map, edge.from_id, &from_index) ||
            !index_map_get(&build->index_map, edge.to_id, &to_index)) {
            continue;
        }

        edges[edge_count].from_id = (int64_t)from_index;
        edges[edge_count].to_id = (int64_t)to_index;
        edges[edge_count].length_m = edge.length_m;
        edges[edge_count].speed_mps = edge.speed_mps;
        edges[edge_count].speed_limit_mps = edge.speed_limit_mps;
        edges[edge_count].grade = edge.grade;
        edges[edge_count].objective_penalty = edge.objective_penalty;
        edges[edge_count].road_class = edge.road_class;
        edge_start[from_index + 1] += 1;
        edge_count += 1;
    }

    for (size_t i = 1; i <= build->node_count; ++i) {
        edge_start[i] += edge_start[i - 1];
    }

    edge_to = (uint32_t *)malloc(sizeof(uint32_t) * edge_count);
    edge_length = (float *)malloc(sizeof(float) * edge_count);
    edge_speed = (float *)malloc(sizeof(float) * edge_count);
    edge_speed_limit = (float *)malloc(sizeof(float) * edge_count);
    edge_grade = (float *)malloc(sizeof(float) * edge_count);
    edge_penalty = (float *)malloc(sizeof(float) * edge_count);
    edge_class = (uint8_t *)malloc(sizeof(uint8_t) * edge_count);
    if (!edge_to || !edge_length || !edge_speed || !edge_speed_limit || !edge_grade || !edge_penalty || !edge_class) {
        free(node_x);
        free(node_y);
        free(edge_start);
        free(edge_to);
        free(edge_length);
        free(edge_speed);
        free(edge_speed_limit);
        free(edge_grade);
        free(edge_penalty);
        free(edge_class);
        free(edges);
        return false;
    }

    uint32_t *cursor = (uint32_t *)calloc(build->node_count, sizeof(uint32_t));
    if (!cursor) {
        free(node_x);
        free(node_y);
        free(edge_start);
        free(edge_to);
        free(edge_length);
        free(edge_speed);
        free(edge_speed_limit);
        free(edge_grade);
        free(edge_penalty);
        free(edge_class);
        free(edges);
        return false;
    }

    for (size_t i = 0; i < edge_count; ++i) {
        uint32_t from = (uint32_t)edges[i].from_id;
        uint32_t slot = edge_start[from] + cursor[from];
        cursor[from] += 1;
        edge_to[slot] = (uint32_t)edges[i].to_id;
        edge_length[slot] = edges[i].length_m;
        edge_speed[slot] = edges[i].speed_mps;
        edge_speed_limit[slot] = edges[i].speed_limit_mps;
        edge_grade[slot] = edges[i].grade;
        edge_penalty[slot] = edges[i].objective_penalty;
        edge_class[slot] = edges[i].road_class;
    }

    free(cursor);
    free(edges);

    uint32_t node_count = (uint32_t)build->node_count;
    uint32_t out_edge_count = (uint32_t)edge_count;
    const size_t header_bytes = 4u + sizeof(uint32_t) * 3u;
    const size_t node_bytes = (size_t)node_count * sizeof(double) * 2u;
    const size_t edge_start_bytes = (size_t)(node_count + 1u) * sizeof(uint32_t);
    const size_t edge_to_bytes = (size_t)out_edge_count * sizeof(uint32_t);
    const size_t edge_length_bytes = (size_t)out_edge_count * sizeof(float);
    const size_t edge_speed_bytes = (size_t)out_edge_count * sizeof(float);
    const size_t edge_class_bytes = (size_t)out_edge_count * sizeof(uint8_t);
    const size_t edge_speed_limit_bytes = (size_t)out_edge_count * sizeof(float);
    const size_t edge_grade_bytes = (size_t)out_edge_count * sizeof(float);
    const size_t edge_penalty_bytes = (size_t)out_edge_count * sizeof(float);
    const size_t total_bytes = header_bytes + node_bytes + edge_start_bytes + edge_to_bytes +
                               edge_length_bytes + edge_speed_bytes + edge_class_bytes +
                               edge_speed_limit_bytes + edge_grade_bytes + edge_penalty_bytes;

    uint8_t *blob = (uint8_t *)malloc(total_bytes);
    if (!blob) {
        free(node_x);
        free(node_y);
        free(edge_start);
        free(edge_to);
        free(edge_length);
        free(edge_speed);
        free(edge_speed_limit);
        free(edge_grade);
        free(edge_penalty);
        free(edge_class);
        return false;
    }
    uint8_t *out_ptr = blob;
    uint32_t version = GRAPH_VERSION_V2;
    memcpy(out_ptr, GRAPH_MAGIC, 4u);
    out_ptr += 4u;
    memcpy(out_ptr, &version, sizeof(uint32_t));
    out_ptr += sizeof(uint32_t);
    memcpy(out_ptr, &node_count, sizeof(uint32_t));
    out_ptr += sizeof(uint32_t);
    memcpy(out_ptr, &out_edge_count, sizeof(uint32_t));
    out_ptr += sizeof(uint32_t);
    memcpy(out_ptr, node_x, (size_t)node_count * sizeof(double));
    out_ptr += (size_t)node_count * sizeof(double);
    memcpy(out_ptr, node_y, (size_t)node_count * sizeof(double));
    out_ptr += (size_t)node_count * sizeof(double);
    memcpy(out_ptr, edge_start, (size_t)(node_count + 1u) * sizeof(uint32_t));
    out_ptr += (size_t)(node_count + 1u) * sizeof(uint32_t);
    memcpy(out_ptr, edge_to, (size_t)out_edge_count * sizeof(uint32_t));
    out_ptr += (size_t)out_edge_count * sizeof(uint32_t);
    memcpy(out_ptr, edge_length, (size_t)out_edge_count * sizeof(float));
    out_ptr += (size_t)out_edge_count * sizeof(float);
    memcpy(out_ptr, edge_speed, (size_t)out_edge_count * sizeof(float));
    out_ptr += (size_t)out_edge_count * sizeof(float);
    memcpy(out_ptr, edge_class, (size_t)out_edge_count * sizeof(uint8_t));
    out_ptr += (size_t)out_edge_count * sizeof(uint8_t);
    memcpy(out_ptr, edge_speed_limit, (size_t)out_edge_count * sizeof(float));
    out_ptr += (size_t)out_edge_count * sizeof(float);
    memcpy(out_ptr, edge_grade, (size_t)out_edge_count * sizeof(float));
    out_ptr += (size_t)out_edge_count * sizeof(float);
    memcpy(out_ptr, edge_penalty, (size_t)out_edge_count * sizeof(float));

    CoreResult write_r = core_io_write_all(path, blob, total_bytes);
    free(blob);

    free(node_x);
    free(node_y);
    free(edge_start);
    free(edge_to);
    free(edge_length);
    free(edge_speed);
    free(edge_speed_limit);
    free(edge_grade);
    free(edge_penalty);
    free(edge_class);

    return write_r.code == CORE_OK;
}
