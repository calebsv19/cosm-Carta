#ifndef MAPFORGE_APP_APP_HEADLESS_H
#define MAPFORGE_APP_APP_HEADLESS_H

#include "app/app_pins.h"
#include "app/region.h"
#include "route/route.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAPFORGE_HEADLESS_PATH_CAPACITY 512u
#define MAPFORGE_HEADLESS_ID_CAPACITY 128u
#define MAPFORGE_HEADLESS_NAME_CAPACITY 128u
#define MAPFORGE_HEADLESS_TEXT_CAPACITY 256u
#define MAPFORGE_HEADLESS_WARNING_CAPACITY 16u
#define MAPFORGE_HEADLESS_WARNING_TEXT_CAPACITY 256u

typedef struct MapForgeHeadlessCliOptions {
    bool headless;
    bool show_help;
    const char *job_path;
    const char *out_dir;
} MapForgeHeadlessCliOptions;

typedef struct MapForgeHeadlessCameraConfig {
    int width;
    int height;
    float zoom;
    bool follow_route;
    bool rotate_with_heading;
    bool has_width;
    bool has_height;
    bool has_zoom;
} MapForgeHeadlessCameraConfig;

typedef enum MapForgeHeadlessHeadingMode {
    MAPFORGE_HEADLESS_HEADING_MODE_BLENDED = 0,
    MAPFORGE_HEADLESS_HEADING_MODE_LOOKAHEAD = 1,
    MAPFORGE_HEADLESS_HEADING_MODE_PATH_TANGENT = 2
} MapForgeHeadlessHeadingMode;

typedef struct MapForgeHeadlessPlaybackHeadingConfig {
    MapForgeHeadlessHeadingMode mode;
    float smoothing_tau_seconds;
    float lookahead_seconds;
    float measurement_window_seconds;
    float max_turn_rate_deg_per_sec;
    bool has_smoothing_tau_seconds;
    bool has_lookahead_seconds;
    bool has_measurement_window_seconds;
    bool has_max_turn_rate_deg_per_sec;
} MapForgeHeadlessPlaybackHeadingConfig;

typedef struct MapForgeHeadlessPlaybackConfig {
    float duration_seconds;
    int fps;
    bool start_paused;
    bool has_duration_seconds;
    bool has_fps;
    MapForgeHeadlessPlaybackHeadingConfig heading;
} MapForgeHeadlessPlaybackConfig;

typedef enum MapForgeHeadlessRenderMode {
    MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE_MARKER = 0,
    MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE = 1,
    MAPFORGE_HEADLESS_RENDER_MODE_MAP_ONLY = 2
} MapForgeHeadlessRenderMode;

typedef enum MapForgeHeadlessQualityProfile {
    MAPFORGE_HEADLESS_QUALITY_PROFILE_RUNTIME = 0,
    MAPFORGE_HEADLESS_QUALITY_PROFILE_FINAL = 1
} MapForgeHeadlessQualityProfile;

typedef struct MapForgeHeadlessOutputConfig {
    bool preview_png;
    bool frames;
    bool video_manifest;
    char frame_format[32];
    MapForgeHeadlessRenderMode render_mode;
    int pixel_scale;
    bool stabilize_visible_zoom;
    bool stabilize_tile_bands;
    bool allow_tile_fallback;
    bool simplify_route_screen_space;
    MapForgeHeadlessQualityProfile quality_profile;
    bool has_pixel_scale;
} MapForgeHeadlessOutputConfig;

typedef struct MapForgeHeadlessPlaybackHeadingState {
    bool valid;
    float heading_rad;
    float route_time_s;
} MapForgeHeadlessPlaybackHeadingState;

typedef struct MapForgeHeadlessPlaybackSample {
    bool valid;
    uint32_t segment_index;
    float progress;
    float route_time_s;
    double world_x;
    double world_y;
    bool has_lookahead;
    double lookahead_world_x;
    double lookahead_world_y;
    float heading_rad;
} MapForgeHeadlessPlaybackSample;

typedef struct MapForgeHeadlessRenderPin {
    bool valid;
    uint32_t nearest_node;
    double world_x;
    double world_y;
} MapForgeHeadlessRenderPin;

typedef struct MapForgeHeadlessImageExportResult {
    bool preview_written;
    bool frames_written;
    bool render_debug_written;
    uint32_t frames_written_count;
    char preview_artifact[64];
    char frames_dir_artifact[64];
    char render_debug_artifact[64];
} MapForgeHeadlessImageExportResult;

typedef struct MapForgeHeadlessJob {
    uint32_t version;
    char type[64];
    char map_region[MAPFORGE_HEADLESS_NAME_CAPACITY];
    char map_data[MAPFORGE_HEADLESS_PATH_CAPACITY];
    char pins_file[MAPFORGE_HEADLESS_PATH_CAPACITY];
    char from_pin[MAPFORGE_HEADLESS_ID_CAPACITY];
    char to_pin[MAPFORGE_HEADLESS_ID_CAPACITY];
    RouteTravelMode route_mode;
    MapForgeHeadlessCameraConfig camera;
    MapForgeHeadlessPlaybackConfig playback;
    MapForgeHeadlessOutputConfig output;
} MapForgeHeadlessJob;

typedef struct MapForgeHeadlessWarningSet {
    size_t count;
    char items[MAPFORGE_HEADLESS_WARNING_CAPACITY][MAPFORGE_HEADLESS_WARNING_TEXT_CAPACITY];
} MapForgeHeadlessWarningSet;

bool map_forge_headless_args_parse(int argc,
                                   char **argv,
                                   MapForgeHeadlessCliOptions *out_options,
                                   char *out_error,
                                   size_t out_error_size);
void map_forge_headless_args_usage(const char *program_name,
                                   char *out_text,
                                   size_t out_text_size);

bool map_forge_headless_job_load(const char *job_path,
                                 MapForgeHeadlessJob *out_job,
                                 char *out_error,
                                 size_t out_error_size);
bool map_forge_headless_job_write(const char *job_path,
                                  const MapForgeHeadlessJob *job,
                                  char *out_error,
                                  size_t out_error_size);
void map_forge_headless_playback_reset_heading_state(MapForgeHeadlessPlaybackHeadingState *state);
bool map_forge_headless_playback_plan(const MapForgeHeadlessPlaybackConfig *config,
                                      const RoutePath *path,
                                      float *out_playback_duration_s,
                                      int *out_fps,
                                      uint32_t *out_frame_count);
bool map_forge_headless_playback_sample(const RouteGraph *graph,
                                        const RoutePath *path,
                                        const MapForgeHeadlessPlaybackConfig *config,
                                        float route_time_s,
                                        MapForgeHeadlessPlaybackHeadingState *io_heading_state,
                                        MapForgeHeadlessPlaybackSample *out_sample);
bool map_forge_headless_render_route_images(const char *out_dir,
                                            const MapForgeHeadlessJob *job,
                                            const RegionInfo *region,
                                            const MapForgeHeadlessRenderPin *from_pin,
                                            const MapForgeHeadlessRenderPin *to_pin,
                                            const RouteState *route_state,
                                            const MapForgeHeadlessPlaybackSample *preview_sample,
                                            const MapForgeHeadlessPlaybackSample *frame_samples,
                                            uint32_t frame_count,
                                            MapForgeHeadlessImageExportResult *out_result);

int map_forge_headless_run(const MapForgeHeadlessCliOptions *options,
                           int argc,
                           char **argv);

#endif
