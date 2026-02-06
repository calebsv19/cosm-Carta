#include "core/log.h"
#include "map/mercator.h"
#include "map/mft_loader.h"
#include "map/tile_math.h"

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define TILE_EXTENT 4096.0

// Stores an OSM node coordinate keyed by ID.
typedef struct NodeEntry {
    int64_t id;
    double lat;
    double lon;
    bool used;
} NodeEntry;

// Stores a hash map of OSM nodes for lookup by ID.
typedef struct NodeMap {
    NodeEntry *entries;
    size_t capacity;
    size_t count;
} NodeMap;

// Stores a dynamic list of node IDs for a way.
typedef struct WayNodes {
    int64_t *items;
    size_t count;
    size_t capacity;
} WayNodes;

// Stores a temporary polyline (quantized points) for a tile.
typedef struct TilePolyline {
    RoadClass road_class;
    uint32_t point_count;
    uint16_t *points;
} TilePolyline;

// Stores a temporary polygon (quantized points) for a tile.
typedef struct TilePolygon {
    PolygonClass polygon_class;
    uint32_t point_count;
    uint16_t *points;
} TilePolygon;

// Stores all polylines assigned to a tile.
typedef struct TileOutput {
    TileCoord coord;
    TilePolyline *polylines;
    uint32_t polyline_count;
    uint32_t polyline_capacity;
    TilePolygon *polygons;
    uint32_t polygon_count;
    uint32_t polygon_capacity;
} TileOutput;

// Stores aggregated output for a region build.
typedef struct BuildContext {
    TileOutput *tiles;
    size_t tile_count;
    size_t tile_capacity;
    double min_lat;
    double min_lon;
    double max_lat;
    double max_lon;
    bool has_bounds;
} BuildContext;

// Stores CLI options for the region build.
typedef struct BuildOptions {
    const char *region;
    const char *osm_path;
    const char *dem_path;
    const char *out_dir;
    uint16_t min_z;
    uint16_t max_z;
} BuildOptions;

// Hashes an OSM node ID for the node map.
static uint64_t hash_id(int64_t id) {
    return (uint64_t)id * 11400714819323198485ull;
}

// Initializes a node map with the requested capacity.
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

// Releases memory owned by the node map.
static void node_map_free(NodeMap *map) {
    if (!map) {
        return;
    }

    free(map->entries);
    memset(map, 0, sizeof(*map));
}

// Rebuilds a node map at a larger capacity.
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

// Inserts or updates a node in the map.
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

// Retrieves a node from the map by ID.
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

// Initializes a way node list.
static void way_nodes_init(WayNodes *nodes) {
    if (!nodes) {
        return;
    }

    nodes->items = NULL;
    nodes->count = 0;
    nodes->capacity = 0;
}

// Clears a way node list and its storage.
static void way_nodes_clear(WayNodes *nodes) {
    if (!nodes) {
        return;
    }

    free(nodes->items);
    nodes->items = NULL;
    nodes->count = 0;
    nodes->capacity = 0;
}

// Appends a node ID to a way node list.
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

// Reads an XML attribute value into an output buffer.
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

// Maps OSM highway tag values to road classes.
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

// Maps OSM tags to polygon classes.
static bool polygon_class_from_tags(const char *building,
                                    const char *landuse,
                                    const char *natural,
                                    const char *leisure,
                                    const char *waterway,
                                    PolygonClass *out_class) {
    if (!out_class) {
        return false;
    }

    if (building && building[0] != '\0') {
        *out_class = POLYGON_CLASS_BUILDING;
        return true;
    }

    if ((natural && strcmp(natural, "water") == 0) ||
        (waterway && strcmp(waterway, "riverbank") == 0)) {
        *out_class = POLYGON_CLASS_WATER;
        return true;
    }

    if ((leisure && (strcmp(leisure, "park") == 0 || strcmp(leisure, "garden") == 0 ||
                     strcmp(leisure, "recreation_ground") == 0)) ||
        (landuse && (strcmp(landuse, "grass") == 0 || strcmp(landuse, "meadow") == 0 ||
                     strcmp(landuse, "recreation_ground") == 0 || strcmp(landuse, "village_green") == 0))) {
        *out_class = POLYGON_CLASS_PARK;
        return true;
    }

    if (landuse && landuse[0] != '\0') {
        *out_class = POLYGON_CLASS_LANDUSE;
        return true;
    }

    return false;
}

// Finds or creates a tile output container for a tile coordinate.
static TileOutput *build_context_get_tile(BuildContext *ctx, TileCoord coord) {
    if (!ctx) {
        return NULL;
    }

    for (size_t i = 0; i < ctx->tile_count; ++i) {
        TileOutput *tile = &ctx->tiles[i];
        if (tile->coord.z == coord.z && tile->coord.x == coord.x && tile->coord.y == coord.y) {
            return tile;
        }
    }

    if (ctx->tile_count == ctx->tile_capacity) {
        size_t next = ctx->tile_capacity == 0 ? 64 : ctx->tile_capacity * 2;
        TileOutput *next_tiles = (TileOutput *)realloc(ctx->tiles, next * sizeof(TileOutput));
        if (!next_tiles) {
            return NULL;
        }
        ctx->tiles = next_tiles;
        ctx->tile_capacity = next;
    }

    TileOutput *tile = &ctx->tiles[ctx->tile_count++];
    memset(tile, 0, sizeof(*tile));
    tile->coord = coord;
    return tile;
}

// Adds a polyline to the tile output container.
static bool tile_output_add_polyline(TileOutput *tile, RoadClass road_class, const uint16_t *points, uint32_t point_count) {
    if (!tile || !points || point_count < 2) {
        return false;
    }

    if (tile->polyline_count == tile->polyline_capacity) {
        uint32_t next = tile->polyline_capacity == 0 ? 16 : tile->polyline_capacity * 2;
        TilePolyline *next_polylines = (TilePolyline *)realloc(tile->polylines, next * sizeof(TilePolyline));
        if (!next_polylines) {
            return false;
        }
        tile->polylines = next_polylines;
        tile->polyline_capacity = next;
    }

    TilePolyline *polyline = &tile->polylines[tile->polyline_count++];
    polyline->road_class = road_class;
    polyline->point_count = point_count;
    polyline->points = (uint16_t *)malloc(point_count * 2 * sizeof(uint16_t));
    if (!polyline->points) {
        return false;
    }
    memcpy(polyline->points, points, point_count * 2 * sizeof(uint16_t));
    return true;
}

// Adds a polygon to the tile output container.
static bool tile_output_add_polygon(TileOutput *tile, PolygonClass polygon_class, const uint16_t *points, uint32_t point_count) {
    if (!tile || !points || point_count < 3) {
        return false;
    }

    if (tile->polygon_count == tile->polygon_capacity) {
        uint32_t next = tile->polygon_capacity == 0 ? 8 : tile->polygon_capacity * 2;
        TilePolygon *next_polygons = (TilePolygon *)realloc(tile->polygons, next * sizeof(TilePolygon));
        if (!next_polygons) {
            return false;
        }
        tile->polygons = next_polygons;
        tile->polygon_capacity = next;
    }

    TilePolygon *polygon = &tile->polygons[tile->polygon_count++];
    polygon->polygon_class = polygon_class;
    polygon->point_count = point_count;
    polygon->points = (uint16_t *)malloc(point_count * 2 * sizeof(uint16_t));
    if (!polygon->points) {
        return false;
    }
    memcpy(polygon->points, points, point_count * 2 * sizeof(uint16_t));
    return true;
}

// Quantizes a tile-local coordinate to the 0..4096 grid.
static uint16_t quantize_tile_coord(double value) {
    long rounded = lround(value * TILE_EXTENT);
    if (rounded < 0) {
        rounded = 0;
    } else if (rounded > (long)TILE_EXTENT) {
        rounded = (long)TILE_EXTENT;
    }
    return (uint16_t)rounded;
}

// Returns whether a point is inside a clipping edge.
static bool polygon_point_inside(MercatorMeters point, int edge, double min_x, double max_x, double min_y, double max_y) {
    switch (edge) {
        case 0:
            return point.x >= min_x;
        case 1:
            return point.x <= max_x;
        case 2:
            return point.y <= max_y;
        case 3:
            return point.y >= min_y;
        default:
            return true;
    }
}

// Returns the intersection of a segment with a clipping edge.
static MercatorMeters polygon_edge_intersection(MercatorMeters a,
                                                MercatorMeters b,
                                                int edge,
                                                double min_x,
                                                double max_x,
                                                double min_y,
                                                double max_y) {
    MercatorMeters out = a;
    double t = 0.0;
    double denom = 0.0;

    switch (edge) {
        case 0:
            denom = b.x - a.x;
            t = fabs(denom) > 1e-9 ? (min_x - a.x) / denom : 0.0;
            out.x = min_x;
            out.y = a.y + (b.y - a.y) * t;
            break;
        case 1:
            denom = b.x - a.x;
            t = fabs(denom) > 1e-9 ? (max_x - a.x) / denom : 0.0;
            out.x = max_x;
            out.y = a.y + (b.y - a.y) * t;
            break;
        case 2:
            denom = b.y - a.y;
            t = fabs(denom) > 1e-9 ? (max_y - a.y) / denom : 0.0;
            out.y = max_y;
            out.x = a.x + (b.x - a.x) * t;
            break;
        case 3:
            denom = b.y - a.y;
            t = fabs(denom) > 1e-9 ? (min_y - a.y) / denom : 0.0;
            out.y = min_y;
            out.x = a.x + (b.x - a.x) * t;
            break;
        default:
            break;
    }

    return out;
}

// Clips a polygon to a rectangle and returns the clipped points.
static bool clip_polygon_to_rect(const MercatorMeters *points,
                                 size_t count,
                                 double min_x,
                                 double max_x,
                                 double min_y,
                                 double max_y,
                                 MercatorMeters **out_points,
                                 size_t *out_count) {
    if (!points || count < 3 || !out_points || !out_count) {
        return false;
    }

    size_t capacity = count * 2 + 4;
    MercatorMeters *scratch_a = (MercatorMeters *)malloc(capacity * sizeof(MercatorMeters));
    MercatorMeters *scratch_b = (MercatorMeters *)malloc(capacity * sizeof(MercatorMeters));
    if (!scratch_a || !scratch_b) {
        free(scratch_a);
        free(scratch_b);
        return false;
    }

    memcpy(scratch_a, points, count * sizeof(MercatorMeters));
    size_t input_count = count;

    for (int edge = 0; edge < 4; ++edge) {
        if (input_count < 3) {
            break;
        }

        size_t output_count = 0;
        MercatorMeters prev = scratch_a[input_count - 1];
        bool prev_inside = polygon_point_inside(prev, edge, min_x, max_x, min_y, max_y);

        for (size_t i = 0; i < input_count; ++i) {
            MercatorMeters curr = scratch_a[i];
            bool curr_inside = polygon_point_inside(curr, edge, min_x, max_x, min_y, max_y);

            if (curr_inside) {
                if (!prev_inside) {
                    scratch_b[output_count++] = polygon_edge_intersection(prev, curr, edge, min_x, max_x, min_y, max_y);
                }
                scratch_b[output_count++] = curr;
            } else if (prev_inside) {
                scratch_b[output_count++] = polygon_edge_intersection(prev, curr, edge, min_x, max_x, min_y, max_y);
            }

            prev = curr;
            prev_inside = curr_inside;
        }

        MercatorMeters *swap = scratch_a;
        scratch_a = scratch_b;
        scratch_b = swap;
        input_count = output_count;
    }

    if (input_count < 3) {
        free(scratch_a);
        free(scratch_b);
        return false;
    }

    *out_points = (MercatorMeters *)malloc(input_count * sizeof(MercatorMeters));
    if (!*out_points) {
        free(scratch_a);
        free(scratch_b);
        return false;
    }

    memcpy(*out_points, scratch_a, input_count * sizeof(MercatorMeters));
    *out_count = input_count;

    free(scratch_a);
    free(scratch_b);
    return true;
}

// Adds a way polyline to tile outputs for a single zoom.
static bool add_way_to_tiles(BuildContext *ctx, const MercatorMeters *points, size_t count, RoadClass road_class, uint16_t z) {
    if (!ctx || !points || count < 2) {
        return false;
    }

    TileCoord current_tile = tile_from_meters(z, points[0]);
    uint16_t *temp_points = NULL;
    uint32_t temp_count = 0;
    uint32_t temp_capacity = 0;

    double tile_size = tile_size_meters(z);

    for (size_t i = 0; i < count; ++i) {
        TileCoord tile = tile_from_meters(z, points[i]);
        if (i > 0 && (tile.x != current_tile.x || tile.y != current_tile.y || tile.z != current_tile.z)) {
            if (temp_count >= 2) {
                TileOutput *output = build_context_get_tile(ctx, current_tile);
                if (!output || !tile_output_add_polyline(output, road_class, temp_points, temp_count)) {
                    free(temp_points);
                    return false;
                }
            }

            temp_count = 0;
            current_tile = tile;

            MercatorMeters prev_point = points[i - 1];
            MercatorMeters curr_point = points[i];

            double origin_x = tile_origin_meters(current_tile).x;
            double origin_y = tile_origin_meters(current_tile).y;
            double u = (prev_point.x - origin_x) / tile_size;
            double v = (origin_y - prev_point.y) / tile_size;

            if (temp_count + 2 > temp_capacity) {
                uint32_t next = temp_capacity == 0 ? 64 : temp_capacity * 2;
                uint16_t *next_points = (uint16_t *)realloc(temp_points, next * 2 * sizeof(uint16_t));
                if (!next_points) {
                    free(temp_points);
                    return false;
                }
                temp_points = next_points;
                temp_capacity = next;
            }

            temp_points[temp_count * 2] = quantize_tile_coord(u);
            temp_points[temp_count * 2 + 1] = quantize_tile_coord(v);
            temp_count += 1;

            u = (curr_point.x - origin_x) / tile_size;
            v = (origin_y - curr_point.y) / tile_size;
            temp_points[temp_count * 2] = quantize_tile_coord(u);
            temp_points[temp_count * 2 + 1] = quantize_tile_coord(v);
            temp_count += 1;
            continue;
        }

        double origin_x = tile_origin_meters(current_tile).x;
        double origin_y = tile_origin_meters(current_tile).y;
        double u = (points[i].x - origin_x) / tile_size;
        double v = (origin_y - points[i].y) / tile_size;

        if (temp_count + 1 > temp_capacity) {
            uint32_t next = temp_capacity == 0 ? 64 : temp_capacity * 2;
            uint16_t *next_points = (uint16_t *)realloc(temp_points, next * 2 * sizeof(uint16_t));
            if (!next_points) {
                free(temp_points);
                return false;
            }
            temp_points = next_points;
            temp_capacity = next;
        }

        temp_points[temp_count * 2] = quantize_tile_coord(u);
        temp_points[temp_count * 2 + 1] = quantize_tile_coord(v);
        temp_count += 1;
    }

    if (temp_count >= 2) {
        TileOutput *output = build_context_get_tile(ctx, current_tile);
        if (!output || !tile_output_add_polyline(output, road_class, temp_points, temp_count)) {
            free(temp_points);
            return false;
        }
    }

    free(temp_points);
    return true;
}

// Adds a polygon to tile outputs for a single zoom.
static bool add_polygon_to_tiles(BuildContext *ctx, const MercatorMeters *points, size_t count, PolygonClass polygon_class, uint16_t z) {
    if (!ctx || !points || count < 3) {
        return false;
    }

    double min_x = points[0].x;
    double max_x = points[0].x;
    double min_y = points[0].y;
    double max_y = points[0].y;
    for (size_t i = 1; i < count; ++i) {
        if (points[i].x < min_x) {
            min_x = points[i].x;
        }
        if (points[i].x > max_x) {
            max_x = points[i].x;
        }
        if (points[i].y < min_y) {
            min_y = points[i].y;
        }
        if (points[i].y > max_y) {
            max_y = points[i].y;
        }
    }

    TileCoord top_left = tile_from_meters(z, (MercatorMeters){min_x, max_y});
    TileCoord bottom_right = tile_from_meters(z, (MercatorMeters){max_x, min_y});

    double tile_size = tile_size_meters(z);

    for (uint32_t ty = top_left.y; ty <= bottom_right.y; ++ty) {
        for (uint32_t tx = top_left.x; tx <= bottom_right.x; ++tx) {
            TileCoord coord = {z, tx, ty};
            MercatorMeters origin = tile_origin_meters(coord);
            double tile_min_x = origin.x;
            double tile_max_x = origin.x + tile_size;
            double tile_max_y = origin.y;
            double tile_min_y = origin.y - tile_size;

            MercatorMeters *clipped = NULL;
            size_t clipped_count = 0;
            if (!clip_polygon_to_rect(points, count, tile_min_x, tile_max_x, tile_min_y, tile_max_y, &clipped, &clipped_count)) {
                continue;
            }

            uint16_t *quantized = (uint16_t *)malloc(clipped_count * 2 * sizeof(uint16_t));
            if (!quantized) {
                free(clipped);
                continue;
            }

            for (size_t i = 0; i < clipped_count; ++i) {
                double u = (clipped[i].x - tile_min_x) / tile_size;
                double v = (tile_max_y - clipped[i].y) / tile_size;
                quantized[i * 2] = quantize_tile_coord(u);
                quantized[i * 2 + 1] = quantize_tile_coord(v);
            }

            TileOutput *output = build_context_get_tile(ctx, coord);
            if (output) {
                tile_output_add_polygon(output, polygon_class, quantized, (uint32_t)clipped_count);
            }

            free(quantized);
            free(clipped);
        }
    }

    return true;
}

// Ensures tiles exist for the full region bounds at each zoom.
static void ensure_tiles_for_bounds(BuildContext *ctx, const BuildOptions *options) {
    if (!ctx || !options || !ctx->has_bounds) {
        return;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){ctx->min_lat, ctx->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){ctx->max_lat, ctx->max_lon});

    for (uint16_t z = options->min_z; z <= options->max_z; ++z) {
        TileCoord top_left = tile_from_meters(z, (MercatorMeters){min_m.x, max_m.y});
        TileCoord bottom_right = tile_from_meters(z, (MercatorMeters){max_m.x, min_m.y});

        for (uint32_t y = top_left.y; y <= bottom_right.y; ++y) {
            for (uint32_t x = top_left.x; x <= bottom_right.x; ++x) {
                TileCoord coord = {z, x, y};
                build_context_get_tile(ctx, coord);
            }
        }
    }
}

// Sorts tile outputs by z/x/y for stable output ordering.
static int tile_coord_compare(const void *a, const void *b) {
    const TileOutput *left = (const TileOutput *)a;
    const TileOutput *right = (const TileOutput *)b;

    if (left->coord.z != right->coord.z) {
        return (left->coord.z < right->coord.z) ? -1 : 1;
    }
    if (left->coord.x != right->coord.x) {
        return (left->coord.x < right->coord.x) ? -1 : 1;
    }
    if (left->coord.y != right->coord.y) {
        return (left->coord.y < right->coord.y) ? -1 : 1;
    }
    return 0;
}

// Ensures a directory exists on disk.
static bool ensure_dir(const char *path) {
    if (!path) {
        return false;
    }

    if (mkdir(path, 0755) == 0) {
        return true;
    }

    return errno == EEXIST;
}

// Builds the output path for a tile and creates parent directories.
static bool ensure_tile_path(const char *base_dir, TileCoord coord, const char *suffix, char *out_path, size_t out_size) {
    if (!base_dir || !out_path || out_size == 0) {
        return false;
    }
    if (!suffix) {
        suffix = "mft";
    }

    char tiles_dir[512];
    snprintf(tiles_dir, sizeof(tiles_dir), "%s/tiles", base_dir);
    if (!ensure_dir(base_dir) || !ensure_dir(tiles_dir)) {
        return false;
    }

    char z_dir[512];
    snprintf(z_dir, sizeof(z_dir), "%s/%u", tiles_dir, coord.z);
    if (!ensure_dir(z_dir)) {
        return false;
    }

    char x_dir[512];
    snprintf(x_dir, sizeof(x_dir), "%s/%u", z_dir, coord.x);
    if (!ensure_dir(x_dir)) {
        return false;
    }

    snprintf(out_path, out_size, "%s/%u.%s", x_dir, coord.y, suffix);
    return true;
}

static bool road_class_is_artery(RoadClass road_class) {
    return road_class == ROAD_CLASS_MOTORWAY ||
        road_class == ROAD_CLASS_TRUNK ||
        road_class == ROAD_CLASS_PRIMARY ||
        road_class == ROAD_CLASS_SECONDARY;
}

static bool write_tile_file_roads(const char *base_dir, const TileOutput *tile, const char *suffix, bool want_artery) {
    if (!base_dir || !tile || !suffix) {
        return false;
    }

    char path[512];
    if (!ensure_tile_path(base_dir, tile->coord, suffix, path, sizeof(path))) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    uint32_t polyline_count = 0;
    for (uint32_t i = 0; i < tile->polyline_count; ++i) {
        bool is_artery = road_class_is_artery(tile->polylines[i].road_class);
        if (is_artery == want_artery) {
            polyline_count += 1;
        }
    }

    const char magic[4] = {'M', 'F', 'T', '1'};
    uint16_t version = 2;
    uint16_t z = tile->coord.z;
    uint32_t x = tile->coord.x;
    uint32_t y = tile->coord.y;
    uint32_t polygon_count = 0;

    fwrite(magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(uint16_t), 1, file);
    fwrite(&z, sizeof(uint16_t), 1, file);
    fwrite(&x, sizeof(uint32_t), 1, file);
    fwrite(&y, sizeof(uint32_t), 1, file);
    fwrite(&polyline_count, sizeof(uint32_t), 1, file);

    for (uint32_t i = 0; i < tile->polyline_count; ++i) {
        const TilePolyline *polyline = &tile->polylines[i];
        bool is_artery = road_class_is_artery(polyline->road_class);
        if (is_artery != want_artery) {
            continue;
        }
        uint8_t road_class = (uint8_t)polyline->road_class;
        fwrite(&road_class, sizeof(uint8_t), 1, file);
        fwrite(&polyline->point_count, sizeof(uint32_t), 1, file);
    }

    for (uint32_t i = 0; i < tile->polyline_count; ++i) {
        const TilePolyline *polyline = &tile->polylines[i];
        bool is_artery = road_class_is_artery(polyline->road_class);
        if (is_artery != want_artery) {
            continue;
        }
        fwrite(polyline->points, sizeof(uint16_t), polyline->point_count * 2, file);
    }

    fwrite(&polygon_count, sizeof(uint32_t), 1, file);
    fclose(file);
    return true;
}

// Writes an empty split MFT tile payload for a layer placeholder.
static bool write_empty_tile_file(const char *base_dir, TileCoord coord, const char *suffix) {
    if (!base_dir || !suffix) {
        return false;
    }

    char path[512];
    if (!ensure_tile_path(base_dir, coord, suffix, path, sizeof(path))) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    const char magic[4] = {'M', 'F', 'T', '1'};
    uint16_t version = 2;
    uint16_t z = coord.z;
    uint32_t x = coord.x;
    uint32_t y = coord.y;
    uint32_t polyline_count = 0;
    uint32_t polygon_count = 0;

    fwrite(magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(uint16_t), 1, file);
    fwrite(&z, sizeof(uint16_t), 1, file);
    fwrite(&x, sizeof(uint32_t), 1, file);
    fwrite(&y, sizeof(uint32_t), 1, file);
    fwrite(&polyline_count, sizeof(uint32_t), 1, file);
    fwrite(&polygon_count, sizeof(uint32_t), 1, file);

    fclose(file);
    return true;
}

static bool write_tile_file_polygons(const char *base_dir, const TileOutput *tile, const char *suffix, PolygonClass polygon_class) {
    if (!base_dir || !tile || !suffix) {
        return false;
    }

    char path[512];
    if (!ensure_tile_path(base_dir, tile->coord, suffix, path, sizeof(path))) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    const char magic[4] = {'M', 'F', 'T', '1'};
    uint16_t version = 2;
    uint16_t z = tile->coord.z;
    uint32_t x = tile->coord.x;
    uint32_t y = tile->coord.y;
    uint32_t polyline_count = 0;
    uint32_t polygon_count = 0;
    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        if (tile->polygons[i].polygon_class == polygon_class) {
            polygon_count += 1;
        }
    }

    fwrite(magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(uint16_t), 1, file);
    fwrite(&z, sizeof(uint16_t), 1, file);
    fwrite(&x, sizeof(uint32_t), 1, file);
    fwrite(&y, sizeof(uint32_t), 1, file);
    fwrite(&polyline_count, sizeof(uint32_t), 1, file);

    fwrite(&polygon_count, sizeof(uint32_t), 1, file);
    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const TilePolygon *polygon = &tile->polygons[i];
        if (polygon->polygon_class != polygon_class) {
            continue;
        }
        uint8_t polygon_class = (uint8_t)polygon->polygon_class;
        uint16_t ring_count = 1;
        fwrite(&polygon_class, sizeof(uint8_t), 1, file);
        fwrite(&ring_count, sizeof(uint16_t), 1, file);
    }

    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const TilePolygon *polygon = &tile->polygons[i];
        if (polygon->polygon_class != polygon_class) {
            continue;
        }
        fwrite(&polygon->point_count, sizeof(uint32_t), 1, file);
    }

    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const TilePolygon *polygon = &tile->polygons[i];
        if (polygon->polygon_class != polygon_class) {
            continue;
        }
        fwrite(polygon->points, sizeof(uint16_t), polygon->point_count * 2, file);
    }

    fclose(file);
    return true;
}

// Writes split MFT tile files to disk.
static bool write_tile_file(const char *base_dir, const TileOutput *tile) {
    if (!base_dir || !tile) {
        return false;
    }

    if (!write_tile_file_roads(base_dir, tile, "artery.mft", true)) {
        return false;
    }
    if (!write_tile_file_roads(base_dir, tile, "local.mft", false)) {
        return false;
    }
    if (!write_tile_file_polygons(base_dir, tile, "water.mft", POLYGON_CLASS_WATER)) {
        return false;
    }
    if (!write_tile_file_polygons(base_dir, tile, "park.mft", POLYGON_CLASS_PARK)) {
        return false;
    }
    if (!write_tile_file_polygons(base_dir, tile, "landuse.mft", POLYGON_CLASS_LANDUSE)) {
        return false;
    }
    if (!write_tile_file_polygons(base_dir, tile, "building.mft", POLYGON_CLASS_BUILDING)) {
        return false;
    }
    if (!write_empty_tile_file(base_dir, tile->coord, "contour.mft")) {
        return false;
    }
    return true;
}

// Writes meta.json for the region pack.
static bool write_meta_json(const BuildOptions *options, const BuildContext *ctx) {
    if (!options || !ctx || !options->out_dir) {
        return false;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/meta.json", options->out_dir);

    FILE *file = fopen(path, "w");
    if (!file) {
        return false;
    }

    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);
    char timestamp[64] = "";
    if (utc) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc);
    }

    fprintf(file, "{\n");
    fprintf(file, "    \"region\": \"%s\",\n", options->region);
    fprintf(file, "    \"city_source\": \"%s\",\n", options->osm_path);
    if (options->dem_path && options->dem_path[0] != '\0') {
        fprintf(file, "    \"terrain_source\": \"%s\",\n", options->dem_path);
    } else {
        fprintf(file, "    \"terrain_source\": null,\n");
    }
    fprintf(file, "    \"created_utc\": \"%s\",\n", timestamp);
    fprintf(file, "    \"bounds\": {\n");
    fprintf(file, "        \"min_lat\": %.8f,\n", ctx->min_lat);
    fprintf(file, "        \"min_lon\": %.8f,\n", ctx->min_lon);
    fprintf(file, "        \"max_lat\": %.8f,\n", ctx->max_lat);
    fprintf(file, "        \"max_lon\": %.8f\n", ctx->max_lon);
    fprintf(file, "    },\n");
    fprintf(file, "    \"tile\": {\n");
    fprintf(file, "        \"min_z\": %u,\n", options->min_z);
    fprintf(file, "        \"max_z\": %u,\n", options->max_z);
    fprintf(file, "        \"extent\": %u\n", (unsigned)TILE_EXTENT);
    fprintf(file, "    },\n");
    fprintf(file, "    \"contours\": {\n");
    fprintf(file, "        \"enabled\": %s,\n", (options->dem_path && options->dem_path[0] != '\0') ? "true" : "false");
    fprintf(file, "        \"phase\": \"A_scaffold\",\n");
    fprintf(file, "        \"interval_m\": 10,\n");
    fprintf(file, "        \"major_every\": 5\n");
    fprintf(file, "    }\n");
    fprintf(file, "}\n");

    fclose(file);
    return true;
}

// Releases tile outputs and their polylines.
static void build_context_free(BuildContext *ctx) {
    if (!ctx) {
        return;
    }

    for (size_t i = 0; i < ctx->tile_count; ++i) {
        TileOutput *tile = &ctx->tiles[i];
        for (uint32_t p = 0; p < tile->polyline_count; ++p) {
            free(tile->polylines[p].points);
        }
        free(tile->polylines);
        for (uint32_t p = 0; p < tile->polygon_count; ++p) {
            free(tile->polygons[p].points);
        }
        free(tile->polygons);
    }

    free(ctx->tiles);
    memset(ctx, 0, sizeof(*ctx));
}

// Updates bounds with a new lat/lon sample.
static void build_context_update_bounds(BuildContext *ctx, double lat, double lon) {
    if (!ctx->has_bounds) {
        ctx->min_lat = ctx->max_lat = lat;
        ctx->min_lon = ctx->max_lon = lon;
        ctx->has_bounds = true;
        return;
    }

    if (lat < ctx->min_lat) {
        ctx->min_lat = lat;
    }
    if (lat > ctx->max_lat) {
        ctx->max_lat = lat;
    }
    if (lon < ctx->min_lon) {
        ctx->min_lon = lon;
    }
    if (lon > ctx->max_lon) {
        ctx->max_lon = lon;
    }
}

// Parses the <bounds> tag for region bounds.
static bool parse_bounds(BuildContext *ctx, const char *line) {
    if (!ctx || !line) {
        return false;
    }

    char minlat[32];
    char minlon[32];
    char maxlat[32];
    char maxlon[32];

    if (!xml_attr(line, "minlat", minlat, sizeof(minlat)) ||
        !xml_attr(line, "minlon", minlon, sizeof(minlon)) ||
        !xml_attr(line, "maxlat", maxlat, sizeof(maxlat)) ||
        !xml_attr(line, "maxlon", maxlon, sizeof(maxlon))) {
        return false;
    }

    ctx->min_lat = strtod(minlat, NULL);
    ctx->min_lon = strtod(minlon, NULL);
    ctx->max_lat = strtod(maxlat, NULL);
    ctx->max_lon = strtod(maxlon, NULL);
    ctx->has_bounds = true;
    return true;
}

// Streams an OSM XML file and emits road tiles.
static bool parse_osm(const BuildOptions *options, BuildContext *ctx) {
    if (!options || !ctx) {
        return false;
    }

    FILE *file = fopen(options->osm_path, "r");
    if (!file) {
        log_error("Failed to open OSM file: %s", options->osm_path);
        return false;
    }

    NodeMap nodes;
    if (!node_map_init(&nodes, 1u << 20)) {
        fclose(file);
        return false;
    }

    char line[8192];
    WayNodes way_nodes;
    way_nodes_init(&way_nodes);
    bool in_way = false;
    char highway_tag[64] = {0};
    char building_tag[64] = {0};
    char landuse_tag[64] = {0};
    char natural_tag[64] = {0};
    char leisure_tag[64] = {0};
    char waterway_tag[64] = {0};

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "<bounds") != NULL) {
            parse_bounds(ctx, line);
        }

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
            node_map_put(&nodes, id, lat, lon);
            continue;
        }

        if (strstr(line, "<way ") != NULL) {
            in_way = true;
            highway_tag[0] = '\0';
            building_tag[0] = '\0';
            landuse_tag[0] = '\0';
            natural_tag[0] = '\0';
            leisure_tag[0] = '\0';
            waterway_tag[0] = '\0';
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
                    } else if (strcmp(key_buf, "building") == 0) {
                        snprintf(building_tag, sizeof(building_tag), "%s", val_buf);
                    } else if (strcmp(key_buf, "landuse") == 0) {
                        snprintf(landuse_tag, sizeof(landuse_tag), "%s", val_buf);
                    } else if (strcmp(key_buf, "natural") == 0) {
                        snprintf(natural_tag, sizeof(natural_tag), "%s", val_buf);
                    } else if (strcmp(key_buf, "leisure") == 0) {
                        snprintf(leisure_tag, sizeof(leisure_tag), "%s", val_buf);
                    } else if (strcmp(key_buf, "waterway") == 0) {
                        snprintf(waterway_tag, sizeof(waterway_tag), "%s", val_buf);
                    }
                }
                continue;
            }

            if (strstr(line, "</way>") != NULL) {
                if (highway_tag[0] != '\0' && way_nodes.count >= 2) {
                    RoadClass road_class = road_class_from_highway(highway_tag);
                    MercatorMeters *points = (MercatorMeters *)malloc(way_nodes.count * sizeof(MercatorMeters));
                    if (points) {
                        size_t count = 0;
                        for (size_t i = 0; i < way_nodes.count; ++i) {
                            double lat = 0.0;
                            double lon = 0.0;
                            if (!node_map_get(&nodes, way_nodes.items[i], &lat, &lon)) {
                                continue;
                            }
                            build_context_update_bounds(ctx, lat, lon);
                            points[count++] = mercator_from_latlon((LatLon){lat, lon});
                        }

                        if (count >= 2) {
                            for (uint16_t z = options->min_z; z <= options->max_z; ++z) {
                                add_way_to_tiles(ctx, points, count, road_class, z);
                            }
                        }

                        free(points);
                    }
                }

                PolygonClass polygon_class;
                if (polygon_class_from_tags(building_tag, landuse_tag, natural_tag, leisure_tag, waterway_tag, &polygon_class) &&
                    way_nodes.count >= 4 && way_nodes.items[0] == way_nodes.items[way_nodes.count - 1]) {
                    MercatorMeters *points = (MercatorMeters *)malloc(way_nodes.count * sizeof(MercatorMeters));
                    if (points) {
                        size_t count = 0;
                        for (size_t i = 0; i < way_nodes.count; ++i) {
                            double lat = 0.0;
                            double lon = 0.0;
                            if (!node_map_get(&nodes, way_nodes.items[i], &lat, &lon)) {
                                continue;
                            }
                            build_context_update_bounds(ctx, lat, lon);
                            points[count++] = mercator_from_latlon((LatLon){lat, lon});
                        }

                        if (count >= 4) {
                            if (points[0].x == points[count - 1].x && points[0].y == points[count - 1].y) {
                                count -= 1;
                            }
                            for (uint16_t z = options->min_z; z <= options->max_z; ++z) {
                                add_polygon_to_tiles(ctx, points, count, polygon_class, z);
                            }
                        }

                        free(points);
                    }
                }

                in_way = false;
                continue;
            }
        }
    }

    way_nodes_clear(&way_nodes);
    node_map_free(&nodes);
    fclose(file);
    return true;
}

// Prints CLI usage to stdout.
static void usage(void) {
    printf("mapforge_region --region <name> --osm <file.osm> [--dem <file.dem>] --out <dir> [--min-z N] [--max-z N]\n");
}

// Parses CLI arguments into build options.
static bool parse_args(int argc, char **argv, BuildOptions *options) {
    if (!options) {
        return false;
    }

    memset(options, 0, sizeof(*options));
    options->min_z = 12;
    options->max_z = 12;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--region") == 0 && i + 1 < argc) {
            options->region = argv[++i];
        } else if (strcmp(argv[i], "--osm") == 0 && i + 1 < argc) {
            options->osm_path = argv[++i];
        } else if (strcmp(argv[i], "--dem") == 0 && i + 1 < argc) {
            options->dem_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            options->out_dir = argv[++i];
        } else if (strcmp(argv[i], "--min-z") == 0 && i + 1 < argc) {
            options->min_z = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--max-z") == 0 && i + 1 < argc) {
            options->max_z = (uint16_t)atoi(argv[++i]);
        } else {
            return false;
        }
    }

    if (!options->region || !options->osm_path || !options->out_dir) {
        return false;
    }

    return true;
}

// Entry point for the region pack tool.
int main(int argc, char **argv) {
    BuildOptions options;
    if (!parse_args(argc, argv, &options)) {
        usage();
        return 1;
    }

    BuildContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    if (!parse_osm(&options, &ctx)) {
        build_context_free(&ctx);
        return 1;
    }

    ensure_tiles_for_bounds(&ctx, &options);

    if (ctx.tile_count == 0) {
        log_info("No tiles produced.");
        build_context_free(&ctx);
        return 0;
    }

    qsort(ctx.tiles, ctx.tile_count, sizeof(TileOutput), tile_coord_compare);

    for (size_t i = 0; i < ctx.tile_count; ++i) {
        if (!write_tile_file(options.out_dir, &ctx.tiles[i])) {
            log_error("Failed to write tile %u/%u/%u", ctx.tiles[i].coord.z, ctx.tiles[i].coord.x, ctx.tiles[i].coord.y);
        }
    }

    write_meta_json(&options, &ctx);
    build_context_free(&ctx);

    log_info("Region pack generated at %s", options.out_dir);
    return 0;
}
