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
#include "core_trace.h"
#include "core_queue.h"

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
#define APP_VK_ASSET_BUILD_BUDGET 6u
/* Max time slice spent building Vulkan polygon assets each frame. */
#define APP_VK_ASSET_BUILD_TIME_SLICE_SEC 0.0012
/* Max queued prepared Vulkan asset jobs waiting for GPU submission. */
#define APP_VK_ASSET_READY_QUEUE_CAPACITY 1024u
/* Route recompute debounce while dragging endpoints. */
#define APP_ROUTE_DRAG_DEBOUNCE_SEC 0.045
/* Runtime trace sample/marker ring capacities. */
#define APP_TRACE_SAMPLE_CAPACITY 262144u
#define APP_TRACE_MARKER_CAPACITY 4096u
#define APP_HUD_ROUTE_LINE_CAPACITY 192u

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
    TileZoomBand band;
    uint32_t request_id;
} VkAssetJob;

/* Prepared Vulkan asset work item ready for main-thread GPU submission. */
typedef struct VkAssetReadyJob {
    TileCoord coord;
    TileLayerKind kind;
    TileZoomBand band;
    uint32_t request_id;
} VkAssetReadyJob;

/* Per-frame timing buckets for runtime phase instrumentation. */
typedef struct FramePhaseTimings {
    double frame_ms;
    double events_ms;
    double update_ms;
    double route_ms;
    double queue_ms;
    double integrate_ms;
    double render_ms;
    double present_ms;
} FramePhaseTimings;

/* Endpoint anchor used for route placement (node snap or edge projection). */
typedef struct RouteEndpointAnchor {
    bool valid;
    bool on_edge;
    uint32_t node;
    uint32_t edge_from;
    uint32_t edge_to;
    float world_x;
    float world_y;
    float dist_to_from_m;
    float dist_to_to_m;
} RouteEndpointAnchor;

/* Route segment stored for proximity snapping queries. */
typedef struct RouteSnapSegment {
    uint32_t from;
    uint32_t to;
} RouteSnapSegment;

/* Cell->segment membership entry used while building snap index. */
typedef struct RouteSnapCellEntry {
    uint64_t key;
    uint32_t segment_index;
} RouteSnapCellEntry;

/* Compacted cell span for fast key lookup in snap index. */
typedef struct RouteSnapCellSpan {
    uint64_t key;
    uint32_t start;
    uint32_t count;
} RouteSnapCellSpan;

/* Per-region route edge spatial index for bounded nearest-segment picking. */
typedef struct RouteSnapIndex {
    bool ready;
    float cell_size_m;
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    RouteSnapSegment *segments;
    uint32_t segment_count;
    RouteSnapCellEntry *entries;
    uint32_t entry_count;
    RouteSnapCellSpan *cells;
    uint32_t cell_count;
    uint32_t *segment_seen;
    uint32_t query_seq;
} RouteSnapIndex;

/* Route worker request payload. */
typedef struct RouteComputeJob {
    uint32_t request_id;
    uint32_t start_node;
    uint32_t goal_node;
    RouteEndpointAnchor start_anchor;
    RouteEndpointAnchor goal_anchor;
    RouteObjective objective;
    RouteTravelMode mode;
} RouteComputeJob;

/* Route worker result payload; paths are transferred to main thread ownership. */
typedef struct RouteComputeResult {
    uint32_t request_id;
    bool ok;
    uint32_t start_node;
    uint32_t goal_node;
    RouteObjective objective;
    RouteTravelMode mode;
    bool has_transfer;
    uint32_t transfer_node;
    RoutePath path;
    RoutePath drive_path;
    RoutePath walk_path;
    RouteAlternativeSet alternatives;
} RouteComputeResult;

/* Thread-safe snapshot for polygon prep worker diagnostics. */
typedef struct VkPolyPrepStats {
    uint32_t in_count;
    uint32_t out_count;
    uint64_t enqueued_count;
    uint64_t done_count;
    uint64_t drop_count;
} VkPolyPrepStats;

/* Phase 2 bridge: target ownership bucket for view/persisted runtime controls. */
typedef struct AppViewState {
    Camera camera;
    bool show_landuse;
    float building_zoom_bias;
    bool building_fill_enabled;
    float road_zoom_bias;
    bool polygon_outline_only;
    bool layer_user_enabled[TILE_LAYER_COUNT];
    bool zoom_logic_enabled;
    uint16_t layer_opacity_milli[TILE_LAYER_COUNT];
    uint16_t layer_fade_start_milli[TILE_LAYER_COUNT];
    uint16_t layer_fade_speed_milli[TILE_LAYER_COUNT];
} AppViewState;

/* Phase 2 bridge: target ownership bucket for tile/cache/runtime coverage state. */
typedef struct AppTileState {
    TileManager tile_managers[TILE_LAYER_COUNT];
    TileLoader tile_loader;
    VkTileCache vk_tile_cache;
    uint32_t tile_request_id;
    TileQueue tile_queues[TILE_LAYER_COUNT];
    TileZoomBand queue_band[TILE_LAYER_COUNT];
    TileZoomBand layer_target_band[TILE_LAYER_COUNT];
    uint32_t band_visible_expected[TILE_BAND_COUNT];
    uint32_t band_visible_loaded[TILE_BAND_COUNT];
    uint32_t band_queue_depth[TILE_BAND_COUNT];
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
    uint32_t vk_road_band_fallback_draws;
} AppTileState;

/* Phase 2 bridge: target ownership bucket for route/path interaction state. */
typedef struct AppRouteRuntimeState {
    RouteState route;
    bool dragging_start;
    bool dragging_goal;
    bool has_hover;
    uint32_t hover_node;
    RouteEndpointAnchor hover_anchor;
    RouteEndpointAnchor start_anchor;
    RouteEndpointAnchor goal_anchor;
    RouteSnapIndex route_snap_index;
    bool route_edge_snap_enabled;
    bool route_edge_snap_debug;
    bool playback_playing;
    float playback_time_s;
    float playback_speed;
    bool route_alt_visible[ROUTE_ALTERNATIVE_MAX];
    uint32_t route_snap_debug_cells;
    uint32_t route_snap_debug_segments;
    uint32_t route_snap_debug_hits;
    float route_snap_debug_query_ms;
    bool route_recompute_scheduled;
    double route_recompute_due_time;
} AppRouteRuntimeState;

/* Phase 2 bridge: target ownership bucket for worker/thread synchronization state. */
typedef struct AppWorkerState {
    bool vk_poly_prep_enabled;
    bool vk_poly_prep_running;
    pthread_t vk_poly_prep_thread;
    pthread_mutex_t vk_poly_prep_mutex;
    pthread_cond_t vk_poly_prep_cond;
    TileResult vk_poly_prep_in_jobs[APP_VK_POLY_PREP_QUEUE_CAPACITY];
    CoreQueueMutex vk_poly_prep_in_queue;
    void *vk_poly_prep_in_queue_backing[APP_VK_POLY_PREP_QUEUE_CAPACITY];
    uint32_t vk_poly_prep_in_write_seq;
    TileResult vk_poly_prep_out_jobs[APP_VK_POLY_PREP_QUEUE_CAPACITY];
    CoreQueueMutex vk_poly_prep_out_queue;
    void *vk_poly_prep_out_queue_backing[APP_VK_POLY_PREP_QUEUE_CAPACITY];
    uint32_t vk_poly_prep_out_write_seq;
    uint64_t vk_poly_prep_enqueued_count;
    uint64_t vk_poly_prep_done_count;
    uint64_t vk_poly_prep_drop_count;
    VkAssetJob vk_asset_jobs[APP_VK_ASSET_QUEUE_CAPACITY];
    uint32_t vk_asset_job_head;
    uint32_t vk_asset_job_tail;
    uint32_t vk_asset_job_count;
    uint64_t vk_asset_job_drop_count;
    uint64_t vk_asset_job_evict_count;
    uint64_t vk_asset_job_build_count;
    bool vk_asset_worker_enabled;
    bool vk_asset_worker_running;
    pthread_t vk_asset_worker_thread;
    pthread_mutex_t vk_asset_worker_mutex;
    pthread_cond_t vk_asset_worker_cond;
    VkAssetJob vk_asset_stage_jobs[APP_VK_ASSET_QUEUE_CAPACITY];
    uint32_t vk_asset_stage_head;
    uint32_t vk_asset_stage_tail;
    uint32_t vk_asset_stage_count;
    VkAssetReadyJob vk_asset_ready_jobs[APP_VK_ASSET_READY_QUEUE_CAPACITY];
    CoreQueueMutex vk_asset_ready_queue;
    void *vk_asset_ready_queue_backing[APP_VK_ASSET_READY_QUEUE_CAPACITY];
    uint32_t vk_asset_ready_write_seq;
    uint64_t vk_asset_stage_drop_count;
    uint64_t vk_asset_stage_evict_count;
    uint64_t vk_asset_stage_enqueued_count;
    uint64_t vk_asset_stage_prepared_count;
    bool route_worker_enabled;
    bool route_worker_running;
    bool route_worker_busy;
    pthread_t route_worker_thread;
    pthread_mutex_t route_worker_mutex;
    pthread_cond_t route_worker_cond;
    RouteState route_worker_state;
    bool route_job_pending;
    RouteComputeJob route_job;
    bool route_result_pending;
    RouteComputeResult route_result;
    uint32_t route_latest_requested_id;
    uint32_t route_latest_submitted_id;
    uint32_t route_latest_applied_id;
} AppWorkerState;

/* Phase 2 bridge: target ownership bucket for HUD/header layout/cache state. */
typedef struct AppUiState {
    InputState input;
    DebugOverlay overlay;
    bool hud_layer_debug_collapsed;
    SDL_FRect hud_layer_debug_panel_rect;
    SDL_FRect hud_layer_debug_collapse_rect;
    SDL_FRect hud_layer_debug_handle_rect;
    bool hud_layer_debug_layout_dirty;
    uint64_t hud_layer_debug_layout_hash;
    float hud_layer_debug_cached_w;
    float hud_layer_debug_cached_h;
    int hud_layer_debug_cached_line_count;
    int hud_layer_debug_cached_max_text_w;
    bool hud_route_panel_collapsed;
    SDL_FRect hud_route_panel_rect;
    SDL_FRect hud_route_panel_collapse_rect;
    SDL_FRect hud_route_panel_handle_rect;
    SDL_FRect hud_route_panel_row_rects[ROUTE_ALTERNATIVE_MAX];
    SDL_FRect hud_route_panel_toggle_rects[ROUTE_ALTERNATIVE_MAX];
    SDL_FRect header_layer_row_rects[TILE_LAYER_COUNT];
    SDL_FRect header_layer_label_rects[TILE_LAYER_COUNT];
    SDL_FRect header_layer_toggle_rects[TILE_LAYER_COUNT];
    SDL_FRect header_zoom_toggle_rect;
    SDL_FRect header_layer_opacity_panel_rect;
    SDL_FRect header_layer_opacity_track_rect;
    SDL_FRect header_layer_fade_panel_rect;
    SDL_FRect header_layer_fade_start_track_rect;
    SDL_FRect header_layer_fade_speed_track_rect;
    bool header_layer_opacity_dragging;
    int header_layer_fade_drag_target;
    int header_layer_panel_mode;
    bool header_layer_selected_valid;
    TileLayerKind header_layer_selected_kind;
    bool hud_route_panel_layout_dirty;
    uint64_t hud_route_panel_layout_hash;
    float hud_route_panel_cached_w;
    float hud_route_panel_cached_h;
    int hud_route_panel_cached_row_count;
    int hud_route_panel_cached_max_text_w;
    char hud_route_panel_summary_text[APP_HUD_ROUTE_LINE_CAPACITY];
    char hud_route_panel_row_text[ROUTE_ALTERNATIVE_MAX][APP_HUD_ROUTE_LINE_CAPACITY];
} AppUiState;

/* Owns core application state for the main loop. */
typedef struct AppState {
    /* Phase 2 ownership buckets (canonical state). */
    AppViewState view_state_bridge;
    AppTileState tile_state_bridge;
    AppRouteRuntimeState route_state_bridge;
    AppWorkerState worker_state_bridge;
    AppUiState ui_state_bridge;

    SDL_Window *window;
    Renderer renderer;
    bool single_line;
    RegionInfo region;
    int region_index;
    FramePhaseTimings frame_timings;
    bool trace_enabled;
    CoreTraceSession trace_session;
    double trace_start_time;
    uint64_t trace_last_tile_enqueue_drop_count;
    uint64_t trace_last_tile_enqueue_evict_count;
    uint64_t trace_last_tile_result_drop_count;
    uint64_t trace_last_tile_result_evict_count;
    uint64_t trace_last_vk_asset_drop_count;
    uint64_t trace_last_vk_asset_evict_count;
    int width;
    int height;
} AppState;

void app_bridge_sync_from_legacy(AppState *app);
void app_bridge_sync_to_legacy(AppState *app);

float app_clampf(float value, float min_value, float max_value);
uint16_t app_zoom_to_tile_level(float zoom, const RegionInfo *region);
float app_building_zoom_bias_for_region(const RegionInfo *region);
float app_road_zoom_bias_for_region(const RegionInfo *region);
void app_center_camera_on_region(Camera *camera, const RegionInfo *region, int screen_w, int screen_h);

float app_layer_zoom_start(const AppState *app, TileLayerKind kind);
bool app_layer_runtime_enabled(const AppState *app, TileLayerKind kind);
bool app_layer_active_runtime(const AppState *app, TileLayerKind kind);
float app_layer_fade_multiplier(const AppState *app, TileLayerKind kind);
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
bool app_vk_asset_enqueue(AppState *app, TileLayerKind kind, TileCoord coord, TileZoomBand band);
bool app_vk_asset_worker_init(AppState *app);
void app_vk_asset_worker_shutdown(AppState *app);
void app_process_vk_asset_queue(AppState *app, uint32_t max_jobs, double max_time_slice_sec);

uint32_t app_draw_visible_tiles(AppState *app);
void app_draw_region_bounds(AppState *app);

bool app_load_route_graph(AppState *app);
void app_route_release_snap_index(AppState *app);
void app_route_schedule_recompute(AppState *app, double debounce_sec);
void app_route_poll_result(AppState *app);
bool app_route_worker_init(AppState *app);
void app_route_worker_shutdown(AppState *app);
void app_route_worker_clear(AppState *app);
bool app_is_near_node(const AppState *app, float world_x, float world_y, uint32_t *out_node);
bool app_pick_route_anchor(AppState *app, float world_x, float world_y, RouteEndpointAnchor *out_anchor);
bool app_pick_route_anchor_unbounded(AppState *app, float world_x, float world_y, RouteEndpointAnchor *out_anchor);
bool app_mouse_over_node(const AppState *app, uint32_t node, float radius);
bool app_mouse_over_anchor(const AppState *app, const RouteEndpointAnchor *anchor, float radius);
void app_update_hover(AppState *app);
void app_draw_hover_marker(AppState *app);
bool app_recompute_route(AppState *app);
const RoutePath *app_route_primary_path(const AppState *app, uint32_t *out_alt_index);

void app_playback_reset(AppState *app);
void app_playback_update(AppState *app, float dt);
float app_next_playback_speed(float current, int direction);
void app_draw_playback_marker(AppState *app);
void app_draw_route_panel(AppState *app);
bool app_route_panel_handle_click(AppState *app);

bool app_header_button_hit(const AppState *app, int x, int y);
bool app_header_layer_toggle_click(AppState *app, int x, int y);
bool app_header_layer_slider_update(AppState *app);
void app_draw_header_bar(AppState *app);
void app_draw_layer_debug(AppState *app);
void app_copy_overlay_text(AppState *app);
bool app_handle_hud_clicks(AppState *app);

void app_runtime_begin_frame(AppState *app, double *out_frame_begin, double *out_after_events);
bool app_runtime_handle_global_controls(AppState *app);
void app_runtime_update_frame(AppState *app,
                              double *io_last_time,
                              float *out_dt,
                              double *out_after_update,
                              double *out_after_queue,
                              double *out_after_integrate,
                              double *out_after_route);
void app_runtime_render_frame(AppState *app,
                              RendererBackend *io_last_backend,
                              uint32_t *out_visible_tiles,
                              double *out_before_present,
                              double *out_after_render);

#endif
