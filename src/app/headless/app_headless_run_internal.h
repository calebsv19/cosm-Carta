#ifndef MAPFORGE_APP_HEADLESS_RUN_INTERNAL_H
#define MAPFORGE_APP_HEADLESS_RUN_INTERNAL_H

#include "app/app_headless.h"
#include "app/app_headless_job_bundle.h"
#include "app/region.h"
#include "route/route.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct MapForgeHeadlessResolvedPin {
    const MapForgePin *pin;
    uint32_t nearest_node;
    double world_x;
    double world_y;
    double node_distance_m;
} MapForgeHeadlessResolvedPin;

typedef struct MapForgeHeadlessRunResult {
    bool ok;
    bool shared_bundle;
    bool job_loaded;
    bool route_computed;
    bool playback_trace_written;
    bool preview_written;
    bool frames_written;
    char status[32];
    char error[256];
    char job_path[PATH_MAX];
    char run_root[PATH_MAX];
    char out_dir[PATH_MAX];
    char canonical_job_request_path[PATH_MAX];
    char shared_job_path[PATH_MAX];
    char shared_report_path[PATH_MAX];
    char pins_path[PATH_MAX];
    char graph_path[PATH_MAX];
    char command[2048];
    char timestamp_utc[64];
    char git_commit[64];
    CoreHeadlessJobEnvelope source_envelope;
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

void map_forge_headless_record_job_warnings(MapForgeHeadlessRunResult *result);
bool map_forge_headless_write_outputs(const MapForgeHeadlessRunResult *result);

#endif
