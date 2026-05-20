#include "app/app_headless.h"

#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static bool map_forge_headless_error(char *out_error,
                                     size_t out_error_size,
                                     const char *message) {
    if (out_error && out_error_size > 0u) {
        snprintf(out_error, out_error_size, "%s", message ? message : "unknown error");
    }
    return false;
}

static bool json_get_required_string(struct json_object *obj,
                                     const char *key,
                                     char *out_value,
                                     size_t out_size,
                                     char *out_error,
                                     size_t out_error_size) {
    struct json_object *value = NULL;
    const char *raw = NULL;
    if (!obj || !key || !out_value || out_size == 0u) {
        return map_forge_headless_error(out_error, out_error_size, "invalid string parse request");
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_string)) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "missing or invalid string field: %s", key);
        return map_forge_headless_error(out_error, out_error_size, buffer);
    }
    raw = json_object_get_string(value);
    if (!raw || raw[0] == '\0') {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "empty string field: %s", key);
        return map_forge_headless_error(out_error, out_error_size, buffer);
    }
    snprintf(out_value, out_size, "%s", raw);
    return true;
}

static bool json_get_optional_string(struct json_object *obj,
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

static bool json_get_required_u32(struct json_object *obj,
                                  const char *key,
                                  uint32_t *out_value,
                                  char *out_error,
                                  size_t out_error_size) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return map_forge_headless_error(out_error, out_error_size, "invalid integer parse request");
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_int)) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "missing or invalid integer field: %s", key);
        return map_forge_headless_error(out_error, out_error_size, buffer);
    }
    *out_value = (uint32_t)json_object_get_int(value);
    return true;
}

static bool json_get_required_double(struct json_object *obj,
                                     const char *key,
                                     double *out_value,
                                     char *out_error,
                                     size_t out_error_size) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return map_forge_headless_error(out_error, out_error_size, "invalid float parse request");
    }
    if (!json_object_object_get_ex(obj, key, &value) ||
        (!json_object_is_type(value, json_type_double) && !json_object_is_type(value, json_type_int))) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "missing or invalid number field: %s", key);
        return map_forge_headless_error(out_error, out_error_size, buffer);
    }
    *out_value = json_object_get_double(value);
    return true;
}

static bool json_get_optional_bool(struct json_object *obj, const char *key, bool *out_value) {
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

static bool json_get_optional_int(struct json_object *obj, const char *key, int *out_value) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out_value = json_object_get_int(value);
    return true;
}

static bool json_get_optional_float(struct json_object *obj, const char *key, float *out_value) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) ||
        (!json_object_is_type(value, json_type_double) && !json_object_is_type(value, json_type_int))) {
        return false;
    }
    *out_value = (float)json_object_get_double(value);
    return true;
}

static bool map_forge_headless_parse_heading_mode(struct json_object *heading_obj,
                                                  MapForgeHeadlessHeadingMode *out_mode,
                                                  char *out_error,
                                                  size_t out_error_size) {
    char mode[64] = {0};
    if (!out_mode) {
        return map_forge_headless_error(out_error, out_error_size, "missing playback heading mode output");
    }
    *out_mode = MAPFORGE_HEADLESS_HEADING_MODE_BLENDED;
    if (!heading_obj || !json_get_optional_string(heading_obj, "mode", mode, sizeof(mode))) {
        return true;
    }
    if (strcmp(mode, "blended") == 0) {
        *out_mode = MAPFORGE_HEADLESS_HEADING_MODE_BLENDED;
        return true;
    }
    if (strcmp(mode, "lookahead") == 0) {
        *out_mode = MAPFORGE_HEADLESS_HEADING_MODE_LOOKAHEAD;
        return true;
    }
    if (strcmp(mode, "path_tangent") == 0 || strcmp(mode, "tangent") == 0) {
        *out_mode = MAPFORGE_HEADLESS_HEADING_MODE_PATH_TANGENT;
        return true;
    }
    return map_forge_headless_error(out_error,
                                    out_error_size,
                                    "playback.heading.mode must be blended, lookahead, or path_tangent");
}

static bool map_forge_headless_parse_render_mode(struct json_object *output_obj,
                                                 MapForgeHeadlessRenderMode *out_mode,
                                                 char *out_error,
                                                 size_t out_error_size) {
    char mode[64] = {0};
    if (!out_mode) {
        return map_forge_headless_error(out_error, out_error_size, "missing render mode output");
    }
    *out_mode = MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE_MARKER;
    if (!output_obj || !json_get_optional_string(output_obj, "render_mode", mode, sizeof(mode))) {
        return true;
    }
    if (strcmp(mode, "map_route_marker") == 0) {
        *out_mode = MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE_MARKER;
        return true;
    }
    if (strcmp(mode, "map_route") == 0) {
        *out_mode = MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE;
        return true;
    }
    if (strcmp(mode, "map_only") == 0) {
        *out_mode = MAPFORGE_HEADLESS_RENDER_MODE_MAP_ONLY;
        return true;
    }
    return map_forge_headless_error(out_error,
                                    out_error_size,
                                    "output.render_mode must be map_route_marker, map_route, or map_only");
}

static bool map_forge_headless_parse_mode(struct json_object *route_obj,
                                          RouteTravelMode *out_mode,
                                          char *out_error,
                                          size_t out_error_size) {
    char mode[32] = {0};
    if (!out_mode) {
        return map_forge_headless_error(out_error, out_error_size, "missing route mode output");
    }
    *out_mode = ROUTE_MODE_WALK;
    if (!route_obj || !json_get_optional_string(route_obj, "mode", mode, sizeof(mode))) {
        return true;
    }
    if (strcmp(mode, "walking") == 0 || strcmp(mode, "walk") == 0) {
        *out_mode = ROUTE_MODE_WALK;
        return true;
    }
    if (strcmp(mode, "car") == 0 || strcmp(mode, "driving") == 0) {
        *out_mode = ROUTE_MODE_CAR;
        return true;
    }
    return map_forge_headless_error(out_error, out_error_size, "route.mode must be walking or car");
}

bool map_forge_headless_job_load(const char *job_path,
                                 MapForgeHeadlessJob *out_job,
                                 char *out_error,
                                 size_t out_error_size) {
    struct json_object *root = NULL;
    struct json_object *route_obj = NULL;
    struct json_object *camera_obj = NULL;
    struct json_object *playback_obj = NULL;
    struct json_object *heading_obj = NULL;
    struct json_object *output_obj = NULL;
    MapForgeHeadlessJob job;

    if (!job_path || !out_job) {
        return map_forge_headless_error(out_error, out_error_size, "missing job path");
    }

    memset(&job, 0, sizeof(job));
    job.route_mode = ROUTE_MODE_WALK;
    snprintf(job.type, sizeof(job.type), "route_playback_render");
    snprintf(job.output.frame_format, sizeof(job.output.frame_format), "bmp");
    job.playback.heading.mode = MAPFORGE_HEADLESS_HEADING_MODE_BLENDED;
    job.output.render_mode = MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE_MARKER;

    root = json_object_from_file(job_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) {
            json_object_put(root);
        }
        return map_forge_headless_error(out_error, out_error_size, "failed to parse job JSON");
    }

    if (!json_get_required_u32(root, "version", &job.version, out_error, out_error_size) ||
        !json_get_required_string(root, "type", job.type, sizeof(job.type), out_error, out_error_size) ||
        !json_get_required_string(root, "map_region", job.map_region, sizeof(job.map_region), out_error, out_error_size) ||
        !json_get_required_string(root, "pins_file", job.pins_file, sizeof(job.pins_file), out_error, out_error_size) ||
        !json_get_required_string(root, "from_pin", job.from_pin, sizeof(job.from_pin), out_error, out_error_size) ||
        !json_get_required_string(root, "to_pin", job.to_pin, sizeof(job.to_pin), out_error, out_error_size)) {
        json_object_put(root);
        return false;
    }
    if (job.version != 1u && job.version != 2u) {
        json_object_put(root);
        return map_forge_headless_error(out_error, out_error_size, "job version must be 1 or 2");
    }
    if (strcmp(job.type, "route_playback_render") != 0) {
        json_object_put(root);
        return map_forge_headless_error(out_error, out_error_size, "job type must be route_playback_render");
    }

    (void)json_get_optional_string(root, "map_data", job.map_data, sizeof(job.map_data));
    if (json_object_object_get_ex(root, "route", &route_obj) && route_obj && !json_object_is_type(route_obj, json_type_object)) {
        json_object_put(root);
        return map_forge_headless_error(out_error, out_error_size, "route must be an object");
    }
    if (!map_forge_headless_parse_mode(route_obj, &job.route_mode, out_error, out_error_size)) {
        json_object_put(root);
        return false;
    }

    if (json_object_object_get_ex(root, "camera", &camera_obj)) {
        if (!json_object_is_type(camera_obj, json_type_object)) {
            json_object_put(root);
            return map_forge_headless_error(out_error, out_error_size, "camera must be an object");
        }
        job.camera.has_width = json_get_optional_int(camera_obj, "width", &job.camera.width);
        job.camera.has_height = json_get_optional_int(camera_obj, "height", &job.camera.height);
        job.camera.has_zoom = json_get_optional_float(camera_obj, "zoom", &job.camera.zoom);
        (void)json_get_optional_bool(camera_obj, "follow_route", &job.camera.follow_route);
        (void)json_get_optional_bool(camera_obj, "rotate_with_heading", &job.camera.rotate_with_heading);
    }

    if (json_object_object_get_ex(root, "playback", &playback_obj)) {
        if (!json_object_is_type(playback_obj, json_type_object)) {
            json_object_put(root);
            return map_forge_headless_error(out_error, out_error_size, "playback must be an object");
        }
        job.playback.has_duration_seconds = json_get_optional_float(playback_obj, "duration_seconds", &job.playback.duration_seconds);
        job.playback.has_fps = json_get_optional_int(playback_obj, "fps", &job.playback.fps);
        (void)json_get_optional_bool(playback_obj, "start_paused", &job.playback.start_paused);
        if (json_object_object_get_ex(playback_obj, "heading", &heading_obj)) {
            if (!json_object_is_type(heading_obj, json_type_object)) {
                json_object_put(root);
                return map_forge_headless_error(out_error, out_error_size, "playback.heading must be an object");
            }
            if (!map_forge_headless_parse_heading_mode(heading_obj,
                                                       &job.playback.heading.mode,
                                                       out_error,
                                                       out_error_size)) {
                json_object_put(root);
                return false;
            }
            job.playback.heading.has_smoothing_tau_seconds =
                json_get_optional_float(heading_obj, "smoothing_tau_seconds", &job.playback.heading.smoothing_tau_seconds);
            job.playback.heading.has_lookahead_seconds =
                json_get_optional_float(heading_obj, "lookahead_seconds", &job.playback.heading.lookahead_seconds);
            job.playback.heading.has_measurement_window_seconds =
                json_get_optional_float(heading_obj, "measurement_window_seconds", &job.playback.heading.measurement_window_seconds);
            job.playback.heading.has_max_turn_rate_deg_per_sec =
                json_get_optional_float(heading_obj, "max_turn_rate_deg_per_sec", &job.playback.heading.max_turn_rate_deg_per_sec);
        }
    }

    if (json_object_object_get_ex(root, "output", &output_obj)) {
        if (!json_object_is_type(output_obj, json_type_object)) {
            json_object_put(root);
            return map_forge_headless_error(out_error, out_error_size, "output must be an object");
        }
        if (job.version >= 2u) {
            (void)json_get_optional_bool(output_obj, "preview", &job.output.preview_png);
        }
        (void)json_get_optional_bool(output_obj, "preview_png", &job.output.preview_png);
        (void)json_get_optional_bool(output_obj, "frames", &job.output.frames);
        (void)json_get_optional_bool(output_obj, "video_manifest", &job.output.video_manifest);
        (void)json_get_optional_string(output_obj, "frame_format", job.output.frame_format, sizeof(job.output.frame_format));
        if (!map_forge_headless_parse_render_mode(output_obj,
                                                  &job.output.render_mode,
                                                  out_error,
                                                  out_error_size)) {
            json_object_put(root);
            return false;
        }
    }

    json_object_put(root);
    *out_job = job;
    return true;
}

bool map_forge_headless_pins_load(const char *pins_path,
                                  MapForgeHeadlessPinsFile *out_pins,
                                  char *out_error,
                                  size_t out_error_size) {
    struct json_object *root = NULL;
    struct json_object *pins_array = NULL;
    MapForgeHeadlessPinsFile pins_file;
    size_t pin_count = 0u;

    if (!pins_path || !out_pins) {
        return map_forge_headless_error(out_error, out_error_size, "missing pins path");
    }

    memset(&pins_file, 0, sizeof(pins_file));
    root = json_object_from_file(pins_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) {
            json_object_put(root);
        }
        return map_forge_headless_error(out_error, out_error_size, "failed to parse pins JSON");
    }

    if (!json_get_required_u32(root, "version", &pins_file.version, out_error, out_error_size)) {
        json_object_put(root);
        return false;
    }
    if (pins_file.version != 1u) {
        json_object_put(root);
        return map_forge_headless_error(out_error, out_error_size, "pins version must be 1");
    }
    (void)json_get_optional_string(root, "map_region", pins_file.map_region, sizeof(pins_file.map_region));

    if (!json_object_object_get_ex(root, "pins", &pins_array) || !json_object_is_type(pins_array, json_type_array)) {
        json_object_put(root);
        return map_forge_headless_error(out_error, out_error_size, "pins must be an array");
    }
    pin_count = (size_t)json_object_array_length(pins_array);
    if (pin_count == 0u) {
        json_object_put(root);
        return map_forge_headless_error(out_error, out_error_size, "pins array must not be empty");
    }

    pins_file.pins = (MapForgeHeadlessPin *)calloc(pin_count, sizeof(MapForgeHeadlessPin));
    if (!pins_file.pins) {
        json_object_put(root);
        return map_forge_headless_error(out_error, out_error_size, "failed to allocate pins");
    }
    pins_file.pin_count = pin_count;

    for (size_t i = 0; i < pin_count; ++i) {
        struct json_object *pin_obj = json_object_array_get_idx(pins_array, (int)i);
        double lat = 0.0;
        double lon = 0.0;
        if (!pin_obj || !json_object_is_type(pin_obj, json_type_object)) {
            map_forge_headless_pins_file_free(&pins_file);
            json_object_put(root);
            return map_forge_headless_error(out_error, out_error_size, "pin entries must be objects");
        }
        if (!json_get_required_string(pin_obj, "id", pins_file.pins[i].id, sizeof(pins_file.pins[i].id), out_error, out_error_size) ||
            !json_get_required_string(pin_obj, "name", pins_file.pins[i].name, sizeof(pins_file.pins[i].name), out_error, out_error_size) ||
            !json_get_required_double(pin_obj, "lat", &lat, out_error, out_error_size) ||
            !json_get_required_double(pin_obj, "lon", &lon, out_error, out_error_size)) {
            map_forge_headless_pins_file_free(&pins_file);
            json_object_put(root);
            return false;
        }
        pins_file.pins[i].lat = lat;
        pins_file.pins[i].lon = lon;
        (void)json_get_optional_string(pin_obj, "type", pins_file.pins[i].type, sizeof(pins_file.pins[i].type));
        (void)json_get_optional_string(pin_obj, "color", pins_file.pins[i].color, sizeof(pins_file.pins[i].color));
        (void)json_get_optional_string(pin_obj, "notes", pins_file.pins[i].notes, sizeof(pins_file.pins[i].notes));
        (void)json_get_optional_bool(pin_obj, "private", &pins_file.pins[i].private_flag);
    }

    json_object_put(root);
    *out_pins = pins_file;
    return true;
}

void map_forge_headless_pins_file_free(MapForgeHeadlessPinsFile *pins_file) {
    if (!pins_file) {
        return;
    }
    free(pins_file->pins);
    memset(pins_file, 0, sizeof(*pins_file));
}
