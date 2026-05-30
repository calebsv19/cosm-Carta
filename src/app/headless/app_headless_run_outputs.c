#include "app_headless_run_internal.h"

#include "core_headless_job.h"
#include "core_io.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *map_forge_headless_shared_report_state_name(const char *status) {
    if (!status || status[0] == '\0') {
        return "failed";
    }
    if (strcmp(status, "complete") == 0) {
        return "succeeded";
    }
    return status;
}

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

static const char *map_forge_headless_quality_profile_name(MapForgeHeadlessQualityProfile profile) {
    switch (profile) {
        case MAPFORGE_HEADLESS_QUALITY_PROFILE_FINAL:
            return "final";
        case MAPFORGE_HEADLESS_QUALITY_PROFILE_RUNTIME:
        default:
            return "runtime";
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

static bool map_forge_headless_write_text_file(const char *path, const char *text) {
    CoreResult write_result;
    if (!path || !text) {
        return false;
    }
    write_result = core_io_write_all(path, text, strlen(text));
    return write_result.code == CORE_OK;
}

static bool map_forge_headless_copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0u || !src) {
        return false;
    }
    if (snprintf(dst, dst_size, "%s", src) >= (int)dst_size) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

void map_forge_headless_record_job_warnings(MapForgeHeadlessRunResult *result) {
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
    json_object_object_add(output, "pixel_scale", json_object_new_int(result->job.output.pixel_scale));
    json_object_object_add(output, "quality_profile",
                           json_object_new_string(map_forge_headless_quality_profile_name(result->job.output.quality_profile)));
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

static bool map_forge_headless_build_shared_job_envelope(const MapForgeHeadlessRunResult *result,
                                                         CoreHeadlessJobEnvelope *out_envelope) {
    if (!result || !out_envelope) {
        return false;
    }
    if (result->source_envelope.job_id[0] != '\0') {
        *out_envelope = result->source_envelope;
    } else {
        core_headless_job_envelope_init(out_envelope);
        (void)map_forge_headless_copy_string(out_envelope->job_id,
                                             sizeof(out_envelope->job_id),
                                             result->job.type[0] != '\0' ? result->job.type : "mapforge-headless-run");
        (void)map_forge_headless_copy_string(out_envelope->program,
                                             sizeof(out_envelope->program),
                                             "map_forge");
        (void)map_forge_headless_copy_string(out_envelope->tool.name,
                                             sizeof(out_envelope->tool.name),
                                             "map_forge");
        (void)map_forge_headless_copy_string(out_envelope->tool.version,
                                             sizeof(out_envelope->tool.version),
                                             result->git_commit[0] != '\0' ? result->git_commit : "workspace");
        (void)map_forge_headless_copy_string(out_envelope->tool.target_os,
                                             sizeof(out_envelope->tool.target_os),
                                             "macOS");
        (void)map_forge_headless_copy_string(out_envelope->tool.target_arch,
                                             sizeof(out_envelope->tool.target_arch),
                                             "arm64");
        (void)map_forge_headless_copy_string(out_envelope->scene_payload.schema_family,
                                             sizeof(out_envelope->scene_payload.schema_family),
                                             "mapforge_headless_job");
        (void)map_forge_headless_copy_string(out_envelope->scene_payload.schema_variant,
                                             sizeof(out_envelope->scene_payload.schema_variant),
                                             "route_job_v1");
        (void)map_forge_headless_copy_string(out_envelope->scene_payload.path,
                                             sizeof(out_envelope->scene_payload.path),
                                             "job.request.json");
        (void)map_forge_headless_copy_string(out_envelope->run_config.schema_family,
                                             sizeof(out_envelope->run_config.schema_family),
                                             "mapforge_headless_run");
        (void)map_forge_headless_copy_string(out_envelope->run_config.schema_variant,
                                             sizeof(out_envelope->run_config.schema_variant),
                                             "runtime_v1");
        (void)map_forge_headless_copy_string(out_envelope->run_config.path,
                                             sizeof(out_envelope->run_config.path),
                                             "job.request.json");
        (void)map_forge_headless_copy_string(out_envelope->metadata.created_by,
                                             sizeof(out_envelope->metadata.created_by),
                                             "codex");
        (void)map_forge_headless_copy_string(out_envelope->metadata.created_at,
                                             sizeof(out_envelope->metadata.created_at),
                                             result->timestamp_utc);
    }
    (void)map_forge_headless_copy_string(out_envelope->outputs.root,
                                         sizeof(out_envelope->outputs.root),
                                         result->run_root);
    (void)map_forge_headless_copy_string(out_envelope->outputs.report_path,
                                         sizeof(out_envelope->outputs.report_path),
                                         "output/report.json");
    (void)map_forge_headless_copy_string(out_envelope->outputs.logs_dir,
                                         sizeof(out_envelope->outputs.logs_dir),
                                         "output/logs");
    (void)map_forge_headless_copy_string(out_envelope->outputs.artifacts_dir,
                                         sizeof(out_envelope->outputs.artifacts_dir),
                                         "output/artifacts");
    return core_headless_job_envelope_validate(out_envelope);
}

static bool map_forge_headless_write_shared_report(const MapForgeHeadlessRunResult *result) {
    CoreHeadlessJobReport report;
    CoreHeadlessJobArtifact artifacts[5];
    size_t artifact_count = 0u;
    char bundle_diag[256];

    if (!result || result->shared_report_path[0] == '\0') {
        return false;
    }

    core_headless_job_report_init(&report);
    (void)map_forge_headless_copy_string(report.job_id,
                                         sizeof(report.job_id),
                                         result->source_envelope.job_id[0] != '\0' ? result->source_envelope.job_id : result->job.type);
    (void)map_forge_headless_copy_string(report.program, sizeof(report.program), "map_forge");
    (void)map_forge_headless_copy_string(report.state,
                                         sizeof(report.state),
                                         map_forge_headless_shared_report_state_name(result->status));
    (void)map_forge_headless_copy_string(report.stage, sizeof(report.stage), "route_headless_run");
    (void)map_forge_headless_copy_string(report.created_at, sizeof(report.created_at), result->timestamp_utc);
    (void)map_forge_headless_copy_string(report.started_at, sizeof(report.started_at), result->timestamp_utc);
    (void)map_forge_headless_copy_string(report.updated_at, sizeof(report.updated_at), result->timestamp_utc);
    (void)map_forge_headless_copy_string(report.finished_at, sizeof(report.finished_at), result->timestamp_utc);

    core_headless_job_artifact_init(&artifacts[artifact_count]);
    (void)map_forge_headless_copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "manifest");
    (void)map_forge_headless_copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), "output/artifacts/manifest.json");
    artifact_count += 1u;

    if (result->preview_written && result->image_exports.preview_artifact[0] != '\0' && artifact_count < 5u) {
        core_headless_job_artifact_init(&artifacts[artifact_count]);
        (void)map_forge_headless_copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "preview");
        (void)snprintf(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), "output/artifacts/%s", result->image_exports.preview_artifact);
        artifact_count += 1u;
    }
    if (result->frames_written && result->image_exports.frames_dir_artifact[0] != '\0' && artifact_count < 5u) {
        core_headless_job_artifact_init(&artifacts[artifact_count]);
        (void)map_forge_headless_copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "frames");
        (void)snprintf(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), "output/artifacts/%s", result->image_exports.frames_dir_artifact);
        artifact_count += 1u;
    }
    if (result->playback_trace_written && artifact_count < 5u) {
        core_headless_job_artifact_init(&artifacts[artifact_count]);
        (void)map_forge_headless_copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "playback_trace");
        (void)map_forge_headless_copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), "output/artifacts/playback_trace.json");
        artifact_count += 1u;
    }

    report.artifacts = artifacts;
    report.artifact_count = artifact_count;
    return map_forge_headless_job_report_write(result->shared_report_path,
                                               &report,
                                               artifacts,
                                               artifact_count,
                                               bundle_diag,
                                               sizeof(bundle_diag));
}

bool map_forge_headless_write_outputs(const MapForgeHeadlessRunResult *result) {
    char path[PATH_MAX];
    struct json_object *resolved_job = NULL;
    struct json_object *manifest = NULL;
    CoreHeadlessJobEnvelope shared_job;
    char bundle_diag[256];
    bool ok = true;
    if (!result) {
        return false;
    }
    if (result->job_loaded && result->canonical_job_request_path[0] != '\0') {
        ok = ok && map_forge_headless_job_write(result->canonical_job_request_path,
                                                &result->job,
                                                bundle_diag,
                                                sizeof(bundle_diag));
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
    if (result->job_loaded &&
        result->shared_job_path[0] != '\0' &&
        map_forge_headless_build_shared_job_envelope(result, &shared_job)) {
        ok = ok && map_forge_headless_job_bundle_write(result->shared_job_path,
                                                       &shared_job,
                                                       bundle_diag,
                                                       sizeof(bundle_diag));
    }
    if (result->shared_report_path[0] != '\0') {
        ok = ok && map_forge_headless_write_shared_report(result);
    }
    return ok;
}
