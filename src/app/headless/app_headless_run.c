#include "app/app_headless.h"

#include "app/region.h"
#include "app/region_loader.h"
#include "core_io.h"
#include "map/mercator.h"
#include "route/route.h"

#include <json-c/json.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

typedef struct MapForgeHeadlessResolvedPin {
    const MapForgePin *pin;
    uint32_t nearest_node;
    double world_x;
    double world_y;
    double node_distance_m;
} MapForgeHeadlessResolvedPin;

typedef struct MapForgeHeadlessRunResult {
    bool ok;
    bool route_computed;
    bool playback_trace_written;
    bool preview_written;
    bool frames_written;
    char status[32];
    char error[256];
    char job_path[PATH_MAX];
    char out_dir[PATH_MAX];
    char pins_path[PATH_MAX];
    char graph_path[PATH_MAX];
    char command[2048];
    char timestamp_utc[64];
    char git_commit[64];
    RegionInfo region;
    MapForgeHeadlessJob job;
    MapForgeHeadlessResolvedPin from_pin;
    MapForgeHeadlessResolvedPin to_pin;
    RouteState route_state;
    float playback_duration_s;
    int playback_fps;
    uint32_t estimated_frame_count;
    uint32_t frames_written_count;
    MapForgeHeadlessPlaybackSample *frame_samples;
    MapForgeHeadlessImageExportResult image_exports;
    MapForgeHeadlessWarningSet warnings;
} MapForgeHeadlessRunResult;

static const char *map_forge_headless_heading_mode_name(MapForgeHeadlessHeadingMode mode) {
    switch (mode) {
        case MAPFORGE_HEADLESS_HEADING_MODE_LOOKAHEAD:
            return "lookahead";
        case MAPFORGE_HEADLESS_HEADING_MODE_PATH_TANGENT:
            return "path_tangent";
        case MAPFORGE_HEADLESS_HEADING_MODE_BLENDED:
        default:
            return "blended";
    }
}

static const char *map_forge_headless_render_mode_name(MapForgeHeadlessRenderMode mode) {
    switch (mode) {
        case MAPFORGE_HEADLESS_RENDER_MODE_MAP_ONLY:
            return "map_only";
        case MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE:
            return "map_route";
        case MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE_MARKER:
        default:
            return "map_route_marker";
    }
}

static bool map_forge_headless_warning_add(MapForgeHeadlessWarningSet *warnings, const char *message) {
    if (!warnings || !message || message[0] == '\0') {
        return false;
    }
    if (warnings->count >= MAPFORGE_HEADLESS_WARNING_CAPACITY) {
        return false;
    }
    snprintf(warnings->items[warnings->count],
             sizeof(warnings->items[warnings->count]),
             "%s",
             message);
    warnings->count += 1u;
    return true;
}

static bool map_forge_headless_set_error(MapForgeHeadlessRunResult *result, const char *message) {
    if (result && message) {
        snprintf(result->error, sizeof(result->error), "%s", message);
        snprintf(result->status, sizeof(result->status), "failed");
    }
    return false;
}

static bool map_forge_headless_ensure_dir_recursive(const char *path) {
    char tmp[PATH_MAX];
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

static bool map_forge_headless_write_text_file(const char *path, const char *text) {
    CoreResult write_result;
    if (!path || !text) {
        return false;
    }
    write_result = core_io_write_all(path, text, strlen(text));
    return write_result.code == CORE_OK;
}

static void map_forge_headless_format_timestamp(char *out_text, size_t out_size) {
    time_t now = time(NULL);
    struct tm tm_utc;
    if (!out_text || out_size == 0u) {
        return;
    }
#if defined(_WIN32)
    gmtime_s(&tm_utc, &now);
#else
    gmtime_r(&now, &tm_utc);
#endif
    strftime(out_text, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

static void map_forge_headless_capture_git_commit(char *out_text, size_t out_size) {
    FILE *pipe = NULL;
    if (!out_text || out_size == 0u) {
        return;
    }
    out_text[0] = '\0';
    pipe = popen("git rev-parse --short HEAD 2>/dev/null", "r");
    if (!pipe) {
        return;
    }
    if (fgets(out_text, (int)out_size, pipe)) {
        size_t len = strlen(out_text);
        while (len > 0u && (out_text[len - 1u] == '\n' || out_text[len - 1u] == '\r')) {
            out_text[--len] = '\0';
        }
    }
    (void)pclose(pipe);
}

static void map_forge_headless_format_command(int argc,
                                              char **argv,
                                              char *out_text,
                                              size_t out_size) {
    size_t used = 0u;
    if (!out_text || out_size == 0u) {
        return;
    }
    out_text[0] = '\0';
    for (int i = 0; i < argc; ++i) {
        const char *arg = argv[i] ? argv[i] : "";
        int written = snprintf(out_text + used,
                               out_size - used,
                               "%s%s%s%s",
                               used == 0u ? "" : " ",
                               strchr(arg, ' ') ? "\"" : "",
                               arg,
                               strchr(arg, ' ') ? "\"" : "");
        if (written < 0 || (size_t)written >= out_size - used) {
            break;
        }
        used += (size_t)written;
    }
}

static bool map_forge_headless_realpath_or_copy(const char *path,
                                                char *out_path,
                                                size_t out_size) {
    char resolved[PATH_MAX];
    if (!path || !out_path || out_size == 0u) {
        return false;
    }
    if (realpath(path, resolved)) {
        snprintf(out_path, out_size, "%s", resolved);
        return true;
    }
    snprintf(out_path, out_size, "%s", path);
    return true;
}

static bool map_forge_headless_parent_dir(const char *path,
                                          char *out_dir,
                                          size_t out_size) {
    const char *slash = NULL;
    if (!path || !out_dir || out_size == 0u) {
        return false;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out_dir, out_size, ".");
        return true;
    }
    if (slash == path) {
        snprintf(out_dir, out_size, "/");
        return true;
    }
    {
        size_t len = (size_t)(slash - path);
        if (len >= out_size) {
            return false;
        }
        memcpy(out_dir, path, len);
        out_dir[len] = '\0';
    }
    return true;
}

static bool map_forge_headless_resolve_relative(const char *base_dir,
                                                const char *path,
                                                char *out_path,
                                                size_t out_size) {
    if (!path || !out_path || out_size == 0u) {
        return false;
    }
    if (path[0] == '/') {
        return map_forge_headless_realpath_or_copy(path, out_path, out_size);
    }
    if (!base_dir || base_dir[0] == '\0') {
        return map_forge_headless_realpath_or_copy(path, out_path, out_size);
    }
    {
        char current_dir[PATH_MAX];
        char parent_dir[PATH_MAX];

        snprintf(current_dir, sizeof(current_dir), "%s", base_dir);
        while (current_dir[0] != '\0') {
            char joined[PATH_MAX];
            snprintf(joined, sizeof(joined), "%s/%s", current_dir, path);
            if (realpath(joined, out_path)) {
                return true;
            }
            if (!map_forge_headless_parent_dir(current_dir, parent_dir, sizeof(parent_dir))) {
                break;
            }
            if (strcmp(parent_dir, current_dir) == 0) {
                break;
            }
            snprintf(current_dir, sizeof(current_dir), "%s", parent_dir);
            if (strcmp(current_dir, "/") == 0) {
                snprintf(joined, sizeof(joined), "/%s", path);
                if (realpath(joined, out_path)) {
                    return true;
                }
                break;
            }
        }
    }
    return map_forge_headless_realpath_or_copy(path, out_path, out_size);
}

static const RegionInfo *map_forge_headless_find_region(const char *name) {
    int count = region_count();
    for (int i = 0; i < count; ++i) {
        const RegionInfo *info = region_get(i);
        if (info && info->name && strcmp(info->name, name) == 0) {
            return info;
        }
    }
    return NULL;
}

static const MapForgePin *map_forge_headless_find_pin(const MapForgePinsFile *pins_file,
                                                      const char *key) {
    const MapForgePin *pin = NULL;
    if (!pins_file || !key) {
        return NULL;
    }
    pin = map_forge_pins_find_by_id_const(pins_file, key);
    if (pin) {
        return pin;
    }
    return map_forge_pins_find_by_name_const(pins_file, key);
}

static bool map_forge_headless_find_nearest_node(const RouteGraph *graph,
                                                 double world_x,
                                                 double world_y,
                                                 uint32_t *out_node,
                                                 double *out_distance_m) {
    uint32_t best_node = 0u;
    double best_dist_sq = 0.0;
    if (!graph || graph->node_count == 0u || !graph->node_x || !graph->node_y || !out_node || !out_distance_m) {
        return false;
    }
    for (uint32_t i = 0; i < graph->node_count; ++i) {
        double dx = graph->node_x[i] - world_x;
        double dy = graph->node_y[i] - world_y;
        double dist_sq = dx * dx + dy * dy;
        if (i == 0u || dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_node = i;
        }
    }
    *out_node = best_node;
    *out_distance_m = sqrt(best_dist_sq);
    return true;
}

static bool map_forge_headless_resolve_pin(const RouteGraph *graph,
                                           const MapForgePin *pin,
                                           MapForgeHeadlessResolvedPin *out_resolved) {
    MercatorMeters meters;
    if (!graph || !pin || !out_resolved) {
        return false;
    }
    memset(out_resolved, 0, sizeof(*out_resolved));
    meters = mercator_from_latlon((LatLon){pin->lat, pin->lon});
    out_resolved->pin = pin;
    out_resolved->world_x = meters.x;
    out_resolved->world_y = meters.y;
    if (!map_forge_headless_find_nearest_node(graph,
                                              meters.x,
                                              meters.y,
                                              &out_resolved->nearest_node,
                                              &out_resolved->node_distance_m)) {
        return false;
    }
    return true;
}

static void map_forge_headless_record_job_warnings(MapForgeHeadlessRunResult *result) {
    bool image_outputs_requested = false;
    if (!result) {
        return;
    }
    if (result->job.map_data[0] != '\0') {
        (void)map_forge_headless_warning_add(&result->warnings,
                                             "job.map_data is currently advisory only; region pack paths are still resolved through map_region.");
    }
    if (result->job.camera.has_width || result->job.camera.has_height || result->job.camera.has_zoom ||
        result->job.camera.follow_route || result->job.camera.rotate_with_heading) {
        (void)map_forge_headless_warning_add(&result->warnings,
                                             "headless export applies the requested camera framing, zoom, and heading-up behavior, but it does not include the interactive HUD or live input-driven camera adjustments.");
    }
    if ((result->job.playback.has_duration_seconds || result->job.playback.has_fps || result->job.playback.start_paused) &&
        !result->playback_trace_written) {
        (void)map_forge_headless_warning_add(&result->warnings,
                                             "playback settings only drive deterministic traces and frames when both playback.duration_seconds and playback.fps are set.");
    }
    image_outputs_requested = result->job.output.preview_png || result->job.output.frames;
    if (image_outputs_requested) {
        if (!result->preview_written && !result->frames_written) {
            (void)map_forge_headless_warning_add(&result->warnings,
                                                 "preview/frame image outputs were requested, but this slice could not emit headless route images.");
        }
        if (result->job.output.frames && !result->frames_written) {
            (void)map_forge_headless_warning_add(&result->warnings,
                                                 "frame sequence export currently requires playback.duration_seconds and playback.fps so headless playback can step deterministically.");
        }
    }
    if (result->job.output.video_manifest) {
        (void)map_forge_headless_warning_add(&result->warnings,
                                             "video_manifest was requested, but ffmpeg/video packaging remains a later slice after real frame image export exists.");
    }
}

static bool map_forge_headless_write_json_file(const char *path, struct json_object *root) {
    const char *text = NULL;
    if (!path || !root) {
        return false;
    }
    text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    return map_forge_headless_write_text_file(path, text ? text : "{}\n");
}

static bool map_forge_headless_write_playback_trace(const MapForgeHeadlessRunResult *result) {
    char path[PATH_MAX];
    struct json_object *root = NULL;
    struct json_object *samples = NULL;
    MapForgeHeadlessPlaybackHeadingState heading_state;

    if (!result || !result->route_computed || result->estimated_frame_count == 0u) {
        return false;
    }

    root = json_object_new_object();
    samples = json_object_new_array();
    json_object_object_add(root, "version", json_object_new_int((int)result->job.version));
    json_object_object_add(root, "map_region", json_object_new_string(result->job.map_region));
    json_object_object_add(root, "from_pin", json_object_new_string(result->from_pin.pin ? result->from_pin.pin->id : ""));
    json_object_object_add(root, "to_pin", json_object_new_string(result->to_pin.pin ? result->to_pin.pin->id : ""));
    json_object_object_add(root, "playback_duration_s", json_object_new_double(result->playback_duration_s));
    json_object_object_add(root, "fps", json_object_new_int(result->playback_fps));
    json_object_object_add(root, "frame_count", json_object_new_int64((int64_t)result->estimated_frame_count));
    json_object_object_add(root, "route_duration_s", json_object_new_double(result->route_state.path.total_time_s));

    map_forge_headless_playback_reset_heading_state(&heading_state);
    for (uint32_t i = 0; i < result->estimated_frame_count; ++i) {
        struct json_object *sample_obj = json_object_new_object();
        MapForgeHeadlessPlaybackSample sample;
        float progress = result->estimated_frame_count > 1u
                             ? (float)i / (float)(result->estimated_frame_count - 1u)
                             : 0.0f;
        float playback_time_s = progress * result->playback_duration_s;
        float route_time_s = progress * result->route_state.path.total_time_s;

        memset(&sample, 0, sizeof(sample));
        if (!map_forge_headless_playback_sample(&result->route_state.graph,
                                                &result->route_state.path,
                                                &result->job.playback,
                                                route_time_s,
                                                &heading_state,
                                                &sample)) {
            json_object_put(root);
            return false;
        }

        json_object_object_add(sample_obj, "frame_index", json_object_new_int64((int64_t)i));
        json_object_object_add(sample_obj, "progress", json_object_new_double(progress));
        json_object_object_add(sample_obj, "playback_time_s", json_object_new_double(playback_time_s));
        json_object_object_add(sample_obj, "route_time_s", json_object_new_double(sample.route_time_s));
        json_object_object_add(sample_obj, "segment_index", json_object_new_int64((int64_t)sample.segment_index));
        json_object_object_add(sample_obj, "world_x", json_object_new_double(sample.world_x));
        json_object_object_add(sample_obj, "world_y", json_object_new_double(sample.world_y));
        json_object_object_add(sample_obj, "heading_rad", json_object_new_double(sample.heading_rad));
        json_object_object_add(sample_obj, "has_lookahead", json_object_new_boolean(sample.has_lookahead));
        json_object_object_add(sample_obj, "lookahead_world_x", json_object_new_double(sample.lookahead_world_x));
        json_object_object_add(sample_obj, "lookahead_world_y", json_object_new_double(sample.lookahead_world_y));
        json_object_array_add(samples, sample_obj);
    }

    json_object_object_add(root, "samples", samples);
    snprintf(path, sizeof(path), "%s/playback_trace.json", result->out_dir);
    if (!map_forge_headless_write_json_file(path, root)) {
        json_object_put(root);
        return false;
    }
    json_object_put(root);
    return true;
}

static bool map_forge_headless_build_playback_samples(MapForgeHeadlessRunResult *result) {
    MapForgeHeadlessPlaybackHeadingState heading_state;
    if (!result || !result->route_computed || result->estimated_frame_count == 0u) {
        return false;
    }
    result->frame_samples =
        (MapForgeHeadlessPlaybackSample *)calloc(result->estimated_frame_count, sizeof(MapForgeHeadlessPlaybackSample));
    if (!result->frame_samples) {
        return false;
    }
    map_forge_headless_playback_reset_heading_state(&heading_state);
    for (uint32_t i = 0; i < result->estimated_frame_count; ++i) {
        float progress = result->estimated_frame_count > 1u
                             ? (float)i / (float)(result->estimated_frame_count - 1u)
                             : 0.0f;
        float route_time_s = progress * result->route_state.path.total_time_s;
        if (!map_forge_headless_playback_sample(&result->route_state.graph,
                                                &result->route_state.path,
                                                &result->job.playback,
                                                route_time_s,
                                                &heading_state,
                                                &result->frame_samples[i])) {
            return false;
        }
    }
    return true;
}

static bool map_forge_headless_build_preview_sample(MapForgeHeadlessRunResult *result,
                                                    MapForgeHeadlessPlaybackSample *out_sample) {
    MapForgeHeadlessPlaybackHeadingState heading_state;
    float route_time_s = 0.0f;

    if (!result || !result->route_computed || !out_sample) {
        return false;
    }
    memset(out_sample, 0, sizeof(*out_sample));
    route_time_s = result->route_state.path.total_time_s * 0.5f;
    map_forge_headless_playback_reset_heading_state(&heading_state);
    return map_forge_headless_playback_sample(&result->route_state.graph,
                                              &result->route_state.path,
                                              &result->job.playback,
                                              route_time_s,
                                              &heading_state,
                                              out_sample);
}

static struct json_object *map_forge_headless_build_resolved_job_json(const MapForgeHeadlessRunResult *result) {
    struct json_object *root = json_object_new_object();
    struct json_object *paths = json_object_new_object();
    struct json_object *route = json_object_new_object();
    struct json_object *camera = json_object_new_object();
    struct json_object *playback = json_object_new_object();
    struct json_object *output = json_object_new_object();
    struct json_object *from_pin = json_object_new_object();
    struct json_object *to_pin = json_object_new_object();
    if (!result) {
        return root;
    }
    json_object_object_add(root, "version", json_object_new_int((int)result->job.version));
    json_object_object_add(root, "type", json_object_new_string(result->job.type));
    json_object_object_add(root, "map_region", json_object_new_string(result->job.map_region));
    json_object_object_add(root, "job_path", json_object_new_string(result->job_path));
    json_object_object_add(root, "out_dir", json_object_new_string(result->out_dir));

    json_object_object_add(paths, "pins_file", json_object_new_string(result->pins_path));
    json_object_object_add(paths, "graph_path", json_object_new_string(result->graph_path));
    if (result->job.map_data[0] != '\0') {
        json_object_object_add(paths, "map_data", json_object_new_string(result->job.map_data));
    }
    json_object_object_add(root, "paths", paths);

    json_object_object_add(from_pin, "id", json_object_new_string(result->from_pin.pin ? result->from_pin.pin->id : ""));
    json_object_object_add(from_pin, "name", json_object_new_string(result->from_pin.pin ? result->from_pin.pin->name : ""));
    json_object_object_add(from_pin, "lat", json_object_new_double(result->from_pin.pin ? result->from_pin.pin->lat : 0.0));
    json_object_object_add(from_pin, "lon", json_object_new_double(result->from_pin.pin ? result->from_pin.pin->lon : 0.0));
    json_object_object_add(from_pin, "world_x", json_object_new_double(result->from_pin.world_x));
    json_object_object_add(from_pin, "world_y", json_object_new_double(result->from_pin.world_y));
    json_object_object_add(from_pin, "nearest_node", json_object_new_int64((int64_t)result->from_pin.nearest_node));
    json_object_object_add(from_pin, "nearest_node_distance_m", json_object_new_double(result->from_pin.node_distance_m));

    json_object_object_add(to_pin, "id", json_object_new_string(result->to_pin.pin ? result->to_pin.pin->id : ""));
    json_object_object_add(to_pin, "name", json_object_new_string(result->to_pin.pin ? result->to_pin.pin->name : ""));
    json_object_object_add(to_pin, "lat", json_object_new_double(result->to_pin.pin ? result->to_pin.pin->lat : 0.0));
    json_object_object_add(to_pin, "lon", json_object_new_double(result->to_pin.pin ? result->to_pin.pin->lon : 0.0));
    json_object_object_add(to_pin, "world_x", json_object_new_double(result->to_pin.world_x));
    json_object_object_add(to_pin, "world_y", json_object_new_double(result->to_pin.world_y));
    json_object_object_add(to_pin, "nearest_node", json_object_new_int64((int64_t)result->to_pin.nearest_node));
    json_object_object_add(to_pin, "nearest_node_distance_m", json_object_new_double(result->to_pin.node_distance_m));

    json_object_object_add(route, "mode", json_object_new_string(result->job.route_mode == ROUTE_MODE_CAR ? "car" : "walking"));
    json_object_object_add(route, "from_pin", from_pin);
    json_object_object_add(route, "to_pin", to_pin);
    if (result->route_computed) {
        json_object_object_add(route, "path_node_count", json_object_new_int64((int64_t)result->route_state.path.count));
        json_object_object_add(route, "path_total_length_m", json_object_new_double(result->route_state.path.total_length_m));
        json_object_object_add(route, "path_total_time_s", json_object_new_double(result->route_state.path.total_time_s));
        json_object_object_add(route, "alternative_count", json_object_new_int64((int64_t)result->route_state.alternatives.count));
    }
    json_object_object_add(root, "route", route);

    if (result->job.camera.has_width) {
        json_object_object_add(camera, "width", json_object_new_int(result->job.camera.width));
    }
    if (result->job.camera.has_height) {
        json_object_object_add(camera, "height", json_object_new_int(result->job.camera.height));
    }
    if (result->job.camera.has_zoom) {
        json_object_object_add(camera, "zoom", json_object_new_double(result->job.camera.zoom));
    }
    json_object_object_add(camera, "follow_route", json_object_new_boolean(result->job.camera.follow_route));
    json_object_object_add(camera, "rotate_with_heading", json_object_new_boolean(result->job.camera.rotate_with_heading));
    json_object_object_add(root, "camera", camera);

    if (result->job.playback.has_duration_seconds) {
        json_object_object_add(playback, "duration_seconds", json_object_new_double(result->job.playback.duration_seconds));
    }
    if (result->job.playback.has_fps) {
        json_object_object_add(playback, "fps", json_object_new_int(result->job.playback.fps));
    }
    json_object_object_add(playback, "start_paused", json_object_new_boolean(result->job.playback.start_paused));
    {
        struct json_object *heading = json_object_new_object();
        json_object_object_add(heading, "mode",
                               json_object_new_string(map_forge_headless_heading_mode_name(result->job.playback.heading.mode)));
        if (result->job.playback.heading.has_smoothing_tau_seconds) {
            json_object_object_add(heading, "smoothing_tau_seconds",
                                   json_object_new_double(result->job.playback.heading.smoothing_tau_seconds));
        }
        if (result->job.playback.heading.has_lookahead_seconds) {
            json_object_object_add(heading, "lookahead_seconds",
                                   json_object_new_double(result->job.playback.heading.lookahead_seconds));
        }
        if (result->job.playback.heading.has_measurement_window_seconds) {
            json_object_object_add(heading, "measurement_window_seconds",
                                   json_object_new_double(result->job.playback.heading.measurement_window_seconds));
        }
        if (result->job.playback.heading.has_max_turn_rate_deg_per_sec) {
            json_object_object_add(heading, "max_turn_rate_deg_per_sec",
                                   json_object_new_double(result->job.playback.heading.max_turn_rate_deg_per_sec));
        }
        json_object_object_add(playback, "heading", heading);
    }
    if (result->playback_duration_s > 0.0f) {
        json_object_object_add(playback, "resolved_duration_seconds", json_object_new_double(result->playback_duration_s));
    }
    if (result->playback_fps > 0) {
        json_object_object_add(playback, "resolved_fps", json_object_new_int(result->playback_fps));
    }
    json_object_object_add(playback, "estimated_frame_count", json_object_new_int64((int64_t)result->estimated_frame_count));
    json_object_object_add(root, "playback", playback);

    json_object_object_add(output, "preview", json_object_new_boolean(result->job.output.preview_png));
    json_object_object_add(output, "preview_png", json_object_new_boolean(result->job.output.preview_png));
    json_object_object_add(output, "frames", json_object_new_boolean(result->job.output.frames));
    json_object_object_add(output, "video_manifest", json_object_new_boolean(result->job.output.video_manifest));
    json_object_object_add(output, "frame_format", json_object_new_string(result->job.output.frame_format));
    json_object_object_add(output, "render_mode",
                           json_object_new_string(map_forge_headless_render_mode_name(result->job.output.render_mode)));
    json_object_object_add(root, "output", output);

    return root;
}

static struct json_object *map_forge_headless_build_manifest_json(const MapForgeHeadlessRunResult *result) {
    struct json_object *root = json_object_new_object();
    struct json_object *artifacts = json_object_new_object();
    struct json_object *warnings = json_object_new_array();
    if (!result) {
        return root;
    }
    json_object_object_add(root, "version", json_object_new_int(1));
    json_object_object_add(root, "program", json_object_new_string("MapForge"));
    json_object_object_add(root, "product_name", json_object_new_string("Carta"));
    json_object_object_add(root, "mode", json_object_new_string("headless_route_job_foundation"));
    json_object_object_add(root, "status", json_object_new_string(result->status));
    json_object_object_add(root, "run_timestamp_utc", json_object_new_string(result->timestamp_utc));
    if (result->git_commit[0] != '\0') {
        json_object_object_add(root, "git_commit", json_object_new_string(result->git_commit));
    }
    json_object_object_add(root, "job_path", json_object_new_string(result->job_path));
    json_object_object_add(root, "output_path", json_object_new_string(result->out_dir));
    json_object_object_add(root, "map_region", json_object_new_string(result->job.map_region));
    if (result->from_pin.pin) {
        json_object_object_add(root, "from_pin", json_object_new_string(result->from_pin.pin->id));
    }
    if (result->to_pin.pin) {
        json_object_object_add(root, "to_pin", json_object_new_string(result->to_pin.pin->id));
    }
    if (result->route_computed) {
        json_object_object_add(root, "route_distance_m", json_object_new_double(result->route_state.path.total_length_m));
        json_object_object_add(root, "route_duration_s", json_object_new_double(result->route_state.path.total_time_s));
        json_object_object_add(root, "path_node_count", json_object_new_int64((int64_t)result->route_state.path.count));
    }
    json_object_object_add(root, "estimated_frame_count", json_object_new_int64((int64_t)result->estimated_frame_count));

    json_object_object_add(artifacts, "command", json_object_new_string("command.txt"));
    json_object_object_add(artifacts, "resolved_job", json_object_new_string("job.resolved.json"));
    json_object_object_add(artifacts, "summary", json_object_new_string("summary.md"));
    json_object_object_add(artifacts, "manifest", json_object_new_string("manifest.json"));
    if (result->playback_trace_written) {
        json_object_object_add(artifacts, "playback_trace", json_object_new_string("playback_trace.json"));
    }
    if (result->preview_written && result->image_exports.preview_artifact[0] != '\0') {
        json_object_object_add(artifacts, "preview", json_object_new_string(result->image_exports.preview_artifact));
    }
    if (result->image_exports.render_debug_written && result->image_exports.render_debug_artifact[0] != '\0') {
        json_object_object_add(artifacts, "render_debug", json_object_new_string(result->image_exports.render_debug_artifact));
    }
    if (result->frames_written && result->image_exports.frames_dir_artifact[0] != '\0') {
        json_object_object_add(artifacts, "frames_dir", json_object_new_string(result->image_exports.frames_dir_artifact));
    }
    json_object_object_add(root, "artifacts", artifacts);

    for (size_t i = 0; i < result->warnings.count; ++i) {
        json_object_array_add(warnings, json_object_new_string(result->warnings.items[i]));
    }
    json_object_object_add(root, "warnings", warnings);
    if (result->error[0] != '\0') {
        json_object_object_add(root, "error", json_object_new_string(result->error));
    }
    return root;
}

static bool map_forge_headless_write_summary(const MapForgeHeadlessRunResult *result) {
    char path[PATH_MAX];
    char text[8192];
    int n = 0;
    if (!result) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/summary.md", result->out_dir);
    n = snprintf(text,
                 sizeof(text),
                 "# Carta Headless Route Summary\n"
                 "Status: %s\n"
                 "Run Timestamp: %s\n"
                 "Region: %s\n"
                 "Route Mode: %s\n"
                 "From: %s (%s)\n"
                 "To: %s (%s)\n"
                 "Output:\n"
                 "- Command: `command.txt`\n"
                 "- Resolved Job: `job.resolved.json`\n"
                 "- Manifest: `manifest.json`\n"
                 "- Export Mode: headless BMP preview/frame export with region layers\n",
                 result->status,
                 result->timestamp_utc,
                 result->job.map_region,
                 result->job.route_mode == ROUTE_MODE_CAR ? "car" : "walking",
                 result->from_pin.pin ? result->from_pin.pin->name : "(missing)",
                 result->from_pin.pin ? result->from_pin.pin->id : "(missing)",
                 result->to_pin.pin ? result->to_pin.pin->name : "(missing)",
                 result->to_pin.pin ? result->to_pin.pin->id : "(missing)");
    if (n < 0 || (size_t)n >= sizeof(text)) {
        return false;
    }
    if (result->route_computed) {
        size_t used = (size_t)n;
        n = snprintf(text + used,
                     sizeof(text) - used,
                     "Route Metrics:\n"
                     "- Distance: %.2f km\n"
                     "- Estimated Travel Time: %.1f min\n"
                     "- Path Nodes: %u\n"
                     "- Estimated Frames From Job Settings: %u\n",
                     result->route_state.path.total_length_m / 1000.0f,
                     result->route_state.path.total_time_s / 60.0f,
                     result->route_state.path.count,
                     result->estimated_frame_count);
        if (n < 0 || (size_t)n >= sizeof(text) - used) {
            return false;
        }
    }
    if (result->playback_trace_written) {
        size_t used = strlen(text);
        n = snprintf(text + used,
                     sizeof(text) - used,
                     "- Playback Trace: `playback_trace.json`\n");
        if (n < 0 || (size_t)n >= sizeof(text) - used) {
            return false;
        }
    }
    if (result->preview_written && result->image_exports.preview_artifact[0] != '\0') {
        size_t used = strlen(text);
        n = snprintf(text + used,
                     sizeof(text) - used,
                     "- Preview Image: `%s`\n",
                     result->image_exports.preview_artifact);
        if (n < 0 || (size_t)n >= sizeof(text) - used) {
            return false;
        }
    }
    if (result->image_exports.render_debug_written && result->image_exports.render_debug_artifact[0] != '\0') {
        size_t used = strlen(text);
        n = snprintf(text + used,
                     sizeof(text) - used,
                     "- Render Debug: `%s`\n",
                     result->image_exports.render_debug_artifact);
        if (n < 0 || (size_t)n >= sizeof(text) - used) {
            return false;
        }
    }
    if (result->frames_written && result->image_exports.frames_dir_artifact[0] != '\0') {
        size_t used = strlen(text);
        n = snprintf(text + used,
                     sizeof(text) - used,
                     "- Frames: `%s`\n",
                     result->image_exports.frames_dir_artifact);
        if (n < 0 || (size_t)n >= sizeof(text) - used) {
            return false;
        }
    }
    if (result->warnings.count > 0u) {
        size_t used = strlen(text);
        n = snprintf(text + used, sizeof(text) - used, "Warnings:\n");
        if (n < 0 || (size_t)n >= sizeof(text) - used) {
            return false;
        }
        used += (size_t)n;
        for (size_t i = 0; i < result->warnings.count; ++i) {
            n = snprintf(text + used, sizeof(text) - used, "- %s\n", result->warnings.items[i]);
            if (n < 0 || (size_t)n >= sizeof(text) - used) {
                return false;
            }
            used += (size_t)n;
        }
    }
    if (result->error[0] != '\0') {
        size_t used = strlen(text);
        n = snprintf(text + used, sizeof(text) - used, "Error:\n- %s\n", result->error);
        if (n < 0 || (size_t)n >= sizeof(text) - used) {
            return false;
        }
    }
    return map_forge_headless_write_text_file(path, text);
}

static bool map_forge_headless_write_outputs(const MapForgeHeadlessRunResult *result) {
    char path[PATH_MAX];
    struct json_object *resolved_job = NULL;
    struct json_object *manifest = NULL;
    bool ok = true;
    if (!result) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/command.txt", result->out_dir);
    ok = ok && map_forge_headless_write_text_file(path, result->command);
    snprintf(path, sizeof(path), "%s/job.resolved.json", result->out_dir);
    resolved_job = map_forge_headless_build_resolved_job_json(result);
    ok = ok && map_forge_headless_write_json_file(path, resolved_job);
    if (result->playback_trace_written) {
        ok = ok && map_forge_headless_write_playback_trace(result);
    }
    snprintf(path, sizeof(path), "%s/manifest.json", result->out_dir);
    manifest = map_forge_headless_build_manifest_json(result);
    ok = ok && map_forge_headless_write_json_file(path, manifest);
    ok = ok && map_forge_headless_write_summary(result);
    if (resolved_job) {
        json_object_put(resolved_job);
    }
    if (manifest) {
        json_object_put(manifest);
    }
    return ok;
}

int map_forge_headless_run(const MapForgeHeadlessCliOptions *options,
                           int argc,
                           char **argv) {
    MapForgeHeadlessRunResult result;
    MapForgePinsFile pins_file;
    char error[256];
    char job_dir[PATH_MAX];
    const RegionInfo *catalog_region = NULL;
    const MapForgePin *from_pin = NULL;
    const MapForgePin *to_pin = NULL;

    if (!options || !options->job_path || !options->out_dir) {
        fprintf(stderr, "map_forge: headless run requires job and output paths\n");
        return 1;
    }

    memset(&result, 0, sizeof(result));
    map_forge_pins_file_init(&pins_file);
    route_state_init(&result.route_state);
    snprintf(result.status, sizeof(result.status), "failed");
    map_forge_headless_format_timestamp(result.timestamp_utc, sizeof(result.timestamp_utc));
    map_forge_headless_capture_git_commit(result.git_commit, sizeof(result.git_commit));
    map_forge_headless_format_command(argc, argv, result.command, sizeof(result.command));
    (void)map_forge_headless_realpath_or_copy(options->job_path, result.job_path, sizeof(result.job_path));
    (void)map_forge_headless_realpath_or_copy(options->out_dir, result.out_dir, sizeof(result.out_dir));

    if (!map_forge_headless_ensure_dir_recursive(result.out_dir)) {
        fprintf(stderr, "map_forge: failed to create output directory: %s\n", result.out_dir);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    if (!map_forge_headless_parent_dir(result.job_path, job_dir, sizeof(job_dir))) {
        (void)map_forge_headless_set_error(&result, "failed to resolve job directory");
        (void)map_forge_headless_write_outputs(&result);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    if (!map_forge_headless_job_load(result.job_path, &result.job, error, sizeof(error))) {
        (void)map_forge_headless_set_error(&result, error);
        (void)map_forge_headless_write_outputs(&result);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    if (!map_forge_headless_resolve_relative(job_dir,
                                             result.job.pins_file,
                                             result.pins_path,
                                             sizeof(result.pins_path))) {
        (void)map_forge_headless_set_error(&result, "failed to resolve pins file path");
        (void)map_forge_headless_write_outputs(&result);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    if (!map_forge_pins_load(result.pins_path, &pins_file, error, sizeof(error))) {
        (void)map_forge_headless_set_error(&result, error);
        (void)map_forge_headless_write_outputs(&result);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    if (pins_file.map_region[0] != '\0' && strcmp(pins_file.map_region, result.job.map_region) != 0) {
        (void)map_forge_headless_warning_add(&result.warnings,
                                             "pins.map_region does not match job.map_region; proceeding with the job region.");
    }

    catalog_region = map_forge_headless_find_region(result.job.map_region);
    if (!catalog_region) {
        (void)map_forge_headless_set_error(&result, "job.map_region was not found under MAPFORGE_REGIONS_DIR");
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    result.region = *catalog_region;
    if (!region_load_meta(catalog_region, &result.region)) {
        (void)map_forge_headless_warning_add(&result.warnings,
                                             "region meta.json could not be fully loaded; proceeding with catalog paths only.");
        result.region = *catalog_region;
    }
    if (!region_graph_path(&result.region, result.graph_path, sizeof(result.graph_path))) {
        (void)map_forge_headless_set_error(&result, "failed to resolve region graph path");
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    result.route_state.mode = result.job.route_mode;
    if (!route_state_load_graph(&result.route_state, result.graph_path)) {
        (void)map_forge_headless_set_error(&result, "failed to load route graph for region");
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    from_pin = map_forge_headless_find_pin(&pins_file, result.job.from_pin);
    to_pin = map_forge_headless_find_pin(&pins_file, result.job.to_pin);
    if (!from_pin || !to_pin) {
        (void)map_forge_headless_set_error(&result, "from_pin or to_pin could not be resolved from the pins file");
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    if (!map_forge_headless_resolve_pin(&result.route_state.graph, from_pin, &result.from_pin) ||
        !map_forge_headless_resolve_pin(&result.route_state.graph, to_pin, &result.to_pin)) {
        (void)map_forge_headless_set_error(&result, "failed to resolve pin coordinates to route graph nodes");
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    if (!route_state_route(&result.route_state, result.from_pin.nearest_node, result.to_pin.nearest_node)) {
        (void)map_forge_headless_set_error(&result, "route computation failed for the selected pins");
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    result.route_computed = true;
    {
        bool playback_ready = false;
        bool image_outputs_requested = result.job.output.preview_png || result.job.output.frames;
        MapForgeHeadlessPlaybackSample preview_sample;
        MapForgeHeadlessPlaybackSample *preview_sample_ptr = NULL;
        MapForgeHeadlessRenderPin render_from = {
            .valid = result.from_pin.pin != NULL,
            .nearest_node = result.from_pin.nearest_node,
            .world_x = result.from_pin.world_x,
            .world_y = result.from_pin.world_y
        };
        MapForgeHeadlessRenderPin render_to = {
            .valid = result.to_pin.pin != NULL,
            .nearest_node = result.to_pin.nearest_node,
            .world_x = result.to_pin.world_x,
            .world_y = result.to_pin.world_y
        };

        memset(&preview_sample, 0, sizeof(preview_sample));
        playback_ready = map_forge_headless_playback_plan(&result.job.playback,
                                                          &result.route_state.path,
                                                          &result.playback_duration_s,
                                                          &result.playback_fps,
                                                          &result.estimated_frame_count);
        if (playback_ready) {
            result.playback_trace_written = true;
            if (!map_forge_headless_build_playback_samples(&result)) {
                (void)map_forge_headless_set_error(&result, "failed to build deterministic playback samples");
                (void)map_forge_headless_write_outputs(&result);
                map_forge_pins_file_free(&pins_file);
                route_state_shutdown(&result.route_state);
                free(result.frame_samples);
                return 1;
            }
            preview_sample_ptr = &result.frame_samples[result.estimated_frame_count > 1u ? result.estimated_frame_count / 2u : 0u];
        } else if (result.job.output.preview_png) {
            if (!map_forge_headless_build_preview_sample(&result, &preview_sample)) {
                (void)map_forge_headless_set_error(&result, "failed to build a diagnostic preview sample");
                (void)map_forge_headless_write_outputs(&result);
                map_forge_pins_file_free(&pins_file);
                route_state_shutdown(&result.route_state);
                return 1;
            }
            preview_sample_ptr = &preview_sample;
        }

        if (image_outputs_requested) {
            if (!map_forge_headless_render_route_images(result.out_dir,
                                                        &result.job,
                                                        &result.region,
                                                        &render_from,
                                                        &render_to,
                                                        &result.route_state,
                                                        preview_sample_ptr,
                                                        playback_ready ? result.frame_samples : NULL,
                                                        playback_ready ? result.estimated_frame_count : 0u,
                                                        &result.image_exports)) {
                (void)map_forge_headless_set_error(&result, "failed to render headless route image exports");
                (void)map_forge_headless_write_outputs(&result);
                map_forge_pins_file_free(&pins_file);
                route_state_shutdown(&result.route_state);
                free(result.frame_samples);
                return 1;
            }
            result.preview_written = result.image_exports.preview_written;
            result.frames_written = result.image_exports.frames_written;
            result.frames_written_count = result.image_exports.frames_written_count;
        }
    }
    map_forge_headless_record_job_warnings(&result);
    snprintf(result.status, sizeof(result.status), "complete");
    result.ok = true;

    if (!map_forge_headless_write_outputs(&result)) {
        fprintf(stderr, "map_forge: failed to write one or more headless output files\n");
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    fprintf(stdout,
            "map_forge: headless run complete status=%s out=%s manifest=%s/manifest.json summary=%s/summary.md\n",
            result.status,
            result.out_dir,
            result.out_dir,
            result.out_dir);
    map_forge_pins_file_free(&pins_file);
    route_state_shutdown(&result.route_state);
    free(result.frame_samples);
    return 0;
}
