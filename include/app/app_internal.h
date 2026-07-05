#ifndef MAPFORGE_APP_INTERNAL_H
#define MAPFORGE_APP_INTERNAL_H

#include "camera/camera.h"
#include "app/app_pins.h"
#include "core/input.h"
#include "ui/debug_overlay.h"
#include "app/region.h"
#include "app/region_loader.h"
#include "map/layer_policy.h"
#include "map/tile_loader.h"
#include "map/tile_manager.h"
#include "app/app_runtime_loop.h"
#include "app/workspace_authoring/map_forge_workspace_authoring_host.h"
#include "render/renderer.h"
#include "render/vk_tile_cache.h"
#include "route/route.h"
#include "ui/shared_theme_font_adapter.h"
#include "core_trace.h"
#include "core_queue.h"
#include "core_pane.h"

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
/* Prevent rapid tile-band flips when zoom oscillates near thresholds. */
#define APP_TILE_BAND_SWITCH_DEBOUNCE_SEC 0.14
/* Prevent immediate queue rebuild churn for band-only transitions. */
#define APP_TILE_QUEUE_REBUILD_MIN_SEC 0.10
/* Minimum global and per-layer visible coverage before committing band switches. */
#define APP_TILE_COVERAGE_GATE_GLOBAL_MIN 0.95f
#define APP_TILE_COVERAGE_GATE_LAYER_MIN 0.90f
/* Maximum delay before forcing a band commit when coverage stays below target. */
#define APP_TILE_COVERAGE_GATE_MAX_WAIT_SEC 0.75
/* Blend window for old/new band handoff in presentation. */
#define APP_TILE_BAND_BLEND_WINDOW_SEC 0.22
/* Per-layer tile presentation hold cache capacity and TTL. */
#define APP_TILE_PRESENT_HOLD_CAPACITY 4096u
#define APP_TILE_PRESENT_HOLD_TTL_SEC 0.28
/* Maximum ancestor depth when resolving parent retention fallbacks. */
#define APP_TILE_RETENTION_PARENT_MAX_DEPTH 4u
/* Route recompute debounce while dragging endpoints. */
#define APP_ROUTE_DRAG_DEBOUNCE_SEC 0.045
/* Runtime trace sample/marker ring capacities. */
#define APP_TRACE_SAMPLE_CAPACITY 262144u
#define APP_TRACE_MARKER_CAPACITY 4096u
#define APP_HUD_ROUTE_LINE_CAPACITY 192u
#define APP_INGEST_LIST_MAX 256
#define APP_INGEST_NAME_CAP 128
#define APP_PIN_LIST_MAX 256

typedef enum TileQueueLane {
    TILE_QUEUE_LANE_L0_VISIBLE_MISSING = 0,
    TILE_QUEUE_LANE_L1_VISIBLE_REFINE = 1,
    TILE_QUEUE_LANE_L2_NEAR_PREFETCH = 2,
    TILE_QUEUE_LANE_L3_FAR_PREFETCH = 3,
    TILE_QUEUE_LANE_COUNT = 4
} TileQueueLane;

typedef enum AppTileLifecycleState {
    APP_TILE_LIFECYCLE_ABSENT = 0,
    APP_TILE_LIFECYCLE_REQUESTED = 1,
    APP_TILE_LIFECYCLE_DECODED_CPU = 2,
    APP_TILE_LIFECYCLE_UPLOADED_GPU = 3,
    APP_TILE_LIFECYCLE_RENDERABLE = 4,
    APP_TILE_LIFECYCLE_STALE = 5,
    APP_TILE_LIFECYCLE_STATE_COUNT = 6
} AppTileLifecycleState;

enum {
    APP_TILE_LIFECYCLE_CAPACITY = 32768u
};

/* Per-tile queue entry sorted by distance from camera center tile. */
typedef struct TileQueueItem {
    TileCoord coord;
    uint32_t dist2;
    TileQueueLane lane;
} TileQueueItem;

/* Per-layer tile loading queue and cursor state. */
typedef struct TileQueue {
    TileQueueItem *items;
    uint32_t count;
    uint32_t index;
    uint32_t capacity;
} TileQueue;

typedef struct AppTileLifecycleEntry {
    bool occupied;
    TileCoord coord;
    TileLayerKind kind;
    TileZoomBand band;
    AppTileLifecycleState state;
    bool has_cpu;
    bool has_gpu;
    bool is_fallback_renderable;
    bool is_ideal_renderable;
    bool visible_drop_pending;
    uint64_t last_transition_frame;
    double last_transition_time;
} AppTileLifecycleEntry;

/* Runtime polygon fill budget for Vulkan tile cache rendering. */
typedef struct VkPolyFillBudget {
    bool enabled;
    uint32_t total_cap;
    uint32_t total_used;
    uint32_t layer_cap[TILE_LAYER_COUNT];
    uint32_t layer_used[TILE_LAYER_COUNT];
    bool layer_fill_allowed[TILE_LAYER_COUNT];
    uint32_t layer_fill_expected[TILE_LAYER_COUNT];
    uint32_t layer_fill_ready[TILE_LAYER_COUNT];
    bool screen_fill_allowed;
    uint32_t screen_fill_expected;
    uint32_t screen_fill_ready;
} VkPolyFillBudget;

/* Runtime cap for on-demand Vulkan polygon asset generation. */
typedef struct VkPolyAssetBuildBudget {
    uint32_t cap;
    uint32_t used;
} VkPolyAssetBuildBudget;

/* Captures one polygon tile presentation decision for opt-in zoom diagnostics. */
typedef struct AppPolygonPresentTraceDecision {
    TileLayerKind kind;
    TileCoord requested_coord;
    TileCoord draw_coord;
    TileZoomBand target_band;
    TileZoomBand resolved_band;
    float zoom;
    float layer_opacity;
    float fade_multiplier;
    bool vk_backend;
    bool vk_assets_enabled;
    bool same_coord;
    uint32_t fallback_depth;
    bool present_hold_hit;
    bool present_hold_valid;
    bool band_blend_attempted;
    bool band_blend_drawn;
    bool asset_present;
    bool retained_fill_ready;
    bool retained_fill_drawn;
    bool retained_fill_skipped_budget;
    bool retained_fill_missing;
    bool retained_outline_drawn;
    bool cpu_fallback_allowed;
    bool cpu_fallback_drawn;
    bool asset_enqueue_attempted;
    bool asset_enqueue_budget_blocked;
    bool asset_enqueue_succeeded;
    bool no_draw_after_asset_miss;
} AppPolygonPresentTraceDecision;

/* Centralized runtime budget knobs for tile/load/render throughput control. */
typedef struct AppRuntimeBudgetPolicy {
    uint32_t load_road_cap;
    uint32_t load_polygon_cap_small_view;
    uint32_t load_polygon_cap_medium_view;
    uint32_t load_polygon_cap_large_view;
    uint32_t load_polygon_building_bonus;
    uint32_t lane_l2_cap_low_budget;
    uint32_t lane_l2_cap_high_budget;
    uint32_t lane_l3_cap_low_budget;
    uint32_t lane_l3_cap_high_budget;
    uint32_t integrate_fallback_budget;
    uint32_t integrate_cap;
    uint32_t vk_poly_prep_integrate_budget;
    double vk_poly_prep_integrate_time_slice_sec;
    uint32_t vk_asset_build_budget;
    double vk_asset_build_time_slice_sec;
    uint32_t vk_poly_asset_cap_small_view;
    uint32_t vk_poly_asset_cap_default;
} AppRuntimeBudgetPolicy;

/* Per-frame budget diagnostics used for D1 throughput tuning. */
typedef struct AppRuntimeBudgetFrameStats {
    uint32_t load_budget_requested_total;
    uint32_t load_budget_applied_total;
    uint32_t load_budget_clamped_count;
    uint32_t load_budget_exhausted_count;
    uint32_t lane_cap_hits[TILE_QUEUE_LANE_COUNT];
    uint32_t integrate_budget_requested;
    uint32_t integrate_budget_applied;
    uint32_t integrate_budget_clamped_count;
    uint32_t integrate_budget_exhausted_count;
    uint32_t vk_asset_jobs_budget;
    uint32_t vk_asset_jobs_built;
    uint32_t vk_asset_budget_saturated_count;
    uint32_t vk_poly_asset_budget_cap;
    uint32_t vk_poly_asset_budget_used;
    uint32_t vk_poly_asset_budget_hit_count;
} AppRuntimeBudgetFrameStats;

/* Last-known tile presentation band retained briefly for visual continuity. */
typedef struct TilePresentHoldEntry {
    bool occupied;
    /* Visible request key; draw_coord may be a retained parent tile. */
    TileCoord coord;
    TileCoord draw_coord;
    TileZoomBand band;
    double expires_at;
    uint64_t stamp;
} TilePresentHoldEntry;

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
    uint64_t quarantine_job_count;
    uint64_t quarantine_polygon_count;
    uint64_t quarantine_ring_bounds_count;
    uint64_t quarantine_ring_min_points_count;
    uint64_t quarantine_ring_degenerate_count;
    uint64_t winding_normalized_count;
    uint64_t quarantine_jobs_by_layer[TILE_LAYER_COUNT];
    uint64_t quarantine_rings_by_layer[TILE_LAYER_COUNT];
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
    TileZoomBand previous_target_band[TILE_LAYER_COUNT];
    TileZoomBand stable_target_band[TILE_LAYER_COUNT];
    TileZoomBand layer_target_band[TILE_LAYER_COUNT];
    double layer_band_last_change_time[TILE_LAYER_COUNT];
    uint32_t band_visible_expected[TILE_BAND_COUNT];
    uint32_t band_visible_loaded[TILE_BAND_COUNT];
    uint32_t band_queue_depth[TILE_BAND_COUNT];
    uint32_t cache_target[TILE_LAYER_COUNT];
    uint32_t cache_resident[TILE_LAYER_COUNT];
    uint32_t cache_evicted_frame[TILE_LAYER_COUNT];
    uint64_t cache_evicted_total_by_layer[TILE_LAYER_COUNT];
    uint32_t cache_evicted_frame_total;
    uint64_t cache_evicted_total;
    uint32_t layer_expected[TILE_LAYER_COUNT];
    uint32_t layer_done[TILE_LAYER_COUNT];
    uint32_t layer_inflight[TILE_LAYER_COUNT];
    uint32_t lane_queue_depth[TILE_QUEUE_LANE_COUNT];
    uint32_t lane_service_count[TILE_QUEUE_LANE_COUNT];
    uint32_t lane_l0_pending;
    bool lane_l0_pending_active;
    double lane_l0_pending_since;
    float lane_l0_latency_ms;
    uint64_t lane_l0_saturation_total;
    uint64_t lane_l0_dropped_visible_requests;
    uint64_t lane_l0_retry_visible_requests;
    uint32_t coverage_suppressed_frame;
    uint32_t coverage_suppressed_visible_frame;
    uint64_t coverage_suppressed_total;
    uint64_t coverage_suppressed_visible_total;
    uint64_t lifecycle_frame_index;
    uint32_t lifecycle_transition_count;
    uint32_t lifecycle_invalid_transition_count;
    uint64_t lifecycle_invalid_transition_total;
    uint32_t lifecycle_transition_to_state[APP_TILE_LIFECYCLE_STATE_COUNT];
    uint32_t lifecycle_renderable_ideal_count;
    uint32_t lifecycle_renderable_fallback_count;
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
    uint32_t visible_ideal_count;
    uint32_t visible_renderable_count;
    uint32_t visible_missing_count;
    float visible_coverage_ratio;
    float layer_coverage_ratio[TILE_LAYER_COUNT];
    bool coverage_gate_pending[TILE_LAYER_COUNT];
    TileZoomBand coverage_gate_target_band[TILE_LAYER_COUNT];
    double coverage_gate_pending_since[TILE_LAYER_COUNT];
    uint32_t coverage_gate_deferred_count;
    uint32_t coverage_gate_timeout_count;
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
    uint32_t draw_path_vk_count;
    uint32_t draw_path_fallback_count;
    uint32_t band_commit_frame_count;
    uint32_t queue_rebuild_frame_count;
    uint64_t band_commit_total;
    uint64_t queue_rebuild_total;
    uint32_t band_switch_deferred_count;
    uint32_t queue_rebuild_deferred_count;
    uint32_t transition_blend_draw_count;
    uint32_t present_hold_hits;
    uint32_t present_hold_misses;
    uint32_t present_hold_updates;
    uint32_t presenter_invariant_fail_count;
    bool presenter_invariants_enabled;
    bool contour_runtime_enabled;
    bool headless_export_policy_active;
    bool headless_stabilize_visible_zoom;
    bool headless_stabilize_tile_bands;
    bool headless_allow_tile_fallback;
    bool headless_route_simplify_screen_space;
    bool headless_locked_visible_zoom_valid;
    bool headless_locked_bands_valid;
    uint16_t headless_locked_visible_zoom;
    uint64_t present_hold_tick;
    TilePresentHoldEntry present_hold[TILE_LAYER_COUNT][APP_TILE_PRESENT_HOLD_CAPACITY];
    AppTileLifecycleEntry lifecycle_entries[APP_TILE_LIFECYCLE_CAPACITY];
    double last_queue_rebuild_time;
    AppRuntimeBudgetPolicy budget_policy;
    AppRuntimeBudgetFrameStats budget_frame;
} AppTileState;

typedef struct AppRoutePreviewState {
    bool valid;
    bool follow_active;
    bool has_lookahead;
    uint32_t segment_index;
    float sample_time_s;
    float world_x;
    float world_y;
    float heading_rad;
    float lookahead_world_x;
    float lookahead_world_y;
} AppRoutePreviewState;

typedef enum AppLeftPaneSection {
    APP_LEFT_PANE_SECTION_PINS = 0,
    APP_LEFT_PANE_SECTION_INGEST = 1,
    APP_LEFT_PANE_SECTION_INSPECT = 2,
    APP_LEFT_PANE_SECTION_COUNT = 3
} AppLeftPaneSection;

typedef enum AppTextEntryFocus {
    APP_TEXT_ENTRY_FOCUS_NONE = 0,
    APP_TEXT_ENTRY_FOCUS_PIN_NAME = 1,
    APP_TEXT_ENTRY_FOCUS_INGEST_PATH = 2
} AppTextEntryFocus;

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
    bool preview_follow_enabled;
    bool preview_heading_up;
    bool preview_heading_memory_valid;
    float preview_heading_memory_rad;
    float preview_heading_memory_sample_time_s;
    AppRoutePreviewState preview;
    bool route_alt_visible[ROUTE_ALTERNATIVE_MAX];
    uint32_t route_snap_debug_cells;
    uint32_t route_snap_debug_segments;
    uint32_t route_snap_debug_hits;
    float route_snap_debug_query_ms;
    bool route_recompute_scheduled;
    double route_recompute_due_time;
    bool route_graph_loading;
    uint32_t route_graph_load_request_id;
} AppRouteRuntimeState;

/* Phase 2 bridge: target ownership bucket for worker/thread synchronization state. */
typedef struct AppWorkerState {
    uint32_t world_generation;
    uint32_t tile_generation;
    uint32_t route_generation;
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
    uint64_t vk_poly_prep_quarantine_job_count;
    uint64_t vk_poly_prep_quarantine_polygon_count;
    uint64_t vk_poly_prep_quarantine_ring_bounds_count;
    uint64_t vk_poly_prep_quarantine_ring_min_points_count;
    uint64_t vk_poly_prep_quarantine_ring_degenerate_count;
    uint64_t vk_poly_prep_winding_normalized_count;
    uint64_t vk_poly_prep_quarantine_jobs_by_layer[TILE_LAYER_COUNT];
    uint64_t vk_poly_prep_quarantine_rings_by_layer[TILE_LAYER_COUNT];
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
    bool route_graph_job_pending;
    uint32_t route_graph_job_request_id;
    char route_graph_job_path[MAPFORGE_REGION_PATH_CAPACITY];
    bool route_graph_result_pending;
    bool route_graph_result_ok;
    uint32_t route_graph_result_request_id;
    RouteState route_graph_result_state;
    RouteSnapIndex route_graph_result_snap_index;
    uint32_t route_latest_requested_id;
    uint32_t route_latest_submitted_id;
    uint32_t route_latest_applied_id;
} AppWorkerState;

/* Phase 2 bridge: target ownership bucket for HUD/header layout/cache state. */
typedef struct AppUiState {
    InputState input;
    DebugOverlay overlay;
    MapForgeWorkspaceAuthoringHostState workspace_authoring;
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
    SDL_FRect header_layer_strip_rect;
    float header_layer_strip_scroll_px;
    float header_layer_strip_content_w;
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
    bool hud_ingest_panel_collapsed;
    SDL_FRect hud_ingest_panel_rect;
    SDL_FRect hud_ingest_collapse_rect;
    SDL_FRect hud_ingest_handle_rect;
    SDL_FRect hud_ingest_source_tab_rect;
    SDL_FRect hud_ingest_active_tab_rect;
    SDL_FRect hud_ingest_import_rect;
    SDL_FRect hud_ingest_import_all_rect;
    SDL_FRect hud_ingest_edit_toggle_rect;
    SDL_FRect hud_ingest_folder_rect;
    SDL_FRect hud_ingest_apply_rect;
    SDL_FRect hud_ingest_row_rects[APP_INGEST_LIST_MAX];
    int hud_ingest_row_base;
    int hud_ingest_row_count;
    bool left_pane_open;
    AppLeftPaneSection left_pane_section;
    AppTextEntryFocus text_entry_focus;
    SDL_FRect left_pane_rect;
    SDL_FRect map_viewport_rect;
    SDL_FRect pin_pane_closed_rect;
    SDL_FRect pin_pane_header_rect;
    SDL_FRect pin_pane_close_rect;
    SDL_FRect pin_pane_tab_rects[APP_LEFT_PANE_SECTION_COUNT];
    SDL_FRect pin_pane_content_rect;
    SDL_FRect pin_pane_summary_rect;
    SDL_FRect pin_pane_list_rect;
    SDL_FRect pin_pane_row_rects[APP_PIN_LIST_MAX];
    SDL_FRect pin_pane_add_rect;
    SDL_FRect pin_pane_save_rect;
    SDL_FRect pin_pane_delete_rect;
    SDL_FRect pin_pane_cancel_rect;
    SDL_FRect pin_pane_name_rect;
    SDL_FRect pin_pane_type_rect;
    SDL_FRect pin_pane_color_rect;
    SDL_FRect pin_pane_private_rect;
    SDL_FRect pin_pane_hint_rect;
    SDL_FRect pin_pane_status_rect;
    SDL_FRect pin_drag_preview_rect;
    int pin_pane_row_count;
    int pin_pane_row_base;
    int pin_selected_index;
    bool pin_add_mode_active;
    bool pin_editor_has_draft;
    bool pin_editor_is_new;
    bool pin_editor_waiting_for_map_click;
    bool pin_name_edit_active;
    int pin_name_cursor_index;
    double pin_name_last_click_time_sec;
    bool pin_list_drag_armed;
    bool pin_list_drag_active;
    int pin_drag_source_index;
    int pin_drag_target_index;
    int pin_drag_target_slot;
    int pin_drag_start_mouse_y;
    int pin_drag_last_mouse_y;
    char pin_route_start_id[MAPFORGE_PIN_ID_CAPACITY];
    char pin_route_goal_id[MAPFORGE_PIN_ID_CAPACITY];
    MapForgePin pin_editor_draft;
    char pin_editor_name_edit[MAPFORGE_PIN_NAME_CAPACITY];
    char pin_editor_status[APP_HUD_ROUTE_LINE_CAPACITY];
} AppUiState;

/* Tracks subsystem ownership so shutdown can be deterministic and idempotent. */
typedef struct AppRuntimeLifetime {
    bool theme_loaded;
    bool sdl_initialized;
    bool window_created;
    bool renderer_initialized;
    bool vk_tile_cache_initialized;
    bool vk_poly_prep_initialized;
    bool vk_asset_worker_initialized;
    bool route_worker_initialized;
    bool ttf_initialized;
    bool trace_session_initialized;
    bool route_state_initialized;
    bool tile_loader_initialized;
    uint8_t tile_managers_initialized;
    bool persisted_state_ready;
    bool shutdown_completed;
} AppRuntimeLifetime;

typedef enum AppViewportScenarioMode {
    APP_VIEWPORT_SCENARIO_NONE = 0,
    APP_VIEWPORT_SCENARIO_PHASE_A = 1,
    APP_VIEWPORT_SCENARIO_PHASE_B = 2
} AppViewportScenarioMode;

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
    uint64_t trace_last_vk_asset_stage_drop_count;
    uint64_t trace_last_vk_asset_stage_evict_count;
    AppRuntimeLifetime lifetime;
    int width;
    int height;
    bool ingest_panel_open;
    bool ingest_show_active_tab;
    bool ingest_edit_mode;
    char input_root[MAPFORGE_REGION_PATH_CAPACITY];
    char input_root_edit[MAPFORGE_REGION_PATH_CAPACITY];
    char latest_imported_region[APP_INGEST_NAME_CAP];
    char ingest_status[APP_HUD_ROUTE_LINE_CAPACITY];
    char ingest_package_status[APP_HUD_ROUTE_LINE_CAPACITY];
    char ingest_osm_files[APP_INGEST_LIST_MAX][APP_INGEST_NAME_CAP];
    int ingest_osm_count;
    int ingest_selected_osm;
    char ingest_active_regions[APP_INGEST_LIST_MAX][APP_INGEST_NAME_CAP];
    int ingest_active_count;
    int ingest_selected_active;
    uint32_t ingest_last_active_click_tick;
    int ingest_last_active_click_index;
    bool ingest_import_running;
    int ingest_import_pid;
    bool ingest_import_all;
    int ingest_import_expected_count;
    char ingest_import_open_region[APP_INGEST_NAME_CAP];
    int ingest_import_total_steps;
    int ingest_import_completed_steps;
    char ingest_import_progress_path[MAPFORGE_REGION_PATH_CAPACITY];
    bool viewport_scenario_active;
    bool viewport_scenario_completed;
    AppViewportScenarioMode viewport_scenario_mode;
    double viewport_scenario_start_time;
    double viewport_scenario_duration_sec;
    float viewport_scenario_origin_x;
    float viewport_scenario_origin_y;
    float viewport_scenario_origin_zoom;
    MapForgePinsFile pins_file;
    char pins_path[MAPFORGE_REGION_PATH_CAPACITY];
    bool pins_dirty;
} AppState;

void app_bridge_sync_from_legacy(AppState *app);
void app_bridge_sync_to_legacy(AppState *app);
bool app_runtime_apply_window_size(AppState *app, int width, int height);
void app_worker_contract_init(AppState *app);
uint32_t app_worker_contract_bump_world_generation(AppState *app);
uint32_t app_worker_contract_bump_tile_generation(AppState *app);
uint32_t app_worker_contract_next_route_request(AppState *app);
void app_worker_contract_note_route_submitted(AppState *app, uint32_t request_id);
void app_worker_contract_note_route_applied(AppState *app, uint32_t request_id);
void app_worker_contract_reset_route_pipeline(AppState *app);
bool app_worker_contract_request_is_stale(uint32_t request_id, uint32_t current_generation);
bool app_worker_contract_tile_request_is_current(const AppState *app, uint32_t request_id);
bool app_worker_contract_route_result_is_current(const AppState *app, uint32_t request_id);
bool app_worker_contract_choose_evict_offset(const uint32_t *request_ids,
                                             uint32_t count,
                                             uint32_t current_generation,
                                             uint32_t *out_offset);

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
void app_runtime_budget_policy_init(AppState *app);
void app_runtime_budget_reset_frame(AppState *app);
uint32_t app_tile_load_budget(TileLayerKind kind, uint32_t expected);
uint32_t app_tile_integrate_budget(TileLayerKind kind, uint32_t expected);
void app_tile_viewport_invalidate(AppState *app);
void app_clear_tile_queue(AppState *app);
void app_drain_tile_results(AppState *app, uint32_t budget);
void app_refresh_layer_states(AppState *app);
void app_update_tile_queue(AppState *app);
void app_tile_lifecycle_begin_frame(AppState *app);
void app_tile_lifecycle_transition(AppState *app,
                                   TileLayerKind kind,
                                   TileCoord coord,
                                   TileZoomBand band,
                                   AppTileLifecycleState next_state,
                                   bool has_cpu,
                                   bool has_gpu,
                                   bool is_fallback_renderable,
                                   bool is_ideal_renderable);
void app_tile_lifecycle_mark_visible_drop(AppState *app,
                                          TileLayerKind kind,
                                          TileCoord coord,
                                          TileZoomBand band);
bool app_tile_lifecycle_consume_visible_drop_retry(AppState *app,
                                                   TileLayerKind kind,
                                                   TileCoord coord,
                                                   TileZoomBand band);
void app_tile_lifecycle_mark_stale_outside_queue(AppState *app,
                                                 TileCoord queue_top_left,
                                                 TileCoord queue_bottom_right,
                                                 uint16_t queue_zoom);
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

bool app_try_draw_vk_cached_polygon_tile(AppState *app,
                                         TileLayerKind kind,
                                         TileCoord coord,
                                         TileZoomBand band,
                                         bool allow_retained_fill,
                                         VkPolyFillBudget *budget,
                                         VkPolyAssetBuildBudget *asset_build_budget,
                                         AppPolygonPresentTraceDecision *trace);
bool app_try_draw_vk_cached_tile(AppState *app, TileLayerKind kind, TileCoord coord, TileZoomBand band);

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
bool app_route_service_select_alternative(AppState *app, uint32_t alt_index);
bool app_route_service_toggle_alternative_visibility(AppState *app, uint32_t alt_index);
void app_route_service_clear_route_selection(AppState *app);
bool app_route_service_begin_endpoint_drag(AppState *app, bool set_start);
bool app_route_service_set_endpoint_anchor(AppState *app,
                                           bool set_start,
                                           const RouteEndpointAnchor *anchor,
                                           double recompute_debounce_sec);
bool app_route_service_update_drag_endpoint(AppState *app,
                                            const RouteEndpointAnchor *anchor,
                                            double recompute_debounce_sec);
bool app_route_service_finish_endpoint_drag(AppState *app,
                                            bool set_start,
                                            double recompute_debounce_sec);

void app_route_preview_reset(AppState *app);
void app_route_preview_set_follow_enabled(AppState *app, bool enabled);
bool app_route_preview_toggle_follow(AppState *app);
void app_route_preview_disable_follow(AppState *app);
void app_route_preview_toggle_heading_mode(AppState *app);
void app_route_preview_update_state(AppState *app);
void app_route_preview_apply_follow(AppState *app);
void app_route_preview_update(AppState *app);
void app_playback_reset(AppState *app);
void app_playback_update(AppState *app, float dt);
float app_next_playback_speed(float current, int direction);
void app_draw_playback_marker(AppState *app);
void app_route_panel_model_update(AppState *app);
void app_draw_route_panel(AppState *app);
bool app_route_panel_handle_click(AppState *app);

bool app_header_button_hit(const AppState *app, int x, int y);
bool app_header_layer_toggle_click(AppState *app, int x, int y);
bool app_header_layer_scroll_update(AppState *app);
bool app_header_layer_slider_update(AppState *app);
void app_draw_header_bar(AppState *app);
void app_draw_layer_debug(AppState *app);
void app_draw_workspace_authoring_overlay(AppState *app);
void app_copy_overlay_text(AppState *app);
bool app_init(AppState *app);
void app_shutdown(AppState *app);

#endif
