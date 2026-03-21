#include "core/log.h"
#include "core_io.h"
#include "map/mercator.h"
#include "map/mft_loader.h"
#include "map/tile_math.h"

#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define GRAPH_MAGIC "MFG1"
#define GRAPH_VERSION_V1 1u
#define GRAPH_VERSION_V2 2u

// Stores a node ID and coordinates.
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

// Stores node IDs for a way.
typedef struct WayNodes {
    int64_t *items;
    size_t count;
    size_t capacity;
} WayNodes;

// Stores a graph edge keyed by node IDs.
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

// Stores a node ID to index mapping.
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

// Stores build state.
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

// CLI options.
typedef struct GraphOptions {
    const char *region;
    const char *osm_path;
    const char *out_dir;
    bool replace;
    uint32_t keep_old;
    uint32_t prune_days;
    bool prune_dry_run;
} GraphOptions;

static uint64_t hash_id(int64_t id) {
    return (uint64_t)id * 11400714819323198485ull;
}

static bool node_map_init(NodeMap *map, size_t capacity) {
    if (!map || capacity == 0) {
        return false;
    }

    map->entries = (NodeEntry *)calloc(capacity, sizeof(NodeEntry));
    if (!map->entries) {
        return false;
    }

    map->capacity = capacity;
    map->count = 0;
    return true;
}

static void node_map_free(NodeMap *map) {
    if (!map) {
        return;
    }

    free(map->entries);
    memset(map, 0, sizeof(*map));
}

static bool node_map_rehash(NodeMap *map, size_t new_capacity) {
    NodeEntry *old_entries = map->entries;
    size_t old_capacity = map->capacity;

    if (!node_map_init(map, new_capacity)) {
        return false;
    }

    for (size_t i = 0; i < old_capacity; ++i) {
        NodeEntry entry = old_entries[i];
        if (!entry.used) {
            continue;
        }

        uint64_t hash = hash_id(entry.id);
        size_t mask = map->capacity - 1;
        size_t index = (size_t)hash & mask;

        while (map->entries[index].used) {
            index = (index + 1) & mask;
        }

        map->entries[index] = entry;
        map->count += 1;
    }

    free(old_entries);
    return true;
}

static bool node_map_put(NodeMap *map, int64_t id, double lat, double lon) {
    if (!map) {
        return false;
    }

    if (map->count * 10 >= map->capacity * 7) {
        if (!node_map_rehash(map, map->capacity * 2)) {
            return false;
        }
    }

    uint64_t hash = hash_id(id);
    size_t mask = map->capacity - 1;
    size_t index = (size_t)hash & mask;

    while (map->entries[index].used) {
        if (map->entries[index].id == id) {
            map->entries[index].lat = lat;
            map->entries[index].lon = lon;
            return true;
        }
        index = (index + 1) & mask;
    }

    map->entries[index].used = true;
    map->entries[index].id = id;
    map->entries[index].lat = lat;
    map->entries[index].lon = lon;
    map->count += 1;
    return true;
}

static bool node_map_get(const NodeMap *map, int64_t id, double *out_lat, double *out_lon) {
    if (!map || !out_lat || !out_lon) {
        return false;
    }

    uint64_t hash = hash_id(id);
    size_t mask = map->capacity - 1;
    size_t index = (size_t)hash & mask;

    while (map->entries[index].used) {
        if (map->entries[index].id == id) {
            *out_lat = map->entries[index].lat;
            *out_lon = map->entries[index].lon;
            return true;
        }
        index = (index + 1) & mask;
    }

    return false;
}

static bool index_map_init(IndexMap *map, size_t capacity) {
    if (!map || capacity == 0) {
        return false;
    }

    map->entries = (IndexEntry *)calloc(capacity, sizeof(IndexEntry));
    if (!map->entries) {
        return false;
    }

    map->capacity = capacity;
    map->count = 0;
    return true;
}

static void index_map_free(IndexMap *map) {
    if (!map) {
        return;
    }

    free(map->entries);
    memset(map, 0, sizeof(*map));
}

static bool index_map_rehash(IndexMap *map, size_t new_capacity) {
    IndexEntry *old_entries = map->entries;
    size_t old_capacity = map->capacity;

    if (!index_map_init(map, new_capacity)) {
        return false;
    }

    for (size_t i = 0; i < old_capacity; ++i) {
        IndexEntry entry = old_entries[i];
        if (!entry.used) {
            continue;
        }

        uint64_t hash = hash_id(entry.id);
        size_t mask = map->capacity - 1;
        size_t index = (size_t)hash & mask;

        while (map->entries[index].used) {
            index = (index + 1) & mask;
        }

        map->entries[index] = entry;
        map->count += 1;
    }

    free(old_entries);
    return true;
}

static bool index_map_put(IndexMap *map, int64_t id, uint32_t index_value) {
    if (!map) {
        return false;
    }

    if (map->count * 10 >= map->capacity * 7) {
        if (!index_map_rehash(map, map->capacity * 2)) {
            return false;
        }
    }

    uint64_t hash = hash_id(id);
    size_t mask = map->capacity - 1;
    size_t index = (size_t)hash & mask;

    while (map->entries[index].used) {
        if (map->entries[index].id == id) {
            map->entries[index].index = index_value;
            return true;
        }
        index = (index + 1) & mask;
    }

    map->entries[index].used = true;
    map->entries[index].id = id;
    map->entries[index].index = index_value;
    map->count += 1;
    return true;
}

static bool index_map_get(const IndexMap *map, int64_t id, uint32_t *out_index) {
    if (!map || !out_index) {
        return false;
    }

    uint64_t hash = hash_id(id);
    size_t mask = map->capacity - 1;
    size_t index = (size_t)hash & mask;

    while (map->entries[index].used) {
        if (map->entries[index].id == id) {
            *out_index = map->entries[index].index;
            return true;
        }
        index = (index + 1) & mask;
    }

    return false;
}

static void way_nodes_init(WayNodes *nodes) {
    if (!nodes) {
        return;
    }

    nodes->items = NULL;
    nodes->count = 0;
    nodes->capacity = 0;
}

static void way_nodes_clear(WayNodes *nodes) {
    if (!nodes) {
        return;
    }

    free(nodes->items);
    nodes->items = NULL;
    nodes->count = 0;
    nodes->capacity = 0;
}

static bool way_nodes_push(WayNodes *nodes, int64_t id) {
    if (!nodes) {
        return false;
    }

    if (nodes->count == nodes->capacity) {
        size_t next = nodes->capacity == 0 ? 64 : nodes->capacity * 2;
        int64_t *next_items = (int64_t *)realloc(nodes->items, next * sizeof(int64_t));
        if (!next_items) {
            return false;
        }
        nodes->items = next_items;
        nodes->capacity = next;
    }

    nodes->items[nodes->count++] = id;
    return true;
}

static bool xml_attr(const char *line, const char *key, char *out, size_t out_size) {
    if (!line || !key || !out || out_size == 0) {
        return false;
    }

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=\"", key);

    const char *start = strstr(line, pattern);
    if (!start) {
        return false;
    }

    start += strlen(pattern);
    const char *end = strchr(start, '"');
    if (!end) {
        return false;
    }

    size_t len = (size_t)(end - start);
    if (len >= out_size) {
        len = out_size - 1;
    }

    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

static RoadClass road_class_from_highway(const char *value) {
    if (!value) {
        return ROAD_CLASS_RESIDENTIAL;
    }

    if (strcmp(value, "motorway") == 0 || strcmp(value, "motorway_link") == 0) {
        return ROAD_CLASS_MOTORWAY;
    }
    if (strcmp(value, "trunk") == 0 || strcmp(value, "trunk_link") == 0) {
        return ROAD_CLASS_TRUNK;
    }
    if (strcmp(value, "primary") == 0 || strcmp(value, "primary_link") == 0) {
        return ROAD_CLASS_PRIMARY;
    }
    if (strcmp(value, "secondary") == 0 || strcmp(value, "secondary_link") == 0) {
        return ROAD_CLASS_SECONDARY;
    }
    if (strcmp(value, "tertiary") == 0 || strcmp(value, "tertiary_link") == 0) {
        return ROAD_CLASS_TERTIARY;
    }
    if (strcmp(value, "service") == 0) {
        return ROAD_CLASS_SERVICE;
    }
    if (strcmp(value, "footway") == 0 || strcmp(value, "cycleway") == 0 ||
        strcmp(value, "pedestrian") == 0 || strcmp(value, "steps") == 0) {
        return ROAD_CLASS_FOOTWAY;
    }
    if (strcmp(value, "path") == 0 || strcmp(value, "track") == 0) {
        return ROAD_CLASS_PATH;
    }

    return ROAD_CLASS_RESIDENTIAL;
}

static float speed_for_class(RoadClass road_class) {
    switch (road_class) {
        case ROAD_CLASS_MOTORWAY:
            return 30.0f;
        case ROAD_CLASS_TRUNK:
            return 24.0f;
        case ROAD_CLASS_PRIMARY:
            return 20.0f;
        case ROAD_CLASS_SECONDARY:
            return 16.0f;
        case ROAD_CLASS_TERTIARY:
            return 13.0f;
        case ROAD_CLASS_RESIDENTIAL:
            return 10.0f;
        case ROAD_CLASS_SERVICE:
            return 7.0f;
        case ROAD_CLASS_FOOTWAY:
            return 5.0f;
        case ROAD_CLASS_PATH:
            return 3.0f;
        default:
            return 10.0f;
    }
}

static bool parse_maxspeed_mps(const char *value, float *out_speed_mps) {
    if (!value || !out_speed_mps || value[0] == '\0') {
        return false;
    }

    while (*value != '\0' && isspace((unsigned char)*value)) {
        value++;
    }
    if (*value == '\0') {
        return false;
    }

    char *end_ptr = NULL;
    double parsed = strtod(value, &end_ptr);
    if (end_ptr == value || parsed <= 0.0) {
        return false;
    }

    while (*end_ptr != '\0' && isspace((unsigned char)*end_ptr)) {
        end_ptr++;
    }

    /* OSM maxspeed without units is conventionally km/h. */
    double speed_mps = parsed / 3.6;
    if (strncasecmp(end_ptr, "mph", 3) == 0) {
        speed_mps = parsed * 0.44704;
    } else if (strncasecmp(end_ptr, "km/h", 4) == 0 || strncasecmp(end_ptr, "kph", 3) == 0) {
        speed_mps = parsed / 3.6;
    }

    if (speed_mps <= 0.1) {
        return false;
    }

    *out_speed_mps = (float)speed_mps;
    return true;
}

static bool parse_incline_grade(const char *value, float *out_grade) {
    if (!value || !out_grade || value[0] == '\0') {
        return false;
    }

    while (*value != '\0' && isspace((unsigned char)*value)) {
        value++;
    }
    if (*value == '\0' || strcasecmp(value, "up") == 0 || strcasecmp(value, "down") == 0) {
        return false;
    }

    char *end_ptr = NULL;
    double parsed = strtod(value, &end_ptr);
    if (end_ptr == value) {
        return false;
    }
    while (*end_ptr != '\0' && isspace((unsigned char)*end_ptr)) {
        end_ptr++;
    }

    double grade = parsed;
    if (*end_ptr == '%') {
        grade = parsed / 100.0;
    } else if (fabs(parsed) > 1.0) {
        /* Treat unqualified values above +/-1 as percent-like. */
        grade = parsed / 100.0;
    }

    if (!isfinite(grade)) {
        return false;
    }
    *out_grade = (float)grade;
    return true;
}

static float objective_penalty_for_surface(const char *value) {
    if (!value || value[0] == '\0') {
        return 0.0f;
    }
    if (strcasecmp(value, "unpaved") == 0 || strcasecmp(value, "ground") == 0) {
        return 0.12f;
    }
    if (strcasecmp(value, "gravel") == 0 || strcasecmp(value, "compacted") == 0) {
        return 0.18f;
    }
    if (strcasecmp(value, "dirt") == 0 || strcasecmp(value, "earth") == 0 || strcasecmp(value, "mud") == 0) {
        return 0.25f;
    }
    if (strcasecmp(value, "sand") == 0) {
        return 0.35f;
    }
    return 0.0f;
}

static bool graph_build_init(GraphBuild *build) {
    if (!build) {
        return false;
    }

    memset(build, 0, sizeof(*build));
    if (!node_map_init(&build->nodes, 1u << 20)) {
        return false;
    }
    if (!index_map_init(&build->index_map, 1u << 20)) {
        node_map_free(&build->nodes);
        return false;
    }
    return true;
}

static void graph_build_free(GraphBuild *build) {
    if (!build) {
        return;
    }

    free(build->node_ids);
    index_map_free(&build->index_map);
    free(build->edges);
    node_map_free(&build->nodes);
    memset(build, 0, sizeof(*build));
}

static bool graph_add_node(GraphBuild *build, int64_t id) {
    if (!build) {
        return false;
    }

    uint32_t existing = 0;
    if (index_map_get(&build->index_map, id, &existing)) {
        return true;
    }

    if (build->node_count == build->node_capacity) {
        size_t next = build->node_capacity == 0 ? 1024 : build->node_capacity * 2;
        int64_t *next_nodes = (int64_t *)realloc(build->node_ids, next * sizeof(int64_t));
        if (!next_nodes) {
            return false;
        }
        build->node_ids = next_nodes;
        build->node_capacity = next;
    }

    build->node_ids[build->node_count] = id;
    if (!index_map_put(&build->index_map, id, (uint32_t)build->node_count)) {
        return false;
    }
    build->node_count += 1;
    return true;
}

static bool graph_add_edge(GraphBuild *build,
                           int64_t from_id,
                           int64_t to_id,
                           float length_m,
                           float speed_mps,
                           float speed_limit_mps,
                           float grade,
                           float objective_penalty,
                           uint8_t road_class) {
    if (!build) {
        return false;
    }

    if (build->edge_count == build->edge_capacity) {
        size_t next = build->edge_capacity == 0 ? 2048 : build->edge_capacity * 2;
        Edge *next_edges = (Edge *)realloc(build->edges, next * sizeof(Edge));
        if (!next_edges) {
            return false;
        }
        build->edges = next_edges;
        build->edge_capacity = next;
    }

    Edge *edge = &build->edges[build->edge_count++];
    edge->from_id = from_id;
    edge->to_id = to_id;
    edge->length_m = length_m;
    edge->speed_mps = speed_mps;
    edge->speed_limit_mps = speed_limit_mps;
    edge->grade = grade;
    edge->objective_penalty = objective_penalty;
    edge->road_class = road_class;
    return true;
}

static bool parse_osm_nodes(GraphBuild *build, const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        log_error("Failed to open OSM file: %s", path);
        return false;
    }

    char line[8192];
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "<node ") != NULL) {
            char id_buf[32];
            char lat_buf[32];
            char lon_buf[32];
            if (!xml_attr(line, "id", id_buf, sizeof(id_buf)) ||
                !xml_attr(line, "lat", lat_buf, sizeof(lat_buf)) ||
                !xml_attr(line, "lon", lon_buf, sizeof(lon_buf))) {
                continue;
            }

            int64_t id = strtoll(id_buf, NULL, 10);
            double lat = strtod(lat_buf, NULL);
            double lon = strtod(lon_buf, NULL);
            node_map_put(&build->nodes, id, lat, lon);
        }
    }

    fclose(file);
    return true;
}

static bool parse_osm_ways(GraphBuild *build, const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        log_error("Failed to open OSM file: %s", path);
        return false;
    }

    WayNodes way_nodes;
    way_nodes_init(&way_nodes);
    bool in_way = false;
    char highway_tag[64] = {0};
    char oneway_tag[16] = {0};
    char maxspeed_tag[64] = {0};
    char incline_tag[64] = {0};
    char surface_tag[64] = {0};

    char line[8192];
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "<way ") != NULL) {
            in_way = true;
            highway_tag[0] = '\0';
            oneway_tag[0] = '\0';
            maxspeed_tag[0] = '\0';
            incline_tag[0] = '\0';
            surface_tag[0] = '\0';
            way_nodes_clear(&way_nodes);
            continue;
        }

        if (in_way) {
            if (strstr(line, "<nd ") != NULL) {
                char ref_buf[32];
                if (xml_attr(line, "ref", ref_buf, sizeof(ref_buf))) {
                    int64_t ref = strtoll(ref_buf, NULL, 10);
                    way_nodes_push(&way_nodes, ref);
                }
                continue;
            }

            if (strstr(line, "<tag ") != NULL) {
                char key_buf[64];
                char val_buf[64];
                if (xml_attr(line, "k", key_buf, sizeof(key_buf)) &&
                    xml_attr(line, "v", val_buf, sizeof(val_buf))) {
                    if (strcmp(key_buf, "highway") == 0) {
                        snprintf(highway_tag, sizeof(highway_tag), "%s", val_buf);
                    } else if (strcmp(key_buf, "oneway") == 0) {
                        snprintf(oneway_tag, sizeof(oneway_tag), "%s", val_buf);
                    } else if (strcmp(key_buf, "maxspeed") == 0) {
                        snprintf(maxspeed_tag, sizeof(maxspeed_tag), "%s", val_buf);
                    } else if (strcmp(key_buf, "incline") == 0) {
                        snprintf(incline_tag, sizeof(incline_tag), "%s", val_buf);
                    } else if (strcmp(key_buf, "surface") == 0) {
                        snprintf(surface_tag, sizeof(surface_tag), "%s", val_buf);
                    }
                }
                continue;
            }

            if (strstr(line, "</way>") != NULL) {
                if (highway_tag[0] != '\0' && way_nodes.count >= 2) {
                    for (size_t i = 0; i < way_nodes.count; ++i) {
                        graph_add_node(build, way_nodes.items[i]);
                    }

                    RoadClass road_class = road_class_from_highway(highway_tag);
                    float speed = speed_for_class(road_class);
                    float maxspeed_mps = 0.0f;
                    if (parse_maxspeed_mps(maxspeed_tag, &maxspeed_mps)) {
                        speed = maxspeed_mps;
                    }
                    float speed_limit_mps = maxspeed_mps;
                    float grade = 0.0f;
                    parse_incline_grade(incline_tag, &grade);
                    float objective_penalty = objective_penalty_for_surface(surface_tag);
                    int oneway = 0;
                    if (strcmp(oneway_tag, "yes") == 0 || strcmp(oneway_tag, "true") == 0 || strcmp(oneway_tag, "1") == 0) {
                        oneway = 1;
                    } else if (strcmp(oneway_tag, "-1") == 0) {
                        oneway = -1;
                    }

                    for (size_t i = 1; i < way_nodes.count; ++i) {
                        int64_t a_id = way_nodes.items[i - 1];
                        int64_t b_id = way_nodes.items[i];

                        double lat_a = 0.0;
                        double lon_a = 0.0;
                        double lat_b = 0.0;
                        double lon_b = 0.0;
                        if (!node_map_get(&build->nodes, a_id, &lat_a, &lon_a) ||
                            !node_map_get(&build->nodes, b_id, &lat_b, &lon_b)) {
                            continue;
                        }

                        MercatorMeters a = mercator_from_latlon((LatLon){lat_a, lon_a});
                        MercatorMeters b = mercator_from_latlon((LatLon){lat_b, lon_b});
                        double dx = b.x - a.x;
                        double dy = b.y - a.y;
                        float length_m = (float)sqrt(dx * dx + dy * dy);

                        if (oneway >= 0) {
                            graph_add_edge(build, a_id, b_id, length_m, speed, speed_limit_mps, grade, objective_penalty, (uint8_t)road_class);
                        }
                        if (oneway <= 0) {
                            graph_add_edge(build, b_id, a_id, length_m, speed, speed_limit_mps, -grade, objective_penalty, (uint8_t)road_class);
                        }
                    }
                }

                in_way = false;
                continue;
            }
        }
    }

    way_nodes_clear(&way_nodes);
    fclose(file);
    return true;
}

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

static bool ensure_dir_recursive(const char *path) {
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

static bool remove_tree(const char *path) {
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

static void prune_staging_dirs(const char *staging_root, uint32_t prune_days, bool dry_run) {
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

static bool build_publish_paths(const char *active_root,
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

static bool validate_staged_graph(const char *stage_root) {
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

static bool publish_staged_graph(const GraphOptions *options,
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

static bool write_graph(const GraphBuild *build, const char *out_dir) {
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

static void usage(void) {
    printf("mapforge_graph --region <name> --osm <file.osm> --out <dir> [--replace] [--keep-old N] [--prune-days N] [--prune-dry-run]\n");
}

static bool parse_args(int argc, char **argv, GraphOptions *options) {
    if (!options) {
        return false;
    }

    memset(options, 0, sizeof(*options));
    options->keep_old = 1u;
    options->prune_days = 30u;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--region") == 0 && i + 1 < argc) {
            options->region = argv[++i];
        } else if (strcmp(argv[i], "--osm") == 0 && i + 1 < argc) {
            options->osm_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            options->out_dir = argv[++i];
        } else if (strcmp(argv[i], "--replace") == 0) {
            options->replace = true;
        } else if (strcmp(argv[i], "--keep-old") == 0 && i + 1 < argc) {
            options->keep_old = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--prune-days") == 0 && i + 1 < argc) {
            options->prune_days = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--prune-dry-run") == 0) {
            options->prune_dry_run = true;
        } else {
            return false;
        }
    }

    if (!options->region || !options->osm_path || !options->out_dir) {
        return false;
    }

    return true;
}

int main(int argc, char **argv) {
    GraphOptions options;
    char active_out_dir[512];
    char stage_root[512];
    char snapshot_root[512];
    char staging_root[512];
    bool stage_created = false;

    if (!parse_args(argc, argv, &options)) {
        usage();
        return 1;
    }

    snprintf(active_out_dir, sizeof(active_out_dir), "%s", options.out_dir);
    if (!build_publish_paths(active_out_dir, stage_root, sizeof(stage_root), snapshot_root, sizeof(snapshot_root), staging_root, sizeof(staging_root))) {
        log_error("Failed to build graph publish paths for output: %s", active_out_dir);
        return 1;
    }
    if (!ensure_dir_recursive(staging_root)) {
        log_error("Failed to create graph staging root: %s", staging_root);
        return 1;
    }
    if (!options.replace && !ensure_dir_recursive(snapshot_root)) {
        log_error("Failed to create graph snapshot root: %s", snapshot_root);
        return 1;
    }
    if (!remove_tree(stage_root)) {
        log_error("Failed to clear graph stage path: %s", stage_root);
        return 1;
    }
    if (!ensure_dir_recursive(stage_root)) {
        log_error("Failed to create graph stage path: %s", stage_root);
        return 1;
    }
    stage_created = true;

    GraphBuild build;
    if (!graph_build_init(&build)) {
        if (stage_created) {
            remove_tree(stage_root);
        }
        return 1;
    }

    if (!parse_osm_nodes(&build, options.osm_path)) {
        graph_build_free(&build);
        if (stage_created) {
            remove_tree(stage_root);
        }
        return 1;
    }

    if (!parse_osm_ways(&build, options.osm_path)) {
        graph_build_free(&build);
        if (stage_created) {
            remove_tree(stage_root);
        }
        return 1;
    }

    if (!write_graph(&build, stage_root)) {
        graph_build_free(&build);
        if (stage_created) {
            remove_tree(stage_root);
        }
        return 1;
    }

    if (!validate_staged_graph(stage_root)) {
        graph_build_free(&build);
        if (stage_created) {
            remove_tree(stage_root);
        }
        return 1;
    }

    if (!publish_staged_graph(&options, stage_root, active_out_dir, snapshot_root)) {
        graph_build_free(&build);
        if (stage_created) {
            remove_tree(stage_root);
        }
        return 1;
    }

    graph_build_free(&build);
    if (!remove_tree(stage_root)) {
        log_info("graph stage cleanup skipped: %s", stage_root);
    }
    prune_staging_dirs(staging_root, options.prune_days, options.prune_dry_run);
    log_info("Graph generated at %s/graph/graph.bin", active_out_dir);
    return 0;
}
