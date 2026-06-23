#include "app/app_headless.h"
#include "app_headless_util.h"

#include <json-c/json.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static bool map_forge_headless_parse_heading_mode(struct json_object *heading_obj,
                                                  MapForgeHeadlessHeadingMode *out_mode,
                                                  char *out_error,
                                                  size_t out_error_size) {
    char mode[64] = {0};
    if (!out_mode) {
        return map_forge_headless_fail(out_error, out_error_size, "missing playback heading mode output");
    }
    *out_mode = MAPFORGE_HEADLESS_HEADING_MODE_BLENDED;
    if (!heading_obj || !map_forge_headless_json_get_optional_string(heading_obj, "mode", mode, sizeof(mode))) {
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
    return map_forge_headless_fail(out_error,
                                    out_error_size,
                                    "playback.heading.mode must be blended, lookahead, or path_tangent");
}

static bool map_forge_headless_parse_render_mode(struct json_object *output_obj,
                                                 MapForgeHeadlessRenderMode *out_mode,
                                                 char *out_error,
                                                 size_t out_error_size) {
    char mode[64] = {0};
    if (!out_mode) {
        return map_forge_headless_fail(out_error, out_error_size, "missing render mode output");
    }
    *out_mode = MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE_MARKER;
    if (!output_obj || !map_forge_headless_json_get_optional_string(output_obj, "render_mode", mode, sizeof(mode))) {
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
    return map_forge_headless_fail(out_error,
                                    out_error_size,
                                    "output.render_mode must be map_route_marker, map_route, or map_only");
}

static bool map_forge_headless_parse_quality_profile(struct json_object *output_obj,
                                                     MapForgeHeadlessQualityProfile *out_profile,
                                                     char *out_error,
                                                     size_t out_error_size) {
    char profile[64] = {0};
    if (!out_profile) {
        return map_forge_headless_fail(out_error, out_error_size, "missing quality profile output");
    }
    *out_profile = MAPFORGE_HEADLESS_QUALITY_PROFILE_RUNTIME;
    if (!output_obj || !map_forge_headless_json_get_optional_string(output_obj, "quality_profile", profile, sizeof(profile))) {
        return true;
    }
    if (strcmp(profile, "runtime") == 0) {
        *out_profile = MAPFORGE_HEADLESS_QUALITY_PROFILE_RUNTIME;
        return true;
    }
    if (strcmp(profile, "final") == 0) {
        *out_profile = MAPFORGE_HEADLESS_QUALITY_PROFILE_FINAL;
        return true;
    }
    return map_forge_headless_fail(out_error,
                                    out_error_size,
                                    "output.quality_profile must be runtime or final");
}

static bool map_forge_headless_parse_mode(struct json_object *route_obj,
                                          RouteTravelMode *out_mode,
                                          char *out_error,
                                          size_t out_error_size) {
    char mode[32] = {0};
    if (!out_mode) {
        return map_forge_headless_fail(out_error, out_error_size, "missing route mode output");
    }
    *out_mode = ROUTE_MODE_WALK;
    if (!route_obj || !map_forge_headless_json_get_optional_string(route_obj, "mode", mode, sizeof(mode))) {
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
    return map_forge_headless_fail(out_error, out_error_size, "route.mode must be walking or car");
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
        return map_forge_headless_fail(out_error, out_error_size, "missing job path");
    }

    memset(&job, 0, sizeof(job));
    job.route_mode = ROUTE_MODE_WALK;
    snprintf(job.type, sizeof(job.type), "route_playback_render");
    snprintf(job.output.frame_format, sizeof(job.output.frame_format), "bmp");
    job.playback.heading.mode = MAPFORGE_HEADLESS_HEADING_MODE_BLENDED;
    job.output.render_mode = MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE_MARKER;
    job.output.pixel_scale = 1;
    job.output.allow_tile_fallback = true;
    job.output.simplify_route_screen_space = true;
    job.output.quality_profile = MAPFORGE_HEADLESS_QUALITY_PROFILE_RUNTIME;

    root = json_object_from_file(job_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) {
            json_object_put(root);
        }
        return map_forge_headless_fail(out_error, out_error_size, "failed to parse job JSON");
    }

    if (!map_forge_headless_json_get_required_u32(root, "version", &job.version, out_error, out_error_size) ||
        !map_forge_headless_json_get_required_string(root, "type", job.type, sizeof(job.type), out_error, out_error_size) ||
        !map_forge_headless_json_get_required_string(root, "map_region", job.map_region, sizeof(job.map_region), out_error, out_error_size) ||
        !map_forge_headless_json_get_required_string(root, "from_pin", job.from_pin, sizeof(job.from_pin), out_error, out_error_size) ||
        !map_forge_headless_json_get_required_string(root, "to_pin", job.to_pin, sizeof(job.to_pin), out_error, out_error_size)) {
        json_object_put(root);
        return false;
    }
    (void)map_forge_headless_json_get_optional_string(root, "pins_file", job.pins_file, sizeof(job.pins_file));
    if (job.version != 1u && job.version != 2u) {
        json_object_put(root);
        return map_forge_headless_fail(out_error, out_error_size, "job version must be 1 or 2");
    }
    if (strcmp(job.type, "route_playback_render") != 0) {
        json_object_put(root);
        return map_forge_headless_fail(out_error, out_error_size, "job type must be route_playback_render");
    }

    (void)map_forge_headless_json_get_optional_string(root, "map_data", job.map_data, sizeof(job.map_data));
    if (json_object_object_get_ex(root, "route", &route_obj) && route_obj && !json_object_is_type(route_obj, json_type_object)) {
        json_object_put(root);
        return map_forge_headless_fail(out_error, out_error_size, "route must be an object");
    }
    if (!map_forge_headless_parse_mode(route_obj, &job.route_mode, out_error, out_error_size)) {
        json_object_put(root);
        return false;
    }

    if (json_object_object_get_ex(root, "camera", &camera_obj)) {
        if (!json_object_is_type(camera_obj, json_type_object)) {
            json_object_put(root);
            return map_forge_headless_fail(out_error, out_error_size, "camera must be an object");
        }
        job.camera.has_width = map_forge_headless_json_get_optional_int(camera_obj, "width", &job.camera.width);
        job.camera.has_height = map_forge_headless_json_get_optional_int(camera_obj, "height", &job.camera.height);
        job.camera.has_zoom = map_forge_headless_json_get_optional_float(camera_obj, "zoom", &job.camera.zoom);
        (void)map_forge_headless_json_get_optional_bool(camera_obj, "follow_route", &job.camera.follow_route);
        (void)map_forge_headless_json_get_optional_bool(camera_obj, "rotate_with_heading", &job.camera.rotate_with_heading);
    }

    if (json_object_object_get_ex(root, "playback", &playback_obj)) {
        if (!json_object_is_type(playback_obj, json_type_object)) {
            json_object_put(root);
            return map_forge_headless_fail(out_error, out_error_size, "playback must be an object");
        }
        job.playback.has_duration_seconds = map_forge_headless_json_get_optional_float(playback_obj, "duration_seconds", &job.playback.duration_seconds);
        job.playback.has_fps = map_forge_headless_json_get_optional_int(playback_obj, "fps", &job.playback.fps);
        (void)map_forge_headless_json_get_optional_bool(playback_obj, "start_paused", &job.playback.start_paused);
        if (json_object_object_get_ex(playback_obj, "heading", &heading_obj)) {
            if (!json_object_is_type(heading_obj, json_type_object)) {
                json_object_put(root);
                return map_forge_headless_fail(out_error, out_error_size, "playback.heading must be an object");
            }
            if (!map_forge_headless_parse_heading_mode(heading_obj,
                                                       &job.playback.heading.mode,
                                                       out_error,
                                                       out_error_size)) {
                json_object_put(root);
                return false;
            }
            job.playback.heading.has_smoothing_tau_seconds =
                map_forge_headless_json_get_optional_float(heading_obj, "smoothing_tau_seconds", &job.playback.heading.smoothing_tau_seconds);
            job.playback.heading.has_lookahead_seconds =
                map_forge_headless_json_get_optional_float(heading_obj, "lookahead_seconds", &job.playback.heading.lookahead_seconds);
            job.playback.heading.has_measurement_window_seconds =
                map_forge_headless_json_get_optional_float(heading_obj, "measurement_window_seconds", &job.playback.heading.measurement_window_seconds);
            job.playback.heading.has_max_turn_rate_deg_per_sec =
                map_forge_headless_json_get_optional_float(heading_obj, "max_turn_rate_deg_per_sec", &job.playback.heading.max_turn_rate_deg_per_sec);
        }
    }

    if (json_object_object_get_ex(root, "output", &output_obj)) {
        if (!json_object_is_type(output_obj, json_type_object)) {
            json_object_put(root);
            return map_forge_headless_fail(out_error, out_error_size, "output must be an object");
        }
        if (job.version >= 2u) {
            (void)map_forge_headless_json_get_optional_bool(output_obj, "preview", &job.output.preview_png);
        }
        (void)map_forge_headless_json_get_optional_bool(output_obj, "preview_png", &job.output.preview_png);
        (void)map_forge_headless_json_get_optional_bool(output_obj, "frames", &job.output.frames);
        (void)map_forge_headless_json_get_optional_bool(output_obj, "video_manifest", &job.output.video_manifest);
        (void)map_forge_headless_json_get_optional_string(output_obj, "frame_format", job.output.frame_format, sizeof(job.output.frame_format));
        job.output.has_pixel_scale = map_forge_headless_json_get_optional_int(output_obj, "pixel_scale", &job.output.pixel_scale);
        (void)map_forge_headless_json_get_optional_bool(output_obj, "stabilize_visible_zoom", &job.output.stabilize_visible_zoom);
        (void)map_forge_headless_json_get_optional_bool(output_obj, "stabilize_tile_bands", &job.output.stabilize_tile_bands);
        (void)map_forge_headless_json_get_optional_bool(output_obj, "allow_tile_fallback", &job.output.allow_tile_fallback);
        (void)map_forge_headless_json_get_optional_bool(output_obj, "simplify_route_screen_space", &job.output.simplify_route_screen_space);
        if (!map_forge_headless_parse_render_mode(output_obj,
                                                  &job.output.render_mode,
                                                  out_error,
                                                  out_error_size)) {
            json_object_put(root);
            return false;
        }
        if (!map_forge_headless_parse_quality_profile(output_obj,
                                                      &job.output.quality_profile,
                                                      out_error,
                                                      out_error_size)) {
            json_object_put(root);
            return false;
        }
        if (job.output.pixel_scale < 1) {
            json_object_put(root);
            return map_forge_headless_fail(out_error, out_error_size, "output.pixel_scale must be >= 1");
        }
    }

    json_object_put(root);
    *out_job = job;
    return true;
}

bool map_forge_headless_job_write(const char *job_path,
                                  const MapForgeHeadlessJob *job,
                                  char *out_error,
                                  size_t out_error_size) {
    struct json_object *root = NULL;
    struct json_object *route = NULL;
    struct json_object *camera = NULL;
    struct json_object *playback = NULL;
    struct json_object *heading = NULL;
    struct json_object *output = NULL;
    FILE *file = NULL;
    char parent_dir[PATH_MAX];

    if (!job_path || !job) {
        return map_forge_headless_fail(out_error, out_error_size, "missing job output path");
    }
    if (!map_forge_headless_parent_dir(job_path, parent_dir, sizeof(parent_dir))) {
        return map_forge_headless_fail(out_error, out_error_size, "failed to resolve job output directory");
    }
    if (!map_forge_headless_ensure_dir_recursive_mode(parent_dir, 0755)) {
        return map_forge_headless_fail(out_error, out_error_size, "failed to create job output directory");
    }

    root = json_object_new_object();
    json_object_object_add(root, "version", json_object_new_int((int)job->version));
    json_object_object_add(root, "type", json_object_new_string(job->type));
    json_object_object_add(root, "map_region", json_object_new_string(job->map_region));
    if (job->map_data[0] != '\0') {
        json_object_object_add(root, "map_data", json_object_new_string(job->map_data));
    }
    if (job->pins_file[0] != '\0') {
        json_object_object_add(root, "pins_file", json_object_new_string(job->pins_file));
    }
    json_object_object_add(root, "from_pin", json_object_new_string(job->from_pin));
    json_object_object_add(root, "to_pin", json_object_new_string(job->to_pin));

    route = json_object_new_object();
    json_object_object_add(route,
                           "mode",
                           json_object_new_string(job->route_mode == ROUTE_MODE_CAR ? "car" : "walking"));
    json_object_object_add(root, "route", route);

    camera = json_object_new_object();
    if (job->camera.has_width) {
        json_object_object_add(camera, "width", json_object_new_int(job->camera.width));
    }
    if (job->camera.has_height) {
        json_object_object_add(camera, "height", json_object_new_int(job->camera.height));
    }
    if (job->camera.has_zoom) {
        json_object_object_add(camera, "zoom", json_object_new_double(job->camera.zoom));
    }
    json_object_object_add(camera, "follow_route", json_object_new_boolean(job->camera.follow_route));
    json_object_object_add(camera, "rotate_with_heading", json_object_new_boolean(job->camera.rotate_with_heading));
    json_object_object_add(root, "camera", camera);

    playback = json_object_new_object();
    if (job->playback.has_duration_seconds) {
        json_object_object_add(playback,
                               "duration_seconds",
                               json_object_new_double(job->playback.duration_seconds));
    }
    if (job->playback.has_fps) {
        json_object_object_add(playback, "fps", json_object_new_int(job->playback.fps));
    }
    json_object_object_add(playback, "start_paused", json_object_new_boolean(job->playback.start_paused));
    heading = json_object_new_object();
    switch (job->playback.heading.mode) {
        case MAPFORGE_HEADLESS_HEADING_MODE_LOOKAHEAD:
            json_object_object_add(heading, "mode", json_object_new_string("lookahead"));
            break;
        case MAPFORGE_HEADLESS_HEADING_MODE_PATH_TANGENT:
            json_object_object_add(heading, "mode", json_object_new_string("path_tangent"));
            break;
        case MAPFORGE_HEADLESS_HEADING_MODE_BLENDED:
        default:
            json_object_object_add(heading, "mode", json_object_new_string("blended"));
            break;
    }
    if (job->playback.heading.has_smoothing_tau_seconds) {
        json_object_object_add(heading,
                               "smoothing_tau_seconds",
                               json_object_new_double(job->playback.heading.smoothing_tau_seconds));
    }
    if (job->playback.heading.has_lookahead_seconds) {
        json_object_object_add(heading,
                               "lookahead_seconds",
                               json_object_new_double(job->playback.heading.lookahead_seconds));
    }
    if (job->playback.heading.has_measurement_window_seconds) {
        json_object_object_add(heading,
                               "measurement_window_seconds",
                               json_object_new_double(job->playback.heading.measurement_window_seconds));
    }
    if (job->playback.heading.has_max_turn_rate_deg_per_sec) {
        json_object_object_add(heading,
                               "max_turn_rate_deg_per_sec",
                               json_object_new_double(job->playback.heading.max_turn_rate_deg_per_sec));
    }
    json_object_object_add(playback, "heading", heading);
    json_object_object_add(root, "playback", playback);

    output = json_object_new_object();
    json_object_object_add(output, "preview_png", json_object_new_boolean(job->output.preview_png));
    json_object_object_add(output, "frames", json_object_new_boolean(job->output.frames));
    json_object_object_add(output, "video_manifest", json_object_new_boolean(job->output.video_manifest));
    json_object_object_add(output, "frame_format", json_object_new_string(job->output.frame_format));
    switch (job->output.render_mode) {
        case MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE:
            json_object_object_add(output, "render_mode", json_object_new_string("map_route"));
            break;
        case MAPFORGE_HEADLESS_RENDER_MODE_MAP_ONLY:
            json_object_object_add(output, "render_mode", json_object_new_string("map_only"));
            break;
        case MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE_MARKER:
        default:
            json_object_object_add(output, "render_mode", json_object_new_string("map_route_marker"));
            break;
    }
    json_object_object_add(output, "pixel_scale", json_object_new_int(job->output.pixel_scale));
    json_object_object_add(output,
                           "stabilize_visible_zoom",
                           json_object_new_boolean(job->output.stabilize_visible_zoom));
    json_object_object_add(output,
                           "stabilize_tile_bands",
                           json_object_new_boolean(job->output.stabilize_tile_bands));
    json_object_object_add(output,
                           "allow_tile_fallback",
                           json_object_new_boolean(job->output.allow_tile_fallback));
    json_object_object_add(output,
                           "simplify_route_screen_space",
                           json_object_new_boolean(job->output.simplify_route_screen_space));
    switch (job->output.quality_profile) {
        case MAPFORGE_HEADLESS_QUALITY_PROFILE_FINAL:
            json_object_object_add(output, "quality_profile", json_object_new_string("final"));
            break;
        case MAPFORGE_HEADLESS_QUALITY_PROFILE_RUNTIME:
        default:
            json_object_object_add(output, "quality_profile", json_object_new_string("runtime"));
            break;
    }
    json_object_object_add(root, "output", output);

    file = fopen(job_path, "wb");
    if (!file) {
        json_object_put(root);
        return map_forge_headless_fail(out_error, out_error_size, "failed to open job output path");
    }
    fputs(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY), file);
    fputc('\n', file);
    fclose(file);
    json_object_put(root);
    return true;
}
