#include "app/app_headless.h"
#include "app/app_headless_job_bundle.h"
#include "app_headless_run_internal.h"
#include "app_headless_util.h"

#include "app/region.h"
#include "app/region_loader.h"
#include "map/mercator.h"
#include "route/route.h"

#include <json-c/json.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool map_forge_headless_set_error(MapForgeHeadlessRunResult *result, const char *message) {
    if (result && message) {
        snprintf(result->error, sizeof(result->error), "%s", message);
        snprintf(result->status, sizeof(result->status), "failed");
    }
    return false;
}

static bool map_forge_headless_set_diagnostic_error(MapForgeHeadlessRunResult *result,
                                                    const char *stage,
                                                    const char *code,
                                                    const char *context,
                                                    const char *message) {
    if (result) {
        snprintf(result->failure_stage,
                 sizeof(result->failure_stage),
                 "%s",
                 stage ? stage : "");
        snprintf(result->failure_code,
                 sizeof(result->failure_code),
                 "%s",
                 code ? code : "");
        snprintf(result->failure_context,
                 sizeof(result->failure_context),
                 "%s",
                 context ? context : "");
    }
    return map_forge_headless_set_error(result, message);
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
                                                      const char *key,
                                                      char *out_error,
                                                      size_t out_error_size) {
    const MapForgePin *pin = NULL;
    const MapForgePin *matched_by_name = NULL;
    size_t name_matches = 0u;
    if (!pins_file || !key) {
        return NULL;
    }
    pin = map_forge_pins_find_by_id_const(pins_file, key);
    if (pin) {
        return pin;
    }
    for (size_t i = 0; i < pins_file->pin_count; ++i) {
        if (strcmp(pins_file->pins[i].name, key) != 0) {
            continue;
        }
        matched_by_name = &pins_file->pins[i];
        name_matches += 1u;
        if (name_matches > 1u) {
            break;
        }
    }
    if (name_matches == 1u) {
        return matched_by_name;
    }
    if (name_matches > 1u && out_error && out_error_size > 0u) {
        snprintf(out_error,
                 out_error_size,
                 "pin reference '%s' matched multiple pin names; use a stable pin id or an explicit pins_file",
                 key);
    }
    return NULL;
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

int map_forge_headless_run(const MapForgeHeadlessCliOptions *options,
                           int argc,
                           char **argv) {
    MapForgeHeadlessRunResult result;
    MapForgeHeadlessJobBundle source_bundle;
    MapForgePinsFile pins_file;
    char error[256];
    char job_dir[PATH_MAX];
    char effective_job_path[PATH_MAX];
    const RegionInfo *catalog_region = NULL;
    const MapForgePin *from_pin = NULL;
    const MapForgePin *to_pin = NULL;
    bool loaded_pins_file = false;
    bool loaded_legacy_pins = false;
    bool is_shared_bundle = false;

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
    memset(&result.source_envelope, 0, sizeof(result.source_envelope));
    memset(&source_bundle, 0, sizeof(source_bundle));
    (void)map_forge_headless_realpath_or_copy(options->out_dir, result.run_root, sizeof(result.run_root));
    (void)map_forge_headless_copy_string(result.out_dir, sizeof(result.out_dir), result.run_root);
    snprintf(result.canonical_job_request_path,
             sizeof(result.canonical_job_request_path),
             "%s/job.request.json",
             result.run_root);
    snprintf(result.shared_job_path,
             sizeof(result.shared_job_path),
             "%s/job.json",
             result.run_root);
    snprintf(result.shared_report_path,
             sizeof(result.shared_report_path),
             "%s/output/report.json",
             result.run_root);

    if (!map_forge_headless_ensure_dir_recursive_mode(result.run_root, 0755)) {
        fprintf(stderr, "map_forge: failed to create output directory: %s\n", result.run_root);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    if (!map_forge_headless_job_load_for_run(options->job_path,
                                             &result.job,
                                             &source_bundle,
                                             &is_shared_bundle,
                                             error,
                                             sizeof(error))) {
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "job_load",
                                                      "job_load_failed",
                                                      options->job_path,
                                                      error);
        (void)map_forge_headless_write_outputs(&result);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    result.job_loaded = true;
    result.shared_bundle = is_shared_bundle;
    if (is_shared_bundle) {
        result.source_envelope = source_bundle.envelope;
        (void)map_forge_headless_copy_string(result.job_path, sizeof(result.job_path), source_bundle.resolved_scene_payload_path);
        if (snprintf(result.out_dir,
                     sizeof(result.out_dir),
                     "%s/output/artifacts",
                     result.run_root) >= (int)sizeof(result.out_dir)) {
            (void)map_forge_headless_set_diagnostic_error(&result,
                                                          "output_setup",
                                                          "bundle_artifacts_path_overflow",
                                                          result.run_root,
                                                          "failed to derive bundle artifacts directory");
            (void)map_forge_headless_write_outputs(&result);
            route_state_shutdown(&result.route_state);
            return 1;
        }
    } else {
        (void)map_forge_headless_realpath_or_copy(options->job_path, result.job_path, sizeof(result.job_path));
    }
    if (!map_forge_headless_ensure_dir_recursive_mode(result.out_dir, 0755)) {
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "output_setup",
                                                      "artifact_output_dir_create_failed",
                                                      result.out_dir,
                                                      "failed to create artifact output directory");
        (void)map_forge_headless_write_outputs(&result);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    (void)map_forge_headless_copy_string(effective_job_path, sizeof(effective_job_path), result.job_path);
    if (!map_forge_headless_parent_dir(effective_job_path, job_dir, sizeof(job_dir))) {
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "job_load",
                                                      "job_directory_resolve_failed",
                                                      effective_job_path,
                                                      "failed to resolve job directory");
        (void)map_forge_headless_write_outputs(&result);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    catalog_region = map_forge_headless_find_region(result.job.map_region);
    if (!catalog_region) {
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "region_lookup",
                                                      "map_region_not_found",
                                                      result.job.map_region,
                                                      "job.map_region was not found under MAPFORGE_REGIONS_DIR");
        (void)map_forge_headless_write_outputs(&result);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    if (result.job.pins_file[0] != '\0') {
        if (!map_forge_headless_resolve_relative(job_dir,
                                                 result.job.pins_file,
                                                 result.pins_path,
                                                 sizeof(result.pins_path))) {
            (void)map_forge_headless_set_diagnostic_error(&result,
                                                          "pins_load",
                                                          "pins_file_path_resolve_failed",
                                                          result.job.pins_file,
                                                          "failed to resolve pins file path");
            (void)map_forge_headless_write_outputs(&result);
            route_state_shutdown(&result.route_state);
            return 1;
        }
        if (!map_forge_pins_load(result.pins_path, &pins_file, error, sizeof(error))) {
            (void)map_forge_headless_set_diagnostic_error(&result,
                                                          "pins_load",
                                                          "pins_file_load_failed",
                                                          result.pins_path,
                                                          error);
            (void)map_forge_headless_write_outputs(&result);
            route_state_shutdown(&result.route_state);
            return 1;
        }
        loaded_pins_file = true;
    } else {
        if (!map_forge_pins_load_preferred_region_file(catalog_region,
                                                       &pins_file,
                                                       result.pins_path,
                                                       sizeof(result.pins_path),
                                                       &loaded_pins_file,
                                                       &loaded_legacy_pins,
                                                       error,
                                                       sizeof(error))) {
            (void)map_forge_headless_set_diagnostic_error(&result,
                                                          "pins_load",
                                                          "region_pins_load_failed",
                                                          result.job.map_region,
                                                          error);
            (void)map_forge_headless_write_outputs(&result);
            route_state_shutdown(&result.route_state);
            return 1;
        }
        if (!loaded_pins_file) {
            char message[512];
            snprintf(message,
                     sizeof(message),
                     "job did not specify pins_file and no saved pins file exists for region '%s' at %s",
                     result.job.map_region,
                     result.pins_path[0] != '\0' ? result.pins_path : "(unresolved)");
            (void)map_forge_headless_set_diagnostic_error(&result,
                                                          "pins_load",
                                                          "region_pins_missing",
                                                          result.pins_path[0] != '\0' ? result.pins_path : result.job.map_region,
                                                          message);
            (void)map_forge_headless_write_outputs(&result);
            map_forge_pins_file_free(&pins_file);
            route_state_shutdown(&result.route_state);
            return 1;
        }
        (void)map_forge_headless_warning_add(&result.warnings,
                                             "job.pins_file was omitted; headless run resolved pins from the region-default local/private pin store.");
        if (loaded_legacy_pins) {
            (void)map_forge_headless_warning_add(&result.warnings,
                                                 "loaded pins from the legacy dev-local path because the runtime/default pin store was missing.");
        }
    }
    if (pins_file.map_region[0] != '\0' && strcmp(pins_file.map_region, result.job.map_region) != 0) {
        (void)map_forge_headless_warning_add(&result.warnings,
                                             "pins.map_region does not match job.map_region; proceeding with the job region.");
    }
    result.region = *catalog_region;
    if (!region_load_meta(catalog_region, &result.region)) {
        (void)map_forge_headless_warning_add(&result.warnings,
                                             "region meta.json could not be fully loaded; proceeding with catalog paths only.");
        result.region = *catalog_region;
    }
    if (!region_graph_path(&result.region, result.graph_path, sizeof(result.graph_path))) {
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "graph_load",
                                                      "graph_path_resolve_failed",
                                                      result.job.map_region,
                                                      "failed to resolve region graph path");
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    result.route_state.mode = result.job.route_mode;
    if (!route_state_load_graph(&result.route_state, result.graph_path)) {
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "graph_load",
                                                      "graph_load_failed",
                                                      result.graph_path,
                                                      "failed to load route graph for region");
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    error[0] = '\0';
    from_pin = map_forge_headless_find_pin(&pins_file, result.job.from_pin, error, sizeof(error));
    if (!from_pin && error[0] != '\0') {
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "pin_lookup",
                                                      "from_pin_ambiguous",
                                                      result.job.from_pin,
                                                      error);
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    error[0] = '\0';
    to_pin = map_forge_headless_find_pin(&pins_file, result.job.to_pin, error, sizeof(error));
    if (!to_pin && error[0] != '\0') {
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "pin_lookup",
                                                      "to_pin_ambiguous",
                                                      result.job.to_pin,
                                                      error);
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    if (!from_pin || !to_pin) {
        char context[256];
        snprintf(context,
                 sizeof(context),
                 "from_pin=%s to_pin=%s pins=%s",
                 result.job.from_pin,
                 result.job.to_pin,
                 result.pins_path[0] != '\0' ? result.pins_path : "(unresolved)");
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "pin_lookup",
                                                      !from_pin ? "from_pin_not_found" : "to_pin_not_found",
                                                      context,
                                                      "from_pin or to_pin could not be resolved from the selected pins source");
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }
    if (!map_forge_headless_resolve_pin(&result.route_state.graph, from_pin, &result.from_pin) ||
        !map_forge_headless_resolve_pin(&result.route_state.graph, to_pin, &result.to_pin)) {
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "pin_lookup",
                                                      "pin_node_resolve_failed",
                                                      result.graph_path,
                                                      "failed to resolve pin coordinates to route graph nodes");
        (void)map_forge_headless_write_outputs(&result);
        map_forge_pins_file_free(&pins_file);
        route_state_shutdown(&result.route_state);
        return 1;
    }

    if (!route_state_route(&result.route_state, result.from_pin.nearest_node, result.to_pin.nearest_node)) {
        (void)map_forge_headless_set_diagnostic_error(&result,
                                                      "route_compute",
                                                      "route_compute_failed",
                                                      result.job.map_region,
                                                      "route computation failed for the selected pins");
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
                (void)map_forge_headless_set_diagnostic_error(&result,
                                                              "playback",
                                                              "playback_samples_failed",
                                                              result.job.map_region,
                                                              "failed to build deterministic playback samples");
                (void)map_forge_headless_write_outputs(&result);
                map_forge_pins_file_free(&pins_file);
                route_state_shutdown(&result.route_state);
                free(result.frame_samples);
                return 1;
            }
            preview_sample_ptr = &result.frame_samples[result.estimated_frame_count > 1u ? result.estimated_frame_count / 2u : 0u];
        } else if (result.job.output.preview_png) {
            if (!map_forge_headless_build_preview_sample(&result, &preview_sample)) {
                (void)map_forge_headless_set_diagnostic_error(&result,
                                                              "playback",
                                                              "preview_sample_failed",
                                                              result.job.map_region,
                                                              "failed to build a diagnostic preview sample");
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
                (void)map_forge_headless_set_diagnostic_error(&result,
                                                              "render_export",
                                                              "render_export_failed",
                                                              result.out_dir,
                                                              "failed to render headless route image exports");
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
