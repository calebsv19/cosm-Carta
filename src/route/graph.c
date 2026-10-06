#include "route/graph.h"

#include "core_io.h"
#include "core/log.h"
#include "map/geodesic.h"

#include <math.h>
#include <limits.h>

#include <stdlib.h>
#include <string.h>

#define GRAPH_VERSION_V1 1u
#define GRAPH_VERSION_V2 2u
#define GRAPH_VERSION_V3 3u
#define GRAPH_VERSION_V4 4u

typedef struct GraphCursor {
    const uint8_t *data;
    size_t size;
    size_t pos;
} GraphCursor;

static bool graph_read_exact(GraphCursor *cursor, void *buffer, size_t bytes) {
    if (!cursor || !buffer) return false;
    if (bytes > cursor->size - cursor->pos) return false;
    memcpy(buffer, cursor->data + cursor->pos, bytes);
    cursor->pos += bytes;
    return true;
}

bool route_graph_load(const char *path, RouteGraph *graph) {
    CoreBuffer file_data = {0};
    CoreResult read_result;
    GraphCursor cursor = {0};
    if (!path || !graph) {
        return false;
    }

    memset(graph, 0, sizeof(*graph));

    read_result = core_io_read_all(path, &file_data);
    if (read_result.code != CORE_OK) {
        log_error("Failed to open graph: %s", path);
        return false;
    }
    cursor.data = file_data.data;
    cursor.size = file_data.size;
    cursor.pos = 0u;

    char magic[4] = {0};
    uint32_t version = 0;
    if (!graph_read_exact(&cursor, magic, sizeof(magic)) || memcmp(magic, "MFG1", 4) != 0) {
        log_error("Graph invalid magic: %s", path);
        core_io_buffer_free(&file_data);
        return false;
    }

    if (!graph_read_exact(&cursor, &version, sizeof(version)) ||
        (version != GRAPH_VERSION_V1 && version != GRAPH_VERSION_V2 && version != GRAPH_VERSION_V3 && version != GRAPH_VERSION_V4)) {
        log_error("Graph unsupported version: %s", path);
        core_io_buffer_free(&file_data);
        return false;
    }
    graph->version = version;

    if (!graph_read_exact(&cursor, &graph->node_count, sizeof(uint32_t)) ||
        !graph_read_exact(&cursor, &graph->edge_count, sizeof(uint32_t))) {
        log_error("Graph header truncated: %s", path);
        core_io_buffer_free(&file_data);
        return false;
    }

    size_t required_bytes = (size_t)graph->node_count * 20u + 4u +
        (size_t)graph->edge_count * (version >= GRAPH_VERSION_V4 ? 26u : version >= GRAPH_VERSION_V2 ? 25u : 13u);
    if (graph->node_count == 0u || graph->node_count > INT32_MAX ||
        required_bytes != cursor.size - cursor.pos) {
        log_error("Graph counts/payload invalid: %s", path);
        core_io_buffer_free(&file_data);
        memset(graph, 0, sizeof(*graph));
        return false;
    }

    graph->node_x = (double *)malloc(sizeof(double) * graph->node_count);
    graph->node_y = (double *)malloc(sizeof(double) * graph->node_count);
    graph->edge_start = (uint32_t *)malloc(sizeof(uint32_t) * (graph->node_count + 1));
    graph->edge_to = (uint32_t *)malloc(sizeof(uint32_t) * graph->edge_count);
    graph->edge_length = (float *)malloc(sizeof(float) * graph->edge_count);
    graph->edge_speed = (float *)malloc(sizeof(float) * graph->edge_count);
    graph->edge_speed_limit = (float *)calloc(graph->edge_count, sizeof(float));
    graph->edge_grade = (float *)calloc(graph->edge_count, sizeof(float));
    graph->edge_penalty = (float *)calloc(graph->edge_count, sizeof(float));
    graph->edge_class = (uint8_t *)malloc(sizeof(uint8_t) * graph->edge_count);

    if (!graph->node_x || !graph->node_y || !graph->edge_start || !graph->edge_to ||
        !graph->edge_length || !graph->edge_speed || !graph->edge_speed_limit ||
        !graph->edge_grade || !graph->edge_penalty || !graph->edge_class) {
        route_graph_free(graph);
        core_io_buffer_free(&file_data);
        return false;
    }

    if (!graph_read_exact(&cursor, graph->node_x, sizeof(double) * graph->node_count) ||
        !graph_read_exact(&cursor, graph->node_y, sizeof(double) * graph->node_count) ||
        !graph_read_exact(&cursor, graph->edge_start, sizeof(uint32_t) * (graph->node_count + 1u)) ||
        !graph_read_exact(&cursor, graph->edge_to, sizeof(uint32_t) * graph->edge_count) ||
        !graph_read_exact(&cursor, graph->edge_length, sizeof(float) * graph->edge_count) ||
        !graph_read_exact(&cursor, graph->edge_speed, sizeof(float) * graph->edge_count) ||
        !graph_read_exact(&cursor, graph->edge_class, sizeof(uint8_t) * graph->edge_count)) {
        log_error("Graph data truncated: %s", path);
        route_graph_free(graph);
        core_io_buffer_free(&file_data);
        return false;
    }

    if (version >= GRAPH_VERSION_V2) {
        if (!graph_read_exact(&cursor, graph->edge_speed_limit, sizeof(float) * graph->edge_count) ||
            !graph_read_exact(&cursor, graph->edge_grade, sizeof(float) * graph->edge_count) ||
            !graph_read_exact(&cursor, graph->edge_penalty, sizeof(float) * graph->edge_count)) {
            log_error("Graph v2 data truncated: %s", path);
            route_graph_free(graph);
            core_io_buffer_free(&file_data);
            return false;
        }
    } else {
        for (uint32_t i = 0; i < graph->edge_count; ++i) {
            graph->edge_speed_limit[i] = graph->edge_speed[i];
        }
    }

    if (version >= GRAPH_VERSION_V4) {
        graph->edge_access = malloc(graph->edge_count);
        if (!graph->edge_access || !graph_read_exact(&cursor, graph->edge_access, graph->edge_count)) {
            route_graph_free(graph); core_io_buffer_free(&file_data); return false;
        }
    }
    bool valid = graph->edge_start[0] == 0u &&
                 graph->edge_start[graph->node_count] == graph->edge_count;
    for (uint32_t node = 0u; valid && node < graph->node_count; ++node) {
        valid = isfinite(graph->node_x[node]) && isfinite(graph->node_y[node]) &&
                graph->edge_start[node] <= graph->edge_start[node + 1u] &&
                graph->edge_start[node + 1u] <= graph->edge_count;
        for (uint32_t edge = graph->edge_start[node]; valid && edge < graph->edge_start[node + 1u]; ++edge) {
            uint32_t to = graph->edge_to[edge];
            valid = to < graph->node_count && isfinite(graph->edge_length[edge]) &&
                    graph->edge_length[edge] >= 0.0f && isfinite(graph->edge_speed[edge]) &&
                    graph->edge_speed[edge] >= 0.0f && isfinite(graph->edge_grade[edge]) &&
                    isfinite(graph->edge_penalty[edge]) && graph->edge_penalty[edge] >= 0.0f &&
                    isfinite(graph->edge_speed_limit[edge]) && graph->edge_speed_limit[edge] >= 0.0f &&
                    graph->edge_class[edge] <= 8u && (!graph->edge_access || (graph->edge_access[edge] > 0u && graph->edge_access[edge] <= 3u));
            if (!valid) break;
            double ground = mercator_ground_distance_meters(
                (MercatorMeters){graph->node_x[node], graph->node_y[node]},
                (MercatorMeters){graph->node_x[to], graph->node_y[to]});
            if (!isfinite(ground)) { valid = false; break; }
            if (version < GRAPH_VERSION_V3) {
                graph->edge_length[edge] = (float)ground;
            } else if (fabs((double)graph->edge_length[edge] - ground) > fmax(0.01, ground * 0.00001)) {
                valid = false;
            }
        }
    }
    core_io_buffer_free(&file_data);
    if (!valid) {
        log_error("Graph adjacency or ground metric invalid: %s", path);
        route_graph_free(graph);
        return false;
    }
    graph->legacy_lengths_normalized = version < GRAPH_VERSION_V3;
    if (graph->legacy_lengths_normalized) {
        log_info("Graph v%u projected lengths normalized to spherical ground meters: %s", version, path);
    }
    return true;
}

void route_graph_free(RouteGraph *graph) {
    if (!graph) {
        return;
    }

    free(graph->node_x);
    free(graph->node_y);
    free(graph->edge_start);
    free(graph->edge_to);
    free(graph->edge_length);
    free(graph->edge_speed);
    free(graph->edge_speed_limit);
    free(graph->edge_grade);
    free(graph->edge_penalty);
    free(graph->edge_class);
    free(graph->edge_access);
    memset(graph, 0, sizeof(*graph));
}
