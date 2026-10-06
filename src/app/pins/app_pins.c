#include "app/app_pins.h"
#include "app/place_repository.h"

#include "core_io.h"

#include <json-c/json.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

// Detects concurrent edits without exposing personal-record bytes in diagnostics.
static uint64_t map_forge_pins_signature(const void *bytes, size_t size) {
    const unsigned char *data = bytes; uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0u; i < size; ++i) { hash ^= data[i]; hash *= 1099511628211ull; }
    return hash;
}

static bool map_forge_pins_error(char *out_error,
                                 size_t out_error_size,
                                 const char *message) {
    if (out_error && out_error_size > 0u) {
        snprintf(out_error, out_error_size, "%s", message ? message : "unknown pins error");
    }
    return false;
}

static bool map_forge_pins_get_required_string(struct json_object *obj,
                                               const char *key,
                                               char *out_value,
                                               size_t out_size,
                                               char *out_error,
                                               size_t out_error_size) {
    struct json_object *value = NULL;
    const char *raw = NULL;
    char message[256];
    if (!obj || !key || !out_value || out_size == 0u) {
        return map_forge_pins_error(out_error, out_error_size, "invalid string parse request");
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_string)) {
        snprintf(message, sizeof(message), "missing or invalid string field: %s", key);
        return map_forge_pins_error(out_error, out_error_size, message);
    }
    raw = json_object_get_string(value);
    if (!raw || raw[0] == '\0') {
        snprintf(message, sizeof(message), "empty string field: %s", key);
        return map_forge_pins_error(out_error, out_error_size, message);
    }
    if ((size_t)json_object_get_string_len(value) >= out_size ||
        strlen(raw) != (size_t)json_object_get_string_len(value)) return false;
    snprintf(out_value, out_size, "%s", raw);
    return true;
}

static bool map_forge_pins_get_optional_string(struct json_object *obj,
                                               const char *key,
                                               char *out_value,
                                               size_t out_size) {
    struct json_object *value = NULL;
    const char *raw = NULL;
    if (!obj || !key || !out_value || out_size == 0u) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_string)) {
        return false;
    }
    raw = json_object_get_string(value);
    if (!raw || raw[0] == '\0') {
        return false;
    }
    if ((size_t)json_object_get_string_len(value) >= out_size ||
        strlen(raw) != (size_t)json_object_get_string_len(value)) return false;
    snprintf(out_value, out_size, "%s", raw);
    return true;
}

static bool map_forge_pins_get_required_u32(struct json_object *obj,
                                            const char *key,
                                            uint32_t *out_value,
                                            char *out_error,
                                            size_t out_error_size) {
    struct json_object *value = NULL;
    char message[256];
    if (!obj || !key || !out_value) {
        return map_forge_pins_error(out_error, out_error_size, "invalid integer parse request");
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_int)) {
        snprintf(message, sizeof(message), "missing or invalid integer field: %s", key);
        return map_forge_pins_error(out_error, out_error_size, message);
    }
    int64_t number = json_object_get_int64(value);
    if (number < 0 || number > UINT32_MAX) return map_forge_pins_error(out_error, out_error_size, "integer out of range");
    *out_value = (uint32_t)number;
    return true;
}

static bool map_forge_pins_get_required_double(struct json_object *obj,
                                               const char *key,
                                               double *out_value,
                                               char *out_error,
                                               size_t out_error_size) {
    struct json_object *value = NULL;
    char message[256];
    if (!obj || !key || !out_value) {
        return map_forge_pins_error(out_error, out_error_size, "invalid float parse request");
    }
    if (!json_object_object_get_ex(obj, key, &value) ||
        (!json_object_is_type(value, json_type_double) && !json_object_is_type(value, json_type_int))) {
        snprintf(message, sizeof(message), "missing or invalid number field: %s", key);
        return map_forge_pins_error(out_error, out_error_size, message);
    }
    *out_value = json_object_get_double(value);
    return true;
}

static bool map_forge_pins_get_optional_bool(struct json_object *obj, const char *key, bool *out_value) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_boolean)) {
        return false;
    }
    *out_value = json_object_get_boolean(value) ? true : false;
    return true;
}

static bool map_forge_pins_reserve(MapForgePinsFile *pins_file, size_t required_count) {
    if (!pins_file) {
        return false;
    }
    if (required_count <= pins_file->pin_capacity) {
        return true;
    }
    size_t new_capacity = pins_file->pin_capacity > 0u ? pins_file->pin_capacity : 8u;
    while (new_capacity < required_count) {
        new_capacity *= 2u;
    }
    MapForgePin *new_pins = (MapForgePin *)realloc(pins_file->pins, new_capacity * sizeof(MapForgePin));
    if (!new_pins) {
        return false;
    }
    if (new_capacity > pins_file->pin_capacity) {
        memset(new_pins + pins_file->pin_capacity, 0, (new_capacity - pins_file->pin_capacity) * sizeof(MapForgePin));
    }
    pins_file->pins = new_pins;
    pins_file->pin_capacity = new_capacity;
    return true;
}

static bool map_forge_pins_ensure_parent_dir(const char *path) {
    char tmp[MAPFORGE_PIN_PATH_CAPACITY];
    size_t len = 0u;
    if (!path || path[0] == '\0') {
        return false;
    }
    len = strnlen(path, sizeof(tmp) - 1u);
    if (len == 0u || len >= sizeof(tmp)) {
        return false;
    }
    memcpy(tmp, path, len);
    tmp[len] = '\0';
    char *slash = strrchr(tmp, '/');
    if (!slash) {
        return true;
    }
    *slash = '\0';
    for (char *p = tmp + 1; *p; ++p) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return false;
        }
        *p = '/';
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static void map_forge_pins_copy(MapForgePin *dst, const MapForgePin *src) {
    if (!dst || !src) {
        return;
    }
    memcpy(dst, src, sizeof(*dst));
}

void map_forge_pins_file_init(MapForgePinsFile *pins_file) {
    if (!pins_file) {
        return;
    }
    memset(pins_file, 0, sizeof(*pins_file));
    pins_file->version = 2u;
}

void map_forge_pins_file_free(MapForgePinsFile *pins_file) {
    if (!pins_file) {
        return;
    }
    free(pins_file->pins);
    if (pins_file->source_json) json_object_put(pins_file->source_json);
    map_forge_pins_file_init(pins_file);
}

bool map_forge_pins_default_private_path(const RegionInfo *region,
                                         char *out_path,
                                         size_t out_path_size) {
    const char *runtime_dir = getenv("MAPFORGE_RUNTIME_DIR");
    if (!out_path || out_path_size == 0u || !region || !region->name || region->name[0] == '\0') {
        return false;
    }
    if (runtime_dir && runtime_dir[0] != '\0') {
        snprintf(out_path,
                 out_path_size,
                 "%s/pins/%s.pins.local.json",
                 runtime_dir,
                 region->name);
        return true;
    }
    snprintf(out_path,
             out_path_size,
             "data/pins/private/%s.pins.local.json",
             region->name);
    return true;
}

bool map_forge_pins_legacy_private_path(const RegionInfo *region,
                                        char *out_path,
                                        size_t out_path_size) {
    if (!out_path || out_path_size == 0u || !region || !region->name || region->name[0] == '\0') {
        return false;
    }
    snprintf(out_path,
             out_path_size,
             "data/pins/private/%s.pins.local.json",
             region->name);
    return true;
}

bool map_forge_pins_load_preferred_region_file(const RegionInfo *region,
                                               MapForgePinsFile *out_pins,
                                               char *out_resolved_path,
                                               size_t out_resolved_path_size,
                                               bool *out_loaded_from_file,
                                               bool *out_loaded_from_legacy,
                                               char *out_error,
                                               size_t out_error_size) {
    char default_path[MAPFORGE_PIN_PATH_CAPACITY];
    char legacy_path[MAPFORGE_PIN_PATH_CAPACITY];

    if (!region || !out_pins) {
        return map_forge_pins_error(out_error, out_error_size, "missing region pins load request");
    }

    if (out_loaded_from_file) {
        *out_loaded_from_file = false;
    }
    if (out_loaded_from_legacy) {
        *out_loaded_from_legacy = false;
    }
    if (out_resolved_path && out_resolved_path_size > 0u) {
        out_resolved_path[0] = '\0';
    }

    map_forge_pins_file_free(out_pins);
    map_forge_pins_file_init(out_pins);
    snprintf(out_pins->map_region, sizeof(out_pins->map_region), "%s", region->name ? region->name : "");

    if (!map_forge_pins_default_private_path(region, default_path, sizeof(default_path))) {
        return map_forge_pins_error(out_error, out_error_size, "failed to resolve default pins path");
    }
    if (out_resolved_path && out_resolved_path_size > 0u) {
        snprintf(out_resolved_path, out_resolved_path_size, "%s", default_path);
    }

    char workspace_path[MAPFORGE_PIN_PATH_CAPACITY];
    if (map_forge_pins_workspace_path(workspace_path, sizeof(workspace_path)) && core_io_path_exists(workspace_path)) {
        if (!map_forge_pins_load(workspace_path, out_pins, out_error, out_error_size)) return false;
        if (out_resolved_path && out_resolved_path_size) snprintf(out_resolved_path, out_resolved_path_size, "%s", workspace_path);
        if (out_loaded_from_file) *out_loaded_from_file = true;
        return true;
    }
    if (map_forge_pins_workspace_path(workspace_path, sizeof(workspace_path))) {
        char *suffix = strrchr(workspace_path, '.');
        if (suffix && (size_t)(suffix-workspace_path)+strlen(".local.json") < sizeof(workspace_path)) {
            strcpy(suffix, ".local.json");
            if (core_io_path_exists(workspace_path)) {
                if (!map_forge_pins_load(workspace_path, out_pins, out_error, out_error_size)) return false;
                if (out_resolved_path && out_resolved_path_size) snprintf(out_resolved_path, out_resolved_path_size, "%s", workspace_path);
                if (out_loaded_from_file) *out_loaded_from_file = true;
                return true;
            }
        }
    }
    if (core_io_path_exists(default_path)) {
        if (!map_forge_pins_load(default_path, out_pins, out_error, out_error_size)) {
            return false;
        }
        if (out_loaded_from_file) {
            *out_loaded_from_file = true;
        }
        return true;
    }

    if (getenv("MAPFORGE_RUNTIME_DIR") &&
        getenv("MAPFORGE_RUNTIME_DIR")[0] != '\0' &&
        map_forge_pins_legacy_private_path(region, legacy_path, sizeof(legacy_path)) &&
        core_io_path_exists(legacy_path)) {
        if (!map_forge_pins_load(legacy_path, out_pins, out_error, out_error_size)) {
            return false;
        }
        if (out_resolved_path && out_resolved_path_size > 0u) {
            snprintf(out_resolved_path, out_resolved_path_size, "%s", legacy_path);
        }
        if (out_loaded_from_file) {
            *out_loaded_from_file = true;
        }
        if (out_loaded_from_legacy) {
            *out_loaded_from_legacy = true;
        }
        return true;
    }

    return true;
}

// Parses one candidate without replacing the caller on validation failure.
static bool map_forge_pins_load_candidate(const char *pins_path,
                         MapForgePinsFile *out_pins,
                         char *out_error,
                         size_t out_error_size) {
    if (!pins_path || !out_pins) {
        return map_forge_pins_error(out_error, out_error_size, "missing pins path");
    }

    CoreBuffer bytes = {0};
    if (core_io_read_all(pins_path, &bytes).code != CORE_OK || bytes.size > INT_MAX) {
        core_io_buffer_free(&bytes);
        return map_forge_pins_error(out_error, out_error_size, "failed to read pins JSON");
    }
    bool ok = map_forge_pins_decode_json((const char *)bytes.data, bytes.size, out_pins, out_error, out_error_size);
    core_io_buffer_free(&bytes);
    return ok;
}

// Parses strict UTF-8 pin documents used by legacy files and the transactional repository.
bool map_forge_pins_decode_json(const char *text, size_t length, MapForgePinsFile *out_pins, char *out_error, size_t out_error_size) {
    if (!text || !out_pins || length > INT_MAX) return false;
    struct json_object *root = NULL, *pins_array = NULL;
    MapForgePinsFile pins_file; map_forge_pins_file_init(&pins_file);
    size_t pin_count = 0u;
    uint64_t source_signature = map_forge_pins_signature((const uint8_t *)text, length);
    struct json_tokener *tokener = json_tokener_new();
    if (!tokener) return false;
    json_tokener_set_flags(tokener, JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
    root = json_tokener_parse_ex(tokener, text, (int)length);
    bool parsed = json_tokener_get_error(tokener) == json_tokener_success;
    size_t consumed = json_tokener_get_parse_end(tokener);
    for (size_t i = consumed; parsed && i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') parsed = false;
    }
    json_tokener_free(tokener);
    if (!parsed && root) { json_object_put(root); root = NULL; }
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) {
            json_object_put(root);
        }
        return map_forge_pins_error(out_error, out_error_size, "failed to parse pins JSON");
    }

    if (!map_forge_pins_get_required_u32(root, "version", &pins_file.version, out_error, out_error_size)) {
        json_object_put(root);
        return false;
    }
    if (pins_file.version != 1u && pins_file.version != 2u) {
        json_object_put(root);
        return map_forge_pins_error(out_error, out_error_size, "pins version must be 1 or 2");
    }
    (void)map_forge_pins_get_optional_string(root, "map_region", pins_file.map_region, sizeof(pins_file.map_region));
    if (!json_object_object_get_ex(root, "pins", &pins_array) || !json_object_is_type(pins_array, json_type_array)) {
        json_object_put(root);
        return map_forge_pins_error(out_error, out_error_size, "pins must be an array");
    }

    pin_count = (size_t)json_object_array_length(pins_array);
    if (!map_forge_pins_reserve(&pins_file, pin_count)) {
        json_object_put(root);
        map_forge_pins_file_free(&pins_file);
        return map_forge_pins_error(out_error, out_error_size, "failed to allocate pins");
    }
    pins_file.pin_count = pin_count;

    for (size_t i = 0; i < pin_count; ++i) {
        struct json_object *pin_obj = json_object_array_get_idx(pins_array, (int)i);
        double lat = 0.0;
        double lon = 0.0;
        if (!pin_obj || !json_object_is_type(pin_obj, json_type_object)) {
            json_object_put(root);
            map_forge_pins_file_free(&pins_file);
            return map_forge_pins_error(out_error, out_error_size, "pin entries must be objects");
        }
        if (!map_forge_pins_get_required_string(pin_obj, "id", pins_file.pins[i].id, sizeof(pins_file.pins[i].id), out_error, out_error_size) ||
            !map_forge_pins_get_required_string(pin_obj, "name", pins_file.pins[i].name, sizeof(pins_file.pins[i].name), out_error, out_error_size) ||
            !map_forge_pins_get_required_double(pin_obj, "lat", &lat, out_error, out_error_size) ||
            !map_forge_pins_get_required_double(pin_obj, "lon", &lon, out_error, out_error_size)) {
            json_object_put(root);
            map_forge_pins_file_free(&pins_file);
            return false;
        }
        if (!isfinite(lat) || !isfinite(lon) || lat < -85.05112878 || lat > 85.05112878 || lon < -180.0 || lon > 180.0) {
            json_object_put(root); map_forge_pins_file_free(&pins_file);
            return map_forge_pins_error(out_error, out_error_size, "pin coordinates out of range");
        }
        for (size_t previous = 0u; previous < i; ++previous) {
            if (strcmp(pins_file.pins[i].id, pins_file.pins[previous].id) == 0) {
                json_object_put(root); map_forge_pins_file_free(&pins_file);
                return map_forge_pins_error(out_error, out_error_size, "duplicate pin ID");
            }
        }
        /* Reject unrepresentable optional fields instead of silently truncating them. */
        const char *keys[] = {"type", "color", "notes", "created_at", "updated_at", "address", "location_source", "location_quality"};
        const size_t caps[] = {MAPFORGE_PIN_TEXT_CAPACITY, MAPFORGE_PIN_TEXT_CAPACITY, MAPFORGE_PIN_TEXT_CAPACITY, 64u, 64u, MAPFORGE_PIN_TEXT_CAPACITY, MAPFORGE_PIN_TEXT_CAPACITY, 64u};
        for (size_t field = 0u; field < 8u; ++field) {
            struct json_object *v = NULL;
            if (json_object_object_get_ex(pin_obj, keys[field], &v) &&
                (!json_object_is_type(v, json_type_string) || (size_t)json_object_get_string_len(v) >= caps[field] ||
                 strlen(json_object_get_string(v)) != (size_t)json_object_get_string_len(v))) {
                json_object_put(root); map_forge_pins_file_free(&pins_file);
                return map_forge_pins_error(out_error, out_error_size, "pin text exceeds supported length or has invalid type");
            }
        }
        struct json_object *collections = NULL;
        pins_file.pins[i].collection_mask = MAPFORGE_COLLECTION_GENERAL;
        if (json_object_object_get_ex(pin_obj, "collections", &collections)) {
            if (!json_object_is_type(collections, json_type_array)) {
                json_object_put(root); map_forge_pins_file_free(&pins_file);
                return map_forge_pins_error(out_error, out_error_size, "collections must be an array");
            }
            pins_file.pins[i].collection_mask = 0u;
            for (size_t c = 0u; c < json_object_array_length(collections); ++c) {
                struct json_object *v = json_object_array_get_idx(collections, c);
                if (!json_object_is_type(v, json_type_string)) {
                    json_object_put(root); map_forge_pins_file_free(&pins_file);
                    return map_forge_pins_error(out_error, out_error_size, "collection IDs must be strings");
                }
                for (uint32_t bit = 1u; bit <= 16u; bit <<= 1u) {
                    if (strcmp(json_object_get_string(v), map_forge_collection_id(bit)) == 0) pins_file.pins[i].collection_mask |= bit;
                }
            }
        }
        (void)map_forge_pins_get_optional_string(pin_obj, "address", pins_file.pins[i].address, sizeof(pins_file.pins[i].address));
        (void)map_forge_pins_get_optional_string(pin_obj, "location_source", pins_file.pins[i].location_source, sizeof(pins_file.pins[i].location_source));
        (void)map_forge_pins_get_optional_string(pin_obj, "location_quality", pins_file.pins[i].location_quality, sizeof(pins_file.pins[i].location_quality));
        pins_file.pins[i].lat = lat;
        pins_file.pins[i].lon = lon;
        (void)map_forge_pins_get_optional_string(pin_obj, "type", pins_file.pins[i].type, sizeof(pins_file.pins[i].type));
        (void)map_forge_pins_get_optional_string(pin_obj, "color", pins_file.pins[i].color, sizeof(pins_file.pins[i].color));
        (void)map_forge_pins_get_optional_string(pin_obj, "notes", pins_file.pins[i].notes, sizeof(pins_file.pins[i].notes));
        (void)map_forge_pins_get_optional_string(pin_obj, "created_at", pins_file.pins[i].created_at, sizeof(pins_file.pins[i].created_at));
        (void)map_forge_pins_get_optional_string(pin_obj, "updated_at", pins_file.pins[i].updated_at, sizeof(pins_file.pins[i].updated_at));
        (void)map_forge_pins_get_optional_bool(pin_obj, "private", &pins_file.pins[i].private_flag);
    }

    pins_file.source_signature = source_signature;
    pins_file.source_json = root;
    pins_file.version = 2u;
    map_forge_pins_file_free(out_pins);
    *out_pins = pins_file;
    return true;
}

// Provides stable identities shared by records in every map region.
const char *map_forge_collection_id(uint32_t collection) {
    switch (collection) {
        case 1u: return "general";
        case 2u: return "dog_walking";
        case 4u: return "pet_sitting";
        case 8u: return "hiking";
        case 16u: return "visited_places";
        default: return "";
    }
}

const char *map_forge_collection_label(uint32_t collection) {
    switch (collection) {
        case 1u: return "General";
        case 2u: return "Dog Walking";
        case 4u: return "Pet Sitting";
        case 8u: return "Hiking";
        case 16u: return "Visited Places";
        default: return "All Places";
    }
}

bool map_forge_pin_in_collection(const MapForgePin *pin, uint32_t collection) {
    return pin && (collection == 0u || ((pin->collection_mask ? pin->collection_mask : 1u) & collection) != 0u);
}

// Validates primary, then explicitly marks recovery from a last-good backup.
bool map_forge_pins_load(const char *path, MapForgePinsFile *out, char *error, size_t error_size) {
    if (!path || !out) return map_forge_pins_error(error, error_size, "missing pins load request");
    if (map_forge_places_is_database(path)) return map_forge_places_load(path, out, error, error_size);
    if (map_forge_pins_load_candidate(path, out, error, error_size)) return true;
    struct json_object *primary = json_object_from_file(path), *version = NULL;
    bool future = primary && json_object_object_get_ex(primary, "version", &version) && json_object_get_int64(version) > 2;
    if (primary) json_object_put(primary);
    if (future) return map_forge_pins_error(error, error_size, "Pins use a newer schema; preserve the file and open it with a compatible app.");
    char backup[MAPFORGE_PIN_PATH_CAPACITY];
    int n = snprintf(backup, sizeof(backup), "%s.bak", path);
    if (n > 0 && (size_t)n < sizeof(backup) && map_forge_pins_load_candidate(backup, out, error, error_size)) {
        out->recovered_from_backup = true;
        if (error && error_size) snprintf(error, error_size, "Recovered last-good pins backup; primary will be repaired on explicit Save.");
        return true;
    }
    return false;
}

// Clones a JSON object so unknown fields survive edits without aliasing source state.
static struct json_object *map_forge_pins_clone(struct json_object *source) {
    return source ? json_tokener_parse(json_object_to_json_string_ext(source, JSON_C_TO_STRING_PLAIN)) : json_object_new_object();
}

// Finds extension metadata by stable ID in the original file.
static struct json_object *map_forge_pins_source_pin(const MapForgePinsFile *file, const char *id) {
    struct json_object *array = NULL;
    if (!file->source_json || !json_object_object_get_ex(file->source_json, "pins", &array)) return NULL;
    for (size_t i = 0u; i < json_object_array_length(array); ++i) {
        struct json_object *pin = json_object_array_get_idx(array, i), *value = NULL;
        if (json_object_object_get_ex(pin, "id", &value) && strcmp(json_object_get_string(value), id) == 0) return pin;
    }
    return NULL;
}

bool map_forge_pins_save(const char *pins_path,
                         MapForgePinsFile *pins_file,
                         char *out_error,
                         size_t out_error_size) {
    struct json_object *root = NULL;
    struct json_object *pins_array = NULL;
    const char *json_text = NULL;
    CoreResult write_result;

    if (!pins_path || !pins_file || (pins_file->pin_count && !pins_file->pins)) {
        return map_forge_pins_error(out_error, out_error_size, "missing pins save request");
    }

    if (pins_file->write_blocked) return map_forge_pins_error(out_error, out_error_size, "Pins are read-only after a load failure; repair or restore the file before saving.");
    root = map_forge_pins_clone(pins_file->source_json);
    pins_array = json_object_new_array();
    json_object_object_add(root, "version", json_object_new_int(2));
    json_object_object_del(root, "map_region");
    if (pins_file->map_region[0] != '\0') {
        json_object_object_add(root, "map_region", json_object_new_string(pins_file->map_region));
    }

    for (size_t i = 0; i < pins_file->pin_count; ++i) {
        const MapForgePin *pin = &pins_file->pins[i];
        if (!isfinite(pin->lat) || !isfinite(pin->lon) || pin->lat < -85.05112878 || pin->lat > 85.05112878 || pin->lon < -180 || pin->lon > 180 ||
            !pin->id[0] || !pin->name[0] || !memchr(pin->id, 0, sizeof(pin->id)) || !memchr(pin->name, 0, sizeof(pin->name)) ||
            !memchr(pin->notes, 0, sizeof(pin->notes)) || !memchr(pin->type, 0, sizeof(pin->type)) || !memchr(pin->color, 0, sizeof(pin->color)) || !memchr(pin->created_at, 0, sizeof(pin->created_at)) || !memchr(pin->updated_at, 0, sizeof(pin->updated_at)) || !memchr(pin->address, 0, sizeof(pin->address)) || !memchr(pin->location_source, 0, sizeof(pin->location_source)) || !memchr(pin->location_quality, 0, sizeof(pin->location_quality))) {
            json_object_put(root); json_object_put(pins_array);
            return map_forge_pins_error(out_error, out_error_size, "invalid pin record");
        }
        for (size_t prior = 0u; prior < i; ++prior) if (strcmp(pin->id, pins_file->pins[prior].id) == 0) {
            json_object_put(root); json_object_put(pins_array);
            return map_forge_pins_error(out_error, out_error_size, "duplicate pin ID in save");
        }
        struct json_object *pin_obj = map_forge_pins_clone(map_forge_pins_source_pin(pins_file, pin->id));
        struct json_object *memberships = json_object_new_array();
        /* Preserve memberships unknown to this app alongside built-in memberships. */
        struct json_object *old_memberships = NULL;
        if (json_object_object_get_ex(pin_obj, "collections", &old_memberships)) {
            for (size_t c = 0u; c < json_object_array_length(old_memberships); ++c) {
                struct json_object *v = json_object_array_get_idx(old_memberships, c);
                bool known = false;
                for (uint32_t bit = 1u; bit <= 16u; bit <<= 1u) if (strcmp(json_object_get_string(v), map_forge_collection_id(bit)) == 0) known = true;
                if (!known) json_object_array_add(memberships, json_object_get(v));
            }
        }
        for (uint32_t bit = 1u; bit <= 16u; bit <<= 1u) if (map_forge_pin_in_collection(pin, bit)) json_object_array_add(memberships, json_object_new_string(map_forge_collection_id(bit)));
        json_object_object_add(pin_obj, "collections", memberships);
        json_object_object_add(pin_obj, "id", json_object_new_string(pin->id));
        json_object_object_add(pin_obj, "name", json_object_new_string(pin->name));
        json_object_object_add(pin_obj, "address", json_object_new_string(pin->address));
        json_object_object_add(pin_obj, "location_source", json_object_new_string(pin->location_source));
        json_object_object_add(pin_obj, "location_quality", json_object_new_string(pin->location_quality));
        json_object_object_add(pin_obj, "lat", json_object_new_double(pin->lat));
        json_object_object_add(pin_obj, "lon", json_object_new_double(pin->lon));
        json_object_object_del(pin_obj, "type");
        if (pin->type[0] != '\0') {
            json_object_object_add(pin_obj, "type", json_object_new_string(pin->type));
        }
        json_object_object_del(pin_obj, "color");
        if (pin->color[0] != '\0') {
            json_object_object_add(pin_obj, "color", json_object_new_string(pin->color));
        }
        json_object_object_del(pin_obj, "notes");
        if (pin->notes[0] != '\0') {
            json_object_object_add(pin_obj, "notes", json_object_new_string(pin->notes));
        }
        json_object_object_add(pin_obj, "private", json_object_new_boolean(pin->private_flag));
        json_object_object_del(pin_obj, "created_at");
        if (pin->created_at[0] != '\0') {
            json_object_object_add(pin_obj, "created_at", json_object_new_string(pin->created_at));
        }
        json_object_object_del(pin_obj, "updated_at");
        if (pin->updated_at[0] != '\0') {
            json_object_object_add(pin_obj, "updated_at", json_object_new_string(pin->updated_at));
        }
        json_object_array_add(pins_array, pin_obj);
    }

    json_object_object_add(root, "pins", pins_array);
    json_text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    if (!map_forge_pins_ensure_parent_dir(pins_path)) {
        json_object_put(root);
        return map_forge_pins_error(out_error, out_error_size, "failed to create pins parent directory");
    }
    if (map_forge_places_is_database(pins_path)) {
        bool ok = map_forge_places_save_document(pins_path, root, pins_file->source_signature, out_error, out_error_size);
        if (ok) {
            pins_file->source_signature = map_forge_pins_signature(json_text, strlen(json_text));
            if (pins_file->source_json) json_object_put(pins_file->source_json);
            pins_file->source_json = root;
        } else json_object_put(root);
        return ok;
    }
    /* Back up only a validated primary, never overwrite a good backup with corruption. */
    if (access(pins_path, F_OK) == 0) {
        MapForgePinsFile previous; map_forge_pins_file_init(&previous);
        if (map_forge_pins_load_candidate(pins_path, &previous, NULL, 0u)) {
            char backup[MAPFORGE_PIN_PATH_CAPACITY]; CoreBuffer bytes = {0};
            int n = snprintf(backup, sizeof(backup), "%s.bak", pins_path);
            CoreResult read = core_io_read_all(pins_path, &bytes);
            if (pins_file->source_signature && read.code == CORE_OK &&
                map_forge_pins_signature(bytes.data, bytes.size) != pins_file->source_signature) {
                core_io_buffer_free(&bytes); map_forge_pins_file_free(&previous); json_object_put(root);
                return map_forge_pins_error(out_error, out_error_size, "Pins changed on disk; reload before saving to preserve those edits.");
            }
            CoreResult saved = (n > 0 && (size_t)n < sizeof(backup) && read.code == CORE_OK)
                ? core_io_write_all_atomic(backup, bytes.data, bytes.size) : (CoreResult){.code = CORE_ERR_IO};
            core_io_buffer_free(&bytes); map_forge_pins_file_free(&previous);
            if (saved.code != CORE_OK) { json_object_put(root); return map_forge_pins_error(out_error, out_error_size, "Failed to preserve last-good backup; save canceled."); }
        } else if (!pins_file->recovered_from_backup) {
            map_forge_pins_file_free(&previous); json_object_put(root);
            return map_forge_pins_error(out_error, out_error_size, "Existing pins file is invalid; refusing to replace it without backup recovery.");
        }
    }
    write_result = core_io_write_all_atomic(pins_path,
                                     json_text ? json_text : "{}",
                                     json_text ? strlen(json_text) : 2u);
    if (write_result.code == CORE_OK) {
        pins_file->source_signature = map_forge_pins_signature(json_text, strlen(json_text));
        if (pins_file->source_json) json_object_put(pins_file->source_json);
        pins_file->source_json = root; root = NULL;
        pins_file->recovered_from_backup = false;
    }
    if (root) json_object_put(root);
    if (write_result.code != CORE_OK) {
        return map_forge_pins_error(out_error, out_error_size, "failed to write pins JSON");
    }
    return true;
}

MapForgePin *map_forge_pins_find_by_id(MapForgePinsFile *pins_file, const char *pin_id) {
    if (!pins_file || !pin_id || pin_id[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < pins_file->pin_count; ++i) {
        if (strcmp(pins_file->pins[i].id, pin_id) == 0) {
            return &pins_file->pins[i];
        }
    }
    return NULL;
}

const MapForgePin *map_forge_pins_find_by_id_const(const MapForgePinsFile *pins_file, const char *pin_id) {
    return map_forge_pins_find_by_id((MapForgePinsFile *)pins_file, pin_id);
}

MapForgePin *map_forge_pins_find_by_name(MapForgePinsFile *pins_file, const char *pin_name) {
    if (!pins_file || !pin_name || pin_name[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < pins_file->pin_count; ++i) {
        if (strcmp(pins_file->pins[i].name, pin_name) == 0) {
            return &pins_file->pins[i];
        }
    }
    return NULL;
}

const MapForgePin *map_forge_pins_find_by_name_const(const MapForgePinsFile *pins_file, const char *pin_name) {
    return map_forge_pins_find_by_name((MapForgePinsFile *)pins_file, pin_name);
}

bool map_forge_pins_upsert(MapForgePinsFile *pins_file,
                           const MapForgePin *pin,
                           char *out_error,
                           size_t out_error_size) {
    if (!pins_file || !pin) {
        return map_forge_pins_error(out_error, out_error_size, "missing pin upsert request");
    }
    if (!isfinite(pin->lat) || !isfinite(pin->lon) || pin->lat < -85.05112878 || pin->lat > 85.05112878 || pin->lon < -180 || pin->lon > 180) return map_forge_pins_error(out_error, out_error_size, "pin coordinates out of range");
    if (pin->id[0] == '\0' || pin->name[0] == '\0') {
        return map_forge_pins_error(out_error, out_error_size, "pin id and name are required");
    }
    MapForgePin *existing = map_forge_pins_find_by_id(pins_file, pin->id);
    if (existing) {
        map_forge_pins_copy(existing, pin);
        return true;
    }
    if (!map_forge_pins_reserve(pins_file, pins_file->pin_count + 1u)) {
        return map_forge_pins_error(out_error, out_error_size, "failed to grow pins array");
    }
    map_forge_pins_copy(&pins_file->pins[pins_file->pin_count], pin);
    pins_file->pin_count += 1u;
    return true;
}

bool map_forge_pins_remove_by_id(MapForgePinsFile *pins_file, const char *pin_id) {
    if (!pins_file || !pin_id || pin_id[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i < pins_file->pin_count; ++i) {
        if (strcmp(pins_file->pins[i].id, pin_id) != 0) {
            continue;
        }
        if (i + 1u < pins_file->pin_count) {
            memmove(&pins_file->pins[i],
                    &pins_file->pins[i + 1u],
                    (pins_file->pin_count - (i + 1u)) * sizeof(MapForgePin));
        }
        pins_file->pin_count -= 1u;
        memset(&pins_file->pins[pins_file->pin_count], 0, sizeof(MapForgePin));
        return true;
    }
    return false;
}

bool map_forge_pins_move(MapForgePinsFile *pins_file, size_t from_index, size_t to_index) {
    MapForgePin moved = {0};
    if (!pins_file || from_index >= pins_file->pin_count || to_index >= pins_file->pin_count) {
        return false;
    }
    if (from_index == to_index) {
        return true;
    }
    moved = pins_file->pins[from_index];
    if (from_index < to_index) {
        memmove(&pins_file->pins[from_index],
                &pins_file->pins[from_index + 1u],
                (to_index - from_index) * sizeof(MapForgePin));
    } else {
        memmove(&pins_file->pins[to_index + 1u],
                &pins_file->pins[to_index],
                (from_index - to_index) * sizeof(MapForgePin));
    }
    pins_file->pins[to_index] = moved;
    return true;
}
