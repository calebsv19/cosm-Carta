#include "app/app_pins.h"

#include "core_io.h"

#include <json-c/json.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

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
    *out_value = (uint32_t)json_object_get_int(value);
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
    pins_file->version = 1u;
}

void map_forge_pins_file_free(MapForgePinsFile *pins_file) {
    if (!pins_file) {
        return;
    }
    free(pins_file->pins);
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

bool map_forge_pins_load(const char *pins_path,
                         MapForgePinsFile *out_pins,
                         char *out_error,
                         size_t out_error_size) {
    struct json_object *root = NULL;
    struct json_object *pins_array = NULL;
    MapForgePinsFile pins_file;
    size_t pin_count = 0u;

    if (!pins_path || !out_pins) {
        return map_forge_pins_error(out_error, out_error_size, "missing pins path");
    }

    map_forge_pins_file_init(&pins_file);
    root = json_object_from_file(pins_path);
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
    if (pins_file.version != 1u) {
        json_object_put(root);
        return map_forge_pins_error(out_error, out_error_size, "pins version must be 1");
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
        pins_file.pins[i].lat = lat;
        pins_file.pins[i].lon = lon;
        (void)map_forge_pins_get_optional_string(pin_obj, "type", pins_file.pins[i].type, sizeof(pins_file.pins[i].type));
        (void)map_forge_pins_get_optional_string(pin_obj, "color", pins_file.pins[i].color, sizeof(pins_file.pins[i].color));
        (void)map_forge_pins_get_optional_string(pin_obj, "notes", pins_file.pins[i].notes, sizeof(pins_file.pins[i].notes));
        (void)map_forge_pins_get_optional_string(pin_obj, "created_at", pins_file.pins[i].created_at, sizeof(pins_file.pins[i].created_at));
        (void)map_forge_pins_get_optional_string(pin_obj, "updated_at", pins_file.pins[i].updated_at, sizeof(pins_file.pins[i].updated_at));
        (void)map_forge_pins_get_optional_bool(pin_obj, "private", &pins_file.pins[i].private_flag);
    }

    json_object_put(root);
    *out_pins = pins_file;
    return true;
}

bool map_forge_pins_save(const char *pins_path,
                         const MapForgePinsFile *pins_file,
                         char *out_error,
                         size_t out_error_size) {
    struct json_object *root = NULL;
    struct json_object *pins_array = NULL;
    const char *json_text = NULL;
    CoreResult write_result;

    if (!pins_path || !pins_file) {
        return map_forge_pins_error(out_error, out_error_size, "missing pins save request");
    }

    root = json_object_new_object();
    pins_array = json_object_new_array();
    json_object_object_add(root, "version", json_object_new_int((int)(pins_file->version > 0u ? pins_file->version : 1u)));
    if (pins_file->map_region[0] != '\0') {
        json_object_object_add(root, "map_region", json_object_new_string(pins_file->map_region));
    }

    for (size_t i = 0; i < pins_file->pin_count; ++i) {
        const MapForgePin *pin = &pins_file->pins[i];
        struct json_object *pin_obj = json_object_new_object();
        json_object_object_add(pin_obj, "id", json_object_new_string(pin->id));
        json_object_object_add(pin_obj, "name", json_object_new_string(pin->name));
        json_object_object_add(pin_obj, "lat", json_object_new_double(pin->lat));
        json_object_object_add(pin_obj, "lon", json_object_new_double(pin->lon));
        if (pin->type[0] != '\0') {
            json_object_object_add(pin_obj, "type", json_object_new_string(pin->type));
        }
        if (pin->color[0] != '\0') {
            json_object_object_add(pin_obj, "color", json_object_new_string(pin->color));
        }
        if (pin->notes[0] != '\0') {
            json_object_object_add(pin_obj, "notes", json_object_new_string(pin->notes));
        }
        if (pin->private_flag) {
            json_object_object_add(pin_obj, "private", json_object_new_boolean(1));
        }
        if (pin->created_at[0] != '\0') {
            json_object_object_add(pin_obj, "created_at", json_object_new_string(pin->created_at));
        }
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
    write_result = core_io_write_all(pins_path,
                                     json_text ? json_text : "{}",
                                     json_text ? strlen(json_text) : 2u);
    json_object_put(root);
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
