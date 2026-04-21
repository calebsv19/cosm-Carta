#ifndef MAPFORGE_APP_RUNTIME_LOOP_H
#define MAPFORGE_APP_RUNTIME_LOOP_H

#include "render/renderer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct AppState;

/* Render-side derivation outputs for frame-visible tile lanes. */
typedef struct AppVisibleTileRenderStats {
    uint32_t visible_tiles;
    uint32_t loading_expected;
    uint32_t loading_done;
    uint32_t vk_asset_misses;
} AppVisibleTileRenderStats;

typedef enum AppInputRouteTargetPolicy {
    APP_INPUT_ROUTE_TARGET_GLOBAL = 0,
    APP_INPUT_ROUTE_TARGET_FOCUSED_PANE,
    APP_INPUT_ROUTE_TARGET_FALLBACK
} AppInputRouteTargetPolicy;

typedef enum AppInputInvalidationReasonBits {
    APP_INPUT_INVALIDATE_REASON_NONE = 0,
    APP_INPUT_INVALIDATE_REASON_KEYBOARD = 1u << 0u,
    APP_INPUT_INVALIDATE_REASON_POINTER = 1u << 1u,
    APP_INPUT_INVALIDATE_REASON_WHEEL = 1u << 2u,
    APP_INPUT_INVALIDATE_REASON_WINDOW = 1u << 3u,
    APP_INPUT_INVALIDATE_REASON_QUIT = 1u << 4u,
    APP_INPUT_INVALIDATE_REASON_GLOBAL = 1u << 5u
} AppInputInvalidationReasonBits;

typedef struct AppInputEventRaw {
    uint32_t sdl_event_count;
    uint32_t wait_call_count;
    uint32_t wait_blocked_ms;
    uint32_t quit_event_count;
    uint32_t window_event_count;
    uint32_t mouse_event_count;
    uint32_t wheel_event_count;
    uint32_t keydown_event_count;
    uint32_t keyup_event_count;
    uint32_t other_event_count;
    bool quit_requested;
} AppInputEventRaw;

typedef struct AppInputEventNormalized {
    uint32_t action_count;
    uint32_t immediate_count;
    uint32_t queued_count;
    uint32_t ignored_count;
    bool text_entry_gate_active;
    bool has_global_shortcut_actions;
    bool has_pointer_actions;
    bool has_keyboard_actions;
} AppInputEventNormalized;

typedef struct AppInputRouteResult {
    uint32_t routed_global_count;
    uint32_t routed_pane_count;
    uint32_t routed_fallback_count;
    bool consumed;
    AppInputRouteTargetPolicy target_policy;
} AppInputRouteResult;

typedef struct AppInputInvalidationResult {
    uint32_t target_invalidation_count;
    uint32_t full_invalidation_count;
    uint32_t invalidation_reason_bits;
    bool full_invalidate;
} AppInputInvalidationResult;

typedef struct AppRuntimeInputFrame {
    AppInputEventRaw raw;
    AppInputEventNormalized normalized;
    AppInputRouteResult route;
    AppInputInvalidationResult invalidation;
} AppRuntimeInputFrame;

typedef struct AppRuntimeRenderDeriveFrame {
    RendererBackend frame_backend;
    bool backend_changed;
    uint8_t clear_r;
    uint8_t clear_g;
    uint8_t clear_b;
    uint8_t clear_a;
    uint32_t cached_tiles;
    uint32_t cache_capacity;
    uint32_t input_invalidation_reason_bits;
} AppRuntimeRenderDeriveFrame;

typedef struct AppRuntimeRenderSubmitFrame {
    AppVisibleTileRenderStats tile_stats;
    uint32_t draw_pass_count;
    double before_present;
    double after_render;
} AppRuntimeRenderSubmitFrame;

typedef struct AppRuntimeRenderTitleFrame {
    uint32_t visible_tiles;
    uint32_t cached_tiles;
    uint32_t cache_capacity;
    bool custom_title_enabled;
    char window_title[128];
} AppRuntimeRenderTitleFrame;

/* Per-frame dispatch outputs across event/update/queue/render stages. */
typedef struct AppRuntimeDispatchFrame {
    double frame_begin;
    double after_events;
    AppRuntimeInputFrame input;
    float dt;
    double after_update;
    double after_queue;
    double after_integrate;
    double after_route;
    double after_render_derive;
    uint32_t visible_tiles;
    uint32_t loading_expected;
    uint32_t loading_done;
    uint32_t vk_asset_misses;
    uint32_t render_draw_pass_count;
    uint32_t render_invalidation_reason_bits;
    double before_present;
    double after_render;
    bool skipped_for_global_controls;
} AppRuntimeDispatchFrame;

void app_runtime_process_input_frame(struct AppState *app,
                                     AppRuntimeInputFrame *out_input,
                                     double *out_frame_begin,
                                     double *out_after_events);
void app_runtime_begin_frame(struct AppState *app, double *out_frame_begin, double *out_after_events);
bool app_runtime_handle_global_controls(struct AppState *app);
void app_runtime_ingest_shutdown(struct AppState *app);
void app_apply_shared_ui_font(struct AppState *app);
void app_runtime_update_frame(struct AppState *app,
                              double *io_last_time,
                              float *out_dt,
                              double *out_after_update,
                              double *out_after_queue,
                              double *out_after_integrate,
                              double *out_after_route);
void app_runtime_render_derive_frame(const struct AppState *app,
                                     RendererBackend *io_last_backend,
                                     const AppRuntimeInputFrame *input_frame,
                                     AppRuntimeRenderDeriveFrame *out_derive);
void app_runtime_render_submit_frame(struct AppState *app,
                                     const AppRuntimeRenderDeriveFrame *derive,
                                     AppRuntimeRenderSubmitFrame *out_submit);
void app_runtime_render_derive_title_frame(const struct AppState *app,
                                           uint32_t visible_tiles,
                                           uint32_t cached_tiles,
                                           uint32_t cache_capacity,
                                           AppRuntimeRenderTitleFrame *out_title);
void app_runtime_render_apply_title_frame(struct AppState *app,
                                          const AppRuntimeRenderTitleFrame *title);
bool app_runtime_has_immediate_work(const struct AppState *app, double now_sec);
int app_runtime_compute_wait_timeout_ms(const struct AppState *app, double now_sec);
void app_runtime_loop_diag_tick(const AppRuntimeDispatchFrame *frame);
void app_runtime_render_frame(struct AppState *app,
                              RendererBackend *io_last_backend,
                              const AppRuntimeInputFrame *input_frame,
                              AppVisibleTileRenderStats *out_tile_stats,
                              double *out_after_render_derive,
                              uint32_t *out_draw_pass_count,
                              uint32_t *out_render_invalidation_reason_bits,
                              double *out_before_present,
                              double *out_after_render);
void app_runtime_dispatch_frame(struct AppState *app,
                                double *io_last_time,
                                RendererBackend *io_last_backend,
                                AppRuntimeDispatchFrame *out_frame);

#endif
