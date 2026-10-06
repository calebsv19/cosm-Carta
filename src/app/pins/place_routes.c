#include "app/place_routes.h"
#include "core_io.h"
#include "route/query.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Reads typed route command text without coercing other JSON values.
static const char *route_text(struct json_object *object, const char *key) {
    struct json_object *v = NULL;
    return json_object_object_get_ex(object, key, &v) && json_object_is_type(v, json_type_string)
               ? json_object_get_string(v)
               : "";
}
// Clones route intent for receipts and safe document mutation.
static struct json_object *route_clone(struct json_object *value) {
    return json_tokener_parse(json_object_to_json_string_ext(value, JSON_C_TO_STRING_PLAIN));
}
// Computes compact content provenance for a bounded offline graph.
static uint64_t route_hash(const void *bytes, size_t length) {
    uint64_t h = UINT64_C(14695981039346656037);
    const unsigned char *p = bytes;
    for (size_t i = 0; i < length; ++i) {
        h ^= p[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}
// Returns a structured refusal without saving an incomplete plan.
static struct json_object *route_error(const char *message) {
    struct json_object *r = json_object_new_object();
    json_object_object_add(r, "ok", json_object_new_boolean(false));
    json_object_object_add(r, "error", json_object_new_string(message));
    return r;
}
// Finds saved route identity without treating display names as unique.
static struct json_object *route_find(struct json_object *plans, const char *id) {
    for (size_t i = 0; plans && i < json_object_array_length(plans); ++i) {
        struct json_object *p = json_object_array_get_idx(plans, i);
        if (!strcmp(route_text(p, "id"), id))
            return p;
    }
    return NULL;
}
// Resolves only a declared region ID and checks both endpoints against its coverage.
static bool route_region(const char *id, const MapForgePin *from, const MapForgePin *to,
                         char graph_path[1024]) {
    if (!id[0] || strlen(id) > 128u)
        return false;
    for (const unsigned char *p = (const unsigned char *)id; *p; ++p)
        if (!isalnum(*p) && *p != '_' && *p != '-')
            return false;
    const char *root = getenv("MAPFORGE_REGIONS_DIR");
    if (!root || !root[0])
        root = "data/regions";
    char meta[1024];
    int n = snprintf(meta, sizeof(meta), "%s/%s/meta.json", root, id);
    if (n <= 0 || (size_t)n >= sizeof(meta))
        return false;
    struct json_object *json = json_object_from_file(meta), *bounds = NULL;
    bool valid = false;
    if (json && json_object_object_get_ex(json, "bounds", &bounds)) {
        struct json_object *a = NULL, *b = NULL, *c = NULL, *d = NULL;
        if (json_object_object_get_ex(bounds, "min_lat", &a) &&
            json_object_object_get_ex(bounds, "max_lat", &b) &&
            json_object_object_get_ex(bounds, "min_lon", &c) &&
            json_object_object_get_ex(bounds, "max_lon", &d)) {
            double minlat = json_object_get_double(a), maxlat = json_object_get_double(b),
                   minlon = json_object_get_double(c), maxlon = json_object_get_double(d);
            valid = from->lat >= minlat && from->lat <= maxlat && from->lon >= minlon &&
                    from->lon <= maxlon && to->lat >= minlat && to->lat <= maxlat && to->lon >= minlon &&
                    to->lon <= maxlon;
        }
    }
    if (json)
        json_object_put(json);
    n = snprintf(graph_path, 1024, "%s/%s/graph/graph.bin", root, id);
    return valid && n > 0 && n < 1024;
}
// Reads a bounded pagination field without coercing booleans, strings or fractions.
static bool route_page_value(struct json_object *request, const char *key, int64_t fallback,
                             int64_t minimum, int64_t maximum, int64_t *value) {
    struct json_object *v = NULL;
    *value = fallback;
    if (!json_object_object_get_ex(request, key, &v))
        return true;
    if (!json_object_is_type(v, json_type_int))
        return false;
    *value = json_object_get_int64(v);
    return *value >= minimum && *value <= maximum;
}
// Lists durable intent and current references without loading or solving an offline graph.
static struct json_object *route_list(const MapForgePinsFile *file, struct json_object *plans,
                                      struct json_object *request) {
    int64_t offset, limit;
    if (!route_page_value(request, "offset", 0, 0, 1000, &offset) ||
        !route_page_value(request, "limit", 100, 1, 100, &limit))
        return route_error("offset must be an integer 0..1000 and limit an integer 1..100");
    struct json_object *reply = json_object_new_object(), *rows = json_object_new_array();
    size_t count = plans ? json_object_array_length(plans) : 0;
    size_t end = (size_t)(offset + limit);
    if (end > count)
        end = count;
    for (size_t i = (size_t)offset; i < end; ++i) {
        struct json_object *intent = json_object_array_get_idx(plans, i);
        if (!json_object_is_type(intent, json_type_object)) {
            json_object_put(reply);
            json_object_put(rows);
            return route_error("Invalid saved route intent");
        }
        struct json_object *row = json_object_new_object();
        json_object_object_add(row, "saved_intent", route_clone(intent));
        const char *id_keys[] = {"from_place_id", "to_place_id"};
        const char *name_keys[] = {"from_place_name", "to_place_name"};
        const char *missing_keys[] = {"from_place_missing", "to_place_missing"};
        const char *lat_keys[] = {"from_lat", "to_lat"};
        const char *lon_keys[] = {"from_lon", "to_lon"};
        bool coordinates_changed = false;
        for (size_t j = 0; j < 2; ++j) {
            const MapForgePin *place = map_forge_pins_find_by_id_const(file, route_text(intent, id_keys[j]));
            json_object_object_add(row, name_keys[j], place ? json_object_new_string(place->name) : NULL);
            json_object_object_add(row, missing_keys[j], json_object_new_boolean(!place));
            struct json_object *lat = NULL, *lon = NULL;
            if (place && (!json_object_object_get_ex(intent, lat_keys[j], &lat) ||
                          !json_object_object_get_ex(intent, lon_keys[j], &lon) ||
                          json_object_get_double(lat) != place->lat || json_object_get_double(lon) != place->lon))
                coordinates_changed = true;
        }
        json_object_object_add(row, "coordinates_changed", json_object_new_boolean(coordinates_changed));
        json_object_array_add(rows, row);
    }
    json_object_object_add(reply, "ok", json_object_new_boolean(true));
    json_object_object_add(reply, "routes", rows);
    json_object_object_add(reply, "total", json_object_new_int64((int64_t)count));
    json_object_object_add(reply, "offset", json_object_new_int64(offset));
    json_object_object_add(reply, "limit", json_object_new_int64(limit));
    json_object_object_add(reply, "next_offset", end < count ? json_object_new_int64((int64_t)end) : NULL);
    json_object_object_add(reply, "recomputed", json_object_new_boolean(false));
    json_object_object_add(reply, "graph_status", json_object_new_string("not_checked"));
    return reply;
}
// Renames or archives intent with revision checks, retry receipts and preserved history.
static struct json_object *route_manage(const char *path, MapForgePinsFile *file,
                                        struct json_object *plans, struct json_object *request) {
    struct json_object *saved = route_find(plans, route_text(request, "route_id"));
    if (!saved) return route_error("Unknown route_id");
    const char *rid = route_text(request, "request_id");
    if (!rid[0] || strlen(rid) > 128u) return route_error("Bounded request_id required");
    const char *text = json_object_to_json_string_ext(request, JSON_C_TO_STRING_PLAIN);
    char fingerprint[32];
    snprintf(fingerprint, sizeof(fingerprint), "%016llx", (unsigned long long)route_hash(text, strlen(text)));
    if (!strcmp(route_text(saved, "last_management_request"), rid)) {
        if (strcmp(route_text(saved, "last_management_fingerprint"), fingerprint))
            return route_error("request_id reused with different input");
        struct json_object *reply = route_clone(saved);
        json_object_object_add(reply, "ok", json_object_new_boolean(true));
        return reply;
    }
    struct json_object *revision = NULL, *expected = NULL;
    json_object_object_get_ex(saved, "revision", &revision);
    int64_t rev = revision ? json_object_get_int64(revision) : 0;
    if (!json_object_object_get_ex(request, "expected_revision", &expected) ||
        !json_object_is_type(expected, json_type_int) || json_object_get_int64(expected) != rev)
        return route_error("Stale or missing route revision");
    bool rename = !strcmp(route_text(request, "operation"), "routes.rename");
    struct json_object *value = NULL;
    if (rename) {
        if (!json_object_object_get_ex(request, "name", &value) || !json_object_is_type(value, json_type_string) ||
            !route_text(request, "name")[0] || json_object_get_string_len(value) >= 128 ||
            (size_t)json_object_get_string_len(value) != strlen(route_text(request, "name")))
            return route_error("A bounded route name is required");
    } else if (!json_object_object_get_ex(request, "archived", &value) || !json_object_is_type(value, json_type_boolean))
        return route_error("archived must be a boolean");
    MapForgePinsFile next; map_forge_pins_file_init(&next);
    text = json_object_to_json_string_ext(file->source_json, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    char error[256] = {0};
    if (!map_forge_pins_decode_json(text, strlen(text), &next, error, sizeof(error))) return route_error(error);
    struct json_object *history = NULL, *next_plans = NULL;
    if (!json_object_object_get_ex(next.source_json, "route_history", &history)) {
        history = json_object_new_array(); json_object_object_add(next.source_json, "route_history", history);
    }
    if (!json_object_is_type(history, json_type_array) || json_object_array_length(history) >= 2000u) {
        map_forge_pins_file_free(&next); return route_error("Route history capacity reached");
    }
    json_object_array_add(history, route_clone(saved));
    json_object_object_get_ex(next.source_json, "route_plans", &next_plans);
    struct json_object *target = route_find(next_plans, route_text(request, "route_id"));
    json_object_object_add(target, rename ? "name" : "archived", json_object_get(value));
    json_object_object_add(target, "revision", json_object_new_int64(rev + 1));
    json_object_object_add(target, "last_management_request", json_object_new_string(rid));
    json_object_object_add(target, "last_management_fingerprint", json_object_new_string(fingerprint));
    bool ok = map_forge_pins_save(path, &next, error, sizeof(error));
    map_forge_pins_file_free(&next);
    if (!ok) return route_error(error);
    if (!map_forge_pins_load(path, file, error, sizeof(error))) return route_error("Saved; reload required");
    json_object_object_get_ex(file->source_json, "route_plans", &plans);
    struct json_object *reply = route_clone(route_find(plans, route_text(request, "route_id")));
    json_object_object_add(reply, "ok", json_object_new_boolean(true));
    return reply;
}
// Creates a place-ID route preview and optionally commits its intent atomically.
struct json_object *map_forge_place_route_command(const char *path, MapForgePinsFile *file,
                                                  struct json_object *request) {
    const char *op = route_text(request, "operation");
    bool save = !strcmp(op, "routes.save"), get = !strcmp(op, "routes.get");
    struct json_object *plans = NULL;
    json_object_object_get_ex(file->source_json, "route_plans", &plans);
    if (plans && (!json_object_is_type(plans, json_type_array) || json_object_array_length(plans) > 1000u))
        return route_error("Route plan capacity/type refused");
    if (!strcmp(op, "routes.rename") || !strcmp(op, "routes.archive"))
        return route_manage(path, file, plans, request);
    if (!strcmp(op, "routes.list"))
        return route_list(file, plans, request);
    struct json_object *saved = get ? route_find(plans, route_text(request, "route_id")) : NULL;
    if (get && !saved)
        return route_error("Unknown route_id");
    struct json_object *archived = NULL;
    if (get && json_object_object_get_ex(saved, "archived", &archived) && json_object_get_boolean(archived))
        return route_error("Restore the archived route before opening it");
    char request_token[32];
    const char *request_text = json_object_to_json_string_ext(request, JSON_C_TO_STRING_PLAIN);
    snprintf(request_token, sizeof(request_token), "%016llx",
             (unsigned long long)route_hash(request_text, strlen(request_text)));
    if (save) {
        const char *rid = route_text(request, "request_id");
        if (!rid[0] || strlen(rid) > 128u)
            return route_error("Bounded request_id required");
        for (size_t i = 0; plans && i < json_object_array_length(plans); ++i) {
            struct json_object *p = json_object_array_get_idx(plans, i);
            if (!strcmp(route_text(p, "request_id"), rid)) {
                if (strcmp(route_text(p, "request_fingerprint"), request_token))
                    return route_error("Route request_id reused with different input");
                struct json_object *r = route_clone(p);
                json_object_object_add(r, "ok", json_object_new_boolean(true));
                return r;
            }
        }
    }
    struct json_object *intent = get ? saved : request;
    const MapForgePin *from = map_forge_pins_find_by_id_const(file, route_text(intent, "from_place_id")),
                      *to = map_forge_pins_find_by_id_const(file, route_text(intent, "to_place_id"));
    if (!from || !to)
        return route_error("A referenced place is missing");
    const char *mode = route_text(intent, "mode"), *objective = route_text(intent, "objective");
    if (strcmp(mode, "walk") && strcmp(mode, "car"))
        return route_error("mode must be walk or car");
    if (strcmp(objective, "distance") && strcmp(objective, "time"))
        return route_error("objective must be distance or time");
    char graph_path[1024];
    if (!route_region(route_text(intent, "region"), from, to, graph_path))
        return route_error("Region unavailable or place outside offline coverage");
    CoreBuffer bytes = {0};
    if (core_io_read_all(graph_path, &bytes).code != CORE_OK)
        return route_error("Graph unavailable");
    char graph_token[32];
    snprintf(graph_token, sizeof(graph_token), "%016llx",
             (unsigned long long)route_hash(bytes.data, bytes.size));
    core_io_buffer_free(&bytes);
    RouteState state;
    route_state_init(&state);
    if (!route_state_load_graph(&state, graph_path)) {
        route_state_shutdown(&state);
        return route_error("Invalid offline graph");
    }
    state.mode = !strcmp(mode, "walk") ? ROUTE_MODE_WALK : ROUTE_MODE_CAR;
    state.objective =
        !strcmp(objective, "time") ? ROUTE_OBJECTIVE_LOWEST_TIME : ROUTE_OBJECTIVE_SHORTEST_DISTANCE;
    RouteEndpointAnchor a, b;
    bool ok = route_query_snap(&state.graph, mercator_from_latlon((LatLon){from->lat, from->lon}), state.mode,
                               200.0, &a) &&
              route_query_snap(&state.graph, mercator_from_latlon((LatLon){to->lat, to->lon}), state.mode,
                               200.0, &b) &&
              route_query_solve(&state, &a, &b);
    if (!ok) {
        route_state_shutdown(&state);
        return route_error("No permitted connected route within the 200 m road limit");
    }
    struct json_object *reply = json_object_new_object();
    json_object_object_add(reply, "ok", json_object_new_boolean(true));
    const char *keys[] = {"from_place_id", "to_place_id", "region", "mode", "objective"};
    for (size_t i = 0; i < 5; ++i)
        json_object_object_add(reply, keys[i], json_object_new_string(route_text(intent, keys[i])));
    json_object_object_add(reply, "graph_token", json_object_new_string(graph_token));
    json_object_object_add(reply, "query_policy", json_object_new_string("carta-place-route-v1-ground-200m"));
    json_object_object_add(reply, "from_lat", json_object_new_double(from->lat));
    json_object_object_add(reply, "from_lon", json_object_new_double(from->lon));
    json_object_object_add(reply, "to_lat", json_object_new_double(to->lat));
    json_object_object_add(reply, "to_lon", json_object_new_double(to->lon));
    json_object_object_add(reply, "distance_m", json_object_new_double(state.path.total_length_m));
    json_object_object_add(reply, "time_s", json_object_new_double(state.path.total_time_s));
    json_object_object_add(reply, "from_road_distance_m", json_object_new_double(a.connector_m));
    json_object_object_add(reply, "to_road_distance_m", json_object_new_double(b.connector_m));
    json_object_object_add(reply, "connector_policy",
                           json_object_new_string("straight approximate home-to-road; walking pace 1.4 m/s"));
    json_object_object_add(reply, "path_point_count", json_object_new_int64(state.path.count));
    route_state_shutdown(&state);
    char preview_token[32];
    const char *text = json_object_to_json_string_ext(reply, JSON_C_TO_STRING_PLAIN);
    snprintf(preview_token, sizeof(preview_token), "%016llx",
             (unsigned long long)route_hash(text, strlen(text)));
    json_object_object_add(reply, "preview_token", json_object_new_string(preview_token));
    if (get) {
        struct json_object *v = NULL;
        bool stale = strcmp(route_text(saved, "graph_token"), graph_token) != 0 ||
                     strcmp(route_text(saved, "query_policy"), "carta-place-route-v1-ground-200m") != 0;
        const char *coords[] = {"from_lat", "from_lon", "to_lat", "to_lon"};
        double values[] = {from->lat, from->lon, to->lat, to->lon};
        for (size_t i = 0; i < 4; ++i)
            if (!json_object_object_get_ex(saved, coords[i], &v) || json_object_get_double(v) != values[i])
                stale = true;
        json_object_object_add(reply, "stale", json_object_new_boolean(stale));
        json_object_object_add(reply, "saved_intent", route_clone(saved));
        return reply;
    }
    if (!save)
        return reply;
    if (strcmp(route_text(request, "preview_token"), preview_token)) {
        json_object_put(reply);
        return route_error("Route preview changed; recompute before saving");
    }
    const char *name = route_text(request, "name");
    if (!name[0] || strlen(name) >= 128u) {
        json_object_put(reply);
        return route_error("A bounded route name is required");
    }
    char id[64];
    snprintf(id, sizeof(id), "route-%016llx",
             (unsigned long long)route_hash(route_text(request, "request_id"),
                                            strlen(route_text(request, "request_id"))));
    if (route_find(plans, id)) {
        json_object_put(reply);
        return route_error("Route identity collision");
    }
    json_object_object_add(reply, "id", json_object_new_string(id));
    json_object_object_add(reply, "name", json_object_new_string(name));
    json_object_object_add(reply, "request_id", json_object_new_string(route_text(request, "request_id")));
    json_object_object_add(reply, "request_fingerprint", json_object_new_string(request_token));
    MapForgePinsFile next;
    map_forge_pins_file_init(&next);
    text =
        json_object_to_json_string_ext(file->source_json, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    char error[256] = {0};
    if (!map_forge_pins_decode_json(text, strlen(text), &next, error, sizeof(error))) {
        json_object_put(reply);
        return route_error(error);
    }
    plans = NULL;
    if (!json_object_object_get_ex(next.source_json, "route_plans", &plans)) {
        plans = json_object_new_array();
        json_object_object_add(next.source_json, "route_plans", plans);
    }
    if (!json_object_is_type(plans, json_type_array) || json_object_array_length(plans) >= 1000u) {
        map_forge_pins_file_free(&next);
        json_object_put(reply);
        return route_error("Route plan capacity/type refused");
    }
    json_object_array_add(plans, route_clone(reply));
    ok = map_forge_pins_save(path, &next, error, sizeof(error));
    map_forge_pins_file_free(&next);
    if (!ok) {
        json_object_put(reply);
        return route_error(error);
    }
    if (!map_forge_pins_load(path, file, error, sizeof(error))) {
        json_object_put(reply);
        return route_error("Route committed but readback failed; retry request_id");
    }
    return reply;
}
