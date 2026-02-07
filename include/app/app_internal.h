#ifndef MAPFORGE_APP_INTERNAL_H
#define MAPFORGE_APP_INTERNAL_H

#include "camera/camera.h"
#include "core/input.h"
#include "ui/debug_overlay.h"
#include "app/region.h"
#include "map/layer_policy.h"
#include "map/tile_loader.h"
#include "map/tile_manager.h"
#include "render/renderer.h"
#include "render/vk_tile_cache.h"
#include "route/route.h"

#include <SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Header bar height in pixels. */
#define APP_HEADER_HEIGHT 34.0f
/* Default fallback tile integration budget per frame. */
#define APP_TILE_INTEGRATE_BUDGET 8u
/* Grace period before marking empty layers ready. */
#define APP_TILE_NO_DATA_TIMEOUT 1.5f
/* Max time slice spent integrating tile results each frame. */
#define APP_TILE_INTEGRATE_TIME_SLICE_SEC 0.0018
/* Max queued polygon prep jobs/results. */
#define APP_VK_POLY_PREP_QUEUE_CAPACITY 128u
/* Max prepared polygon results integrated per frame. */
#define APP_VK_POLY_PREP_INTEGRATE_BUDGET 2u
/* Max time slice spent integrating prepared polygon results each frame. */
#define APP_VK_POLY_PREP_INTEGRATE_TIME_SLICE_SEC 0.0010
/* Max queued Vulkan polygon asset build jobs. */
#define APP_VK_ASSET_QUEUE_CAPACITY 4096u
/* Default per-frame Vulkan polygon asset build jobs. */
#define APP_VK_ASSET_BUILD_BUDGET 2u
/* Max time slice spent building Vulkan polygon assets each frame. */
#define APP_VK_ASSET_BUILD_TIME_SLICE_SEC 0.0012

/* Per-tile queue entry sorted by distance from camera center tile. */
typedef struct TileQueueItem {
    TileCoord coord;
    uint32_t dist2;
} TileQueueItem;

/* Per-layer tile loading queue and cursor state. */
typedef struct TileQueue {
    TileQueueItem *items;
    uint32_t count;
    uint32_t index;
    uint32_t capacity;
} TileQueue;

/* Runtime polygon fill budget for Vulkan tile cache rendering. */
typedef struct VkPolyFillBudget {
    bool enabled;
    uint32_t total_cap;
    uint32_t total_used;
    uint32_t layer_cap[TILE_LAYER_COUNT];
    uint32_t layer_used[TILE_LAYER_COUNT];
} VkPolyFillBudget;

/* Runtime cap for on-demand Vulkan polygon asset generation. */
typedef struct VkPolyAssetBuildBudget {
    uint32_t cap;
    uint32_t used;
} VkPolyAssetBuildBudget;

/* Work item for deferred Vulkan polygon asset construction. */
typedef struct VkAssetJob {
    TileCoord coord;
    TileLayerKind kind;
    uint32_t request_id;
} VkAssetJob;

/* Thread-safe snapshot for polygon prep worker diagnostics. */
typedef struct VkPolyPrepStats {
    uint32_t in_count;
    uint32_t out_count;
    uint64_t enqueued_count;
    uint64_t done_count;
    uint64_t drop_count;
} VkPolyPrepStats;

/* Owns core application state for the main loop. */
typedef struct AppState {
    SDL_Window *window;
    Renderer renderer;
    Camera camera;
    InputState input;
    DebugOverlay overlay;
    TileManager tile_managers[TILE_LAYER_COUNT];
    TileLoader tile_loader;
    VkTileCache vk_tile_cache;
    bool single_line;
    RegionInfo region;
    int region_index;
    RouteState route;
    bool dragging_start;
    bool dragging_goal;
    bool has_hover;
    uint32_t hover_node;
    bool playback_playing;
    float playback_time_s;
    float playback_speed;
    bool show_landuse;
    float building_zoom_bias;
    bool building_fill_enabled;
    float road_zoom_bias;
    bool polygon_outline_only;
    uint32_t tile_request_id;
    TileQueue tile_queues[TILE_LAYER_COUNT];
    uint32_t layer_expected[TILE_LAYER_COUNT];
    uint32_t layer_done[TILE_LAYER_COUNT];
    uint32_t layer_inflight[TILE_LAYER_COUNT];
    uint32_t layer_visible_expected[TILE_LAYER_COUNT];
    uint32_t layer_visible_loaded[TILE_LAYER_COUNT];
    LayerReadinessState layer_state[TILE_LAYER_COUNT];
    TileCoord queue_top_left;
    TileCoord queue_bottom_right;
    uint16_t queue_zoom;
    bool queue_valid;
    TileCoord visible_top_left;
    TileCoord visible_bottom_right;
    uint16_t visible_zoom;
    bool visible_valid;
    uint32_t loading_expected;
    uint32_t loading_done;
    float loading_no_data_time;
    size_t loading_layer_index;
    uint32_t visible_tile_count;
    TileLayerKind active_layer_kind;
    uint32_t active_layer_expected;
    bool active_layer_valid;
    bool vk_assets_enabled;
    uint32_t vk_asset_misses;
    uint32_t vk_poly_fill_drawn;
    uint32_t vk_poly_fill_skip;
    uint32_t vk_poly_fill_fail;
    uint32_t vk_poly_fill_indices;
    bool vk_poly_prep_enabled;
    bool vk_poly_prep_running;
    pthread_t vk_poly_prep_thread;
    pthread_mutex_t vk_poly_prep_mutex;
    pthread_cond_t vk_poly_prep_cond;
    TileResult vk_poly_prep_in[APP_VK_POLY_PREP_QUEUE_CAPACITY];
    uint32_t vk_poly_prep_in_head;
    uint32_t vk_poly_prep_in_tail;
    uint32_t vk_poly_prep_in_count;
    TileResult vk_poly_prep_out[APP_VK_POLY_PREP_QUEUE_CAPACITY];
    uint32_t vk_poly_prep_out_head;
    uint32_t vk_poly_prep_out_tail;
    uint32_t vk_poly_prep_out_count;
    uint64_t vk_poly_prep_enqueued_count;
    uint64_t vk_poly_prep_done_count;
    uint64_t vk_poly_prep_drop_count;
    VkAssetJob vk_asset_jobs[APP_VK_ASSET_QUEUE_CAPACITY];
    uint32_t vk_asset_job_head;
    uint32_t vk_asset_job_tail;
    uint32_t vk_asset_job_count;
    uint64_t vk_asset_job_drop_count;
    uint64_t vk_asset_job_build_count;
    int width;
    int height;
} AppState;

float app_clampf(float value, float min_value, float max_value);
uint16_t app_zoom_to_tile_level(float zoom, const RegionInfo *region);
float app_building_zoom_bias_for_region(const RegionInfo *region);
float app_road_zoom_bias_for_region(const RegionInfo *region);
void app_center_camera_on_region(Camera *camera, const RegionInfo *region, int screen_w, int screen_h);

float app_layer_zoom_start(const AppState *app, TileLayerKind kind);
bool app_layer_runtime_enabled(const AppState *app, TileLayerKind kind);
bool app_layer_active_runtime(const AppState *app, TileLayerKind kind);
void app_update_vk_line_budget(AppState *app);
uint32_t app_tile_load_budget(TileLayerKind kind, uint32_t expected);
uint32_t app_tile_integrate_budget(TileLayerKind kind, uint32_t expected);
void app_clear_tile_queue(AppState *app);
void app_drain_tile_results(AppState *app, uint32_t budget);
void app_refresh_layer_states(AppState *app);
void app_update_tile_queue(AppState *app);
bool app_vk_poly_prep_init(AppState *app);
void app_vk_poly_prep_shutdown(AppState *app);
void app_vk_poly_prep_clear(AppState *app);
bool app_vk_poly_prep_enqueue(AppState *app, const TileResult *result);
void app_vk_poly_prep_drain(AppState *app, uint32_t max_results, double max_time_slice_sec);
void app_vk_poly_prep_get_stats(AppState *app, VkPolyPrepStats *out_stats);
void app_vk_asset_queue_clear(AppState *app);
bool app_vk_asset_enqueue(AppState *app, TileLayerKind kind, TileCoord coord);
void app_process_vk_asset_queue(AppState *app, uint32_t max_jobs, double max_time_slice_sec);

uint32_t app_draw_visible_tiles(AppState *app);
void app_draw_region_bounds(AppState *app);

bool app_load_route_graph(AppState *app);
bool app_is_near_node(const AppState *app, float world_x, float world_y, uint32_t *out_node);
bool app_mouse_over_node(const AppState *app, uint32_t node, float radius);
void app_update_hover(AppState *app);
void app_draw_hover_marker(AppState *app);
bool app_recompute_route(AppState *app);

void app_playback_reset(AppState *app);
void app_playback_update(AppState *app, float dt);
float app_next_playback_speed(float current, int direction);
void app_draw_playback_marker(AppState *app);
void app_draw_route_panel(AppState *app);

bool app_header_button_hit(const AppState *app, int x, int y);
void app_draw_header_bar(AppState *app);
void app_draw_layer_debug(AppState *app);
void app_copy_overlay_text(AppState *app);

#endif
