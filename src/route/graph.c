#include "route/graph.h"

#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool read_exact(FILE *file, void *buffer, size_t size) {
    return fread(buffer, 1, size, file) == size;
}

bool route_graph_load(const char *path, RouteGraph *graph) {
    if (!path || !graph) {
        return false;
    }

    memset(graph, 0, sizeof(*graph));

    FILE *file = fopen(path, "rb");
    if (!file) {
        log_error("Failed to open graph: %s", path);
        return false;
    }

    char magic[4] = {0};
    uint32_t version = 0;
    if (!read_exact(file, magic, sizeof(magic)) || memcmp(magic, "MFG1", 4) != 0) {
        log_error("Graph invalid magic: %s", path);
        fclose(file);
        return false;
    }

    if (!read_exact(file, &version, sizeof(version)) || version != 1) {
        log_error("Graph unsupported version: %s", path);
        fclose(file);
        return false;
    }

    if (!read_exact(file, &graph->node_count, sizeof(uint32_t)) ||
        !read_exact(file, &graph->edge_count, sizeof(uint32_t))) {
        log_error("Graph header truncated: %s", path);
        fclose(file);
        return false;
    }

    graph->node_x = (double *)malloc(sizeof(double) * graph->node_count);
    graph->node_y = (double *)malloc(sizeof(double) * graph->node_count);
    graph->edge_start = (uint32_t *)malloc(sizeof(uint32_t) * (graph->node_count + 1));
    graph->edge_to = (uint32_t *)malloc(sizeof(uint32_t) * graph->edge_count);
    graph->edge_length = (float *)malloc(sizeof(float) * graph->edge_count);
    graph->edge_speed = (float *)malloc(sizeof(float) * graph->edge_count);
    graph->edge_class = (uint8_t *)malloc(sizeof(uint8_t) * graph->edge_count);

    if (!graph->node_x || !graph->node_y || !graph->edge_start || !graph->edge_to ||
        !graph->edge_length || !graph->edge_speed || !graph->edge_class) {
        route_graph_free(graph);
        fclose(file);
        return false;
    }

    if (!read_exact(file, graph->node_x, sizeof(double) * graph->node_count) ||
        !read_exact(file, graph->node_y, sizeof(double) * graph->node_count) ||
        !read_exact(file, graph->edge_start, sizeof(uint32_t) * (graph->node_count + 1)) ||
        !read_exact(file, graph->edge_to, sizeof(uint32_t) * graph->edge_count) ||
        !read_exact(file, graph->edge_length, sizeof(float) * graph->edge_count) ||
        !read_exact(file, graph->edge_speed, sizeof(float) * graph->edge_count) ||
        !read_exact(file, graph->edge_class, sizeof(uint8_t) * graph->edge_count)) {
        log_error("Graph data truncated: %s", path);
        route_graph_free(graph);
        fclose(file);
        return false;
    }

    fclose(file);
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
    free(graph->edge_class);
    memset(graph, 0, sizeof(*graph));
}
