#include "mapforge_region_internal.h"

static const char *osm_source_kind_label(OSMSourceKind kind) {
    switch (kind) {
        case OSM_SOURCE_KIND_PBF:
            return "pbf";
        case OSM_SOURCE_KIND_XML:
            return "xml";
        case OSM_SOURCE_KIND_UNKNOWN:
        default:
            return "unknown";
    }
}

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

static bool source_name_has_suffix(const char *name, const char *suffix) {
    size_t name_len = 0u;
    size_t suffix_len = 0u;
    if (!name || !suffix) {
        return false;
    }
    name_len = strlen(name);
    suffix_len = strlen(suffix);
    if (name_len < suffix_len) {
        return false;
    }
    return strcasecmp(name + (name_len - suffix_len), suffix) == 0;
}

static bool source_buffer_contains(const uint8_t *buffer,
                                   size_t buffer_size,
                                   const char *needle) {
    size_t needle_len = 0u;
    if (!buffer || !needle) {
        return false;
    }
    needle_len = strlen(needle);
    if (needle_len == 0u || needle_len > buffer_size) {
        return false;
    }
    for (size_t i = 0; i + needle_len <= buffer_size; ++i) {
        if (memcmp(buffer + i, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

typedef struct SourceFingerprint {
    bool ok;
    bool has_mtime;
    uint64_t size_bytes;
    uint64_t hash_fnv1a64;
    uint64_t mtime_unix;
} SourceFingerprint;

static bool source_compute_fingerprint(const char *path, SourceFingerprint *out_fp) {
    FILE *file = NULL;
    struct stat st;
    uint8_t buffer[65536];
    uint64_t hash = 1469598103934665603ULL;
    size_t read_size = 0u;

    if (!path || !out_fp) {
        return false;
    }
    memset(out_fp, 0, sizeof(*out_fp));

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }
    out_fp->size_bytes = (uint64_t)st.st_size;
    out_fp->mtime_unix = (uint64_t)st.st_mtime;
    out_fp->has_mtime = true;

    file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    for (;;) {
        read_size = fread(buffer, 1u, sizeof(buffer), file);
        if (read_size > 0u) {
            for (size_t i = 0u; i < read_size; ++i) {
                hash ^= (uint64_t)buffer[i];
                hash *= 1099511628211ULL;
            }
        }
        if (read_size < sizeof(buffer)) {
            if (ferror(file)) {
                fclose(file);
                return false;
            }
            break;
        }
    }
    fclose(file);
    out_fp->hash_fnv1a64 = hash;
    out_fp->ok = true;
    return true;
}

static OSMSourceKind detect_osm_source_kind(const char *path) {
    uint8_t probe[4096];
    size_t read_size = 0u;
    FILE *file = NULL;

    if (!path || path[0] == '\0') {
        return OSM_SOURCE_KIND_UNKNOWN;
    }

    if (source_name_has_suffix(path, ".osm") || source_name_has_suffix(path, ".osm.xml")) {
        return OSM_SOURCE_KIND_XML;
    }
    if (source_name_has_suffix(path, ".pbf") || source_name_has_suffix(path, ".osm.pbf")) {
        return OSM_SOURCE_KIND_PBF;
    }

    file = fopen(path, "rb");
    if (!file) {
        return OSM_SOURCE_KIND_UNKNOWN;
    }
    read_size = fread(probe, 1u, sizeof(probe), file);
    fclose(file);

    if (read_size == 0u) {
        return OSM_SOURCE_KIND_UNKNOWN;
    }
    if (source_buffer_contains(probe, read_size, "OSMHeader")) {
        return OSM_SOURCE_KIND_PBF;
    }
    if (source_buffer_contains(probe, read_size, "<osm") ||
        source_buffer_contains(probe, read_size, "<?xml")) {
        return OSM_SOURCE_KIND_XML;
    }
    return OSM_SOURCE_KIND_UNKNOWN;
}

static int source_spawn_and_wait(const char *program, char *const argv[]) {
    pid_t pid = 0;
    int rc = 0;
    int status = 0;

    if (!program || !argv || !argv[0]) {
        return EINVAL;
    }

    if (strchr(program, '/') != NULL) {
        rc = posix_spawn(&pid, program, NULL, NULL, argv, environ);
    } else {
        rc = posix_spawnp(&pid, program, NULL, NULL, argv, environ);
    }
    if (rc != 0) {
        return rc;
    }

    for (;;) {
        pid_t waited = waitpid(pid, &status, 0);
        if (waited == pid) {
            break;
        }
        if (waited < 0 && errno == EINTR) {
            continue;
        }
        return errno != 0 ? errno : ECHILD;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return EPROTO;
    }
    return 0;
}

static bool resolve_osmium_program(char *out_path, size_t out_path_cap) {
    const char *env_path = getenv("MAPFORGE_OSMIUM_PATH");
    const char *candidates[] = {
        "/opt/homebrew/bin/osmium",
        "/usr/local/bin/osmium",
        "/usr/bin/osmium",
        "osmium"
    };
    if (!out_path || out_path_cap == 0u) {
        return false;
    }
    out_path[0] = '\0';

    if (env_path && env_path[0] != '\0') {
        if (access(env_path, X_OK) == 0) {
            snprintf(out_path, out_path_cap, "%s", env_path);
            return true;
        }
        log_error("MAPFORGE_OSMIUM_PATH is set but not executable: %s", env_path);
    }

    for (size_t i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
        const char *candidate = candidates[i];
        if (candidate[0] == '/') {
            if (access(candidate, X_OK) != 0) {
                continue;
            }
        }
        snprintf(out_path, out_path_cap, "%s", candidate);
        return true;
    }
    return false;
}

static bool convert_pbf_to_xml(const char *pbf_path,
                               char *xml_path,
                               size_t xml_path_cap,
                               char *out_converter_path,
                               size_t out_converter_path_cap) {
    char tmp_template[] = "/tmp/mapforge_osmxml_XXXXXX";
    char osmium_program[MAPFORGE_SOURCE_PATH_CAPACITY];
    int fd = -1;
    int rc = 0;
    char *osmium_argv[] = {NULL, "cat", "-F", "pbf", (char *)pbf_path, "-o", tmp_template, "-f", "osm", "--overwrite", NULL};

    if (!pbf_path || !xml_path || xml_path_cap == 0u) {
        return false;
    }
    xml_path[0] = '\0';
    if (out_converter_path && out_converter_path_cap > 0u) {
        out_converter_path[0] = '\0';
    }

    if (!resolve_osmium_program(osmium_program, sizeof(osmium_program))) {
        log_error("PBF source detected (%s) but converter is unavailable. Install `osmium` or set MAPFORGE_OSMIUM_PATH to the osmium binary.", pbf_path);
        return false;
    }
    osmium_argv[0] = osmium_program;
    if (out_converter_path && out_converter_path_cap > 0u) {
        snprintf(out_converter_path, out_converter_path_cap, "%s", osmium_program);
    }

    fd = mkstemp(tmp_template);
    if (fd < 0) {
        log_error("Failed to create temporary XML path for PBF conversion: %s", strerror(errno));
        return false;
    }
    close(fd);

    rc = source_spawn_and_wait(osmium_program, osmium_argv);
    if (rc == 0) {
        snprintf(xml_path, xml_path_cap, "%s", tmp_template);
        return true;
    }

    (void)unlink(tmp_template);
    if (rc == ENOENT) {
        log_error("PBF source detected (%s) but converter is unavailable. Install `osmium` or set MAPFORGE_OSMIUM_PATH to the osmium binary.", pbf_path);
    } else {
        log_error("PBF conversion failed for %s (converter=%s, rc=%d)", pbf_path, osmium_program, rc);
    }
    return false;
}

static bool parse_osm_xml(const BuildOptions *options,
                          BuildContext *ctx,
                          const char *osm_xml_path) {
    if (!options || !ctx) {
        return false;
    }

    FILE *file = fopen(osm_xml_path, "r");
    if (!file) {
        log_error("Failed to open OSM file: %s", osm_xml_path);
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
                                mapforge_region_add_way_to_tiles(ctx, points, count, road_class, z);
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
                                mapforge_region_add_polygon_to_tiles(ctx, points, count, polygon_class, z);
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

bool mapforge_region_parse_osm(const BuildOptions *options, BuildContext *ctx) {
    OSMSourceKind source_kind;
    bool ok = false;
    SourceFingerprint fingerprint;
    char converted_xml_path[MAPFORGE_SOURCE_PATH_CAPACITY];
    char converter_path[MAPFORGE_SOURCE_PATH_CAPACITY];
    const char *parse_path = NULL;

    if (!options || !ctx || !options->osm_path) {
        return false;
    }

    memset(&fingerprint, 0, sizeof(fingerprint));
    converted_xml_path[0] = '\0';
    converter_path[0] = '\0';
    ctx->source_kind_detected = OSM_SOURCE_KIND_UNKNOWN;
    ctx->source_is_canonical_pbf = false;
    ctx->source_compat_xml_mode = false;
    ctx->source_pbf_conversion_used = false;
    ctx->source_fingerprint_ok = false;
    ctx->source_mtime_ok = false;
    ctx->source_size_bytes = 0u;
    ctx->source_hash_fnv1a64 = 0u;
    ctx->source_mtime_unix = 0u;
    ctx->source_converter_program[0] = '\0';

    if (source_compute_fingerprint(options->osm_path, &fingerprint)) {
        ctx->source_fingerprint_ok = fingerprint.ok;
        ctx->source_mtime_ok = fingerprint.has_mtime;
        ctx->source_size_bytes = fingerprint.size_bytes;
        ctx->source_hash_fnv1a64 = fingerprint.hash_fnv1a64;
        ctx->source_mtime_unix = fingerprint.mtime_unix;
    }

    parse_path = options->osm_path;
    source_kind = detect_osm_source_kind(options->osm_path);
    ctx->source_kind_detected = source_kind;
    ctx->source_is_canonical_pbf = source_kind == OSM_SOURCE_KIND_PBF;
    ctx->source_compat_xml_mode = source_kind == OSM_SOURCE_KIND_XML;
    if (source_kind == OSM_SOURCE_KIND_XML) {
        log_info("OSM source input kind=xml (compat mode). Canonical input is .osm.pbf for deterministic ingest.");
    } else if (source_kind == OSM_SOURCE_KIND_UNKNOWN) {
        log_info("OSM source kind unknown for %s; attempting XML parse path", options->osm_path);
    }
    if (source_kind == OSM_SOURCE_KIND_PBF) {
        if (!convert_pbf_to_xml(options->osm_path,
                                converted_xml_path,
                                sizeof(converted_xml_path),
                                converter_path,
                                sizeof(converter_path))) {
            return false;
        }
        ctx->source_pbf_conversion_used = true;
        if (converter_path[0] != '\0') {
            snprintf(ctx->source_converter_program, sizeof(ctx->source_converter_program), "%s", converter_path);
        }
        parse_path = converted_xml_path;
    }

    ok = parse_osm_xml(options, ctx, parse_path);
    if (converted_xml_path[0] != '\0') {
        (void)unlink(converted_xml_path);
    }
    if (ok) {
        log_info("OSM source ingest finished: kind=%s", osm_source_kind_label(ctx->source_kind_detected));
    }
    return ok;
}
