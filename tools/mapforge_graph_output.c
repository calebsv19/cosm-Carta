#include "mapforge_graph_output.h"

#include "core/log.h"
#include "core_io.h"
#include "map/mercator.h"
#include "mapforge_publish_support.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

bool ensure_dir_recursive(const char *path) {
    return mapforge_publish_ensure_dir_recursive(path);
}

bool remove_tree(const char *path) {
    return mapforge_publish_remove_tree(path);
}

static void prune_snapshot_dir(const char *snapshot_root, uint32_t keep_old, uint32_t prune_days, bool dry_run) {
    mapforge_publish_prune_snapshot_dir(snapshot_root,
                                        keep_old,
                                        prune_days,
                                        dry_run,
                                        "dry-run prune graph snapshot: %s",
                                        "Failed to prune graph snapshot: %s",
                                        NULL);
}

void prune_staging_dirs(const char *staging_root, uint32_t prune_days, bool dry_run) {
    mapforge_publish_prune_staging_dirs(staging_root,
                                        prune_days,
                                        dry_run,
                                        "dry-run prune graph staging dir: %s",
                                        "Failed to prune graph staging dir: %s",
                                        NULL);
}

bool build_publish_paths(const char *active_root,
                         char *out_stage_root,
                         size_t stage_size,
                         char *out_snapshot_root,
                         size_t snapshot_size,
                         char *out_staging_root,
                         size_t staging_root_size) {
    return mapforge_publish_build_paths(active_root,
                                        ".graph_staging",
                                        ".graph_snapshots",
                                        out_stage_root,
                                        stage_size,
                                        out_snapshot_root,
                                        snapshot_size,
                                        out_staging_root,
                                        staging_root_size);
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

    if (mapforge_publish_path_exists(active_graph)) {
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
    if (!mapforge_publish_ensure_dir_recursive(out_dir)) {
        return false;
    }
    if (!mapforge_publish_ensure_dir_recursive(graph_dir)) {
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
