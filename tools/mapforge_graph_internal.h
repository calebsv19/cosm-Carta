#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GRAPH_MAGIC "MFG1"
#define GRAPH_VERSION_V1 1u
#define GRAPH_VERSION_V2 2u
#define MAPFORGE_SOURCE_PATH_CAPACITY 1024

typedef struct NodeEntry {
    int64_t id;
    double lat;
    double lon;
    bool used;
} NodeEntry;

typedef struct NodeMap {
    NodeEntry *entries;
    size_t capacity;
    size_t count;
} NodeMap;

typedef struct WayNodes {
    int64_t *items;
    size_t count;
    size_t capacity;
} WayNodes;

typedef struct Edge {
    int64_t from_id;
    int64_t to_id;
    float length_m;
    float speed_mps;
    float speed_limit_mps;
    float grade;
    float objective_penalty;
    uint8_t road_class;
} Edge;

typedef struct IndexEntry {
    int64_t id;
    uint32_t index;
    bool used;
} IndexEntry;

typedef struct IndexMap {
    IndexEntry *entries;
    size_t capacity;
    size_t count;
} IndexMap;

typedef struct GraphBuild {
    NodeMap nodes;
    int64_t *node_ids;
    IndexMap index_map;
    size_t node_count;
    size_t node_capacity;
    Edge *edges;
    size_t edge_count;
    size_t edge_capacity;
} GraphBuild;

typedef struct GraphOptions {
    const char *region;
    const char *osm_path;
    const char *out_dir;
    bool replace;
    uint32_t keep_old;
    uint32_t prune_days;
    bool prune_dry_run;
} GraphOptions;

bool node_map_get(const NodeMap *map, int64_t id, double *out_lat, double *out_lon);
bool index_map_get(const IndexMap *map, int64_t id, uint32_t *out_index);
