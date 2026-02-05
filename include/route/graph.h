#ifndef MAPFORGE_ROUTE_GRAPH_H
#define MAPFORGE_ROUTE_GRAPH_H

#include <stdbool.h>
#include <stdint.h>

// Stores a routing graph loaded from disk.
typedef struct RouteGraph {
    uint32_t node_count;
    uint32_t edge_count;
    double *node_x;
    double *node_y;
    uint32_t *edge_start;
    uint32_t *edge_to;
    float *edge_length;
    float *edge_speed;
    uint8_t *edge_class;
} RouteGraph;

// Loads a routing graph from a graph.bin file.
bool route_graph_load(const char *path, RouteGraph *graph);

// Releases memory associated with a graph.
void route_graph_free(RouteGraph *graph);

#endif
