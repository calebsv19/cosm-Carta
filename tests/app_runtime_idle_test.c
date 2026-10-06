#include "app/app_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AppRuntimeInputFrame test_input;
static TileLoaderStats loader_stats;
static VkPolyPrepStats prep_stats;
static unsigned update_calls, render_calls;
static double update_previous_time;

// Supplies a deterministic asynchronous loader snapshot.
void tile_loader_get_stats(TileLoader *loader, TileLoaderStats *stats) {
    (void)loader; *stats = loader_stats;
}
// Supplies deterministic worker completion and in-flight state.
void app_vk_poly_prep_get_stats(AppState *app, VkPolyPrepStats *stats) {
    (void)app; *stats = prep_stats;
}
// Matches enabled test layers without loading a geographic region.
bool app_layer_active_runtime(const AppState *app, TileLayerKind kind) {
    return app->view_state_bridge.layer_user_enabled[kind];
}
// Supplies a maintenance wake or native event without a real window.
void app_runtime_process_input_frame(AppState *app, AppRuntimeInputFrame *input,
                                    double *begin, double *after) {
    (void)app; *input = test_input; *begin = 9.0; *after = 10.0;
}
// Keeps unrelated global controls inert for dispatch tests.
bool app_runtime_handle_global_controls(AppState *app) { (void)app; return false; }
// Counts update execution without modifying application resources.
void app_runtime_update_frame(AppState *app, double *last, float *dt,
                              double *a, double *b, double *c, double *d) {
    (void)app; ++update_calls; update_previous_time = *last; *last = 10.0; *dt = 0.01f;
    *a = *b = *c = *d = 10.01;
}
// Counts actual submissions and produces nonzero retained loading state.
void app_runtime_render_frame(AppState *app, RendererBackend *backend,
                              const AppRuntimeInputFrame *input, AppVisibleTileRenderStats *stats,
                              double *derive, uint32_t *passes, uint32_t *reasons,
                              double *before, double *after) {
    (void)app; (void)backend; (void)input; ++render_calls;
    stats->loading_expected = 3; stats->loading_done = 3;
    *derive = *before = *after = 10.02; *passes = 1; *reasons = 0;
}
// Exercises idle outside coverage, input, animation, deadlines and worker wake paths.
int main(void) {
    AppState *app = calloc(1, sizeof(*app)); assert(app);
    double last = 1.0; RendererBackend backend = 0; AppRuntimeDispatchFrame frame;
    assert(app_runtime_compute_wait_timeout_ms(app, 10.0) == 0);
    app_runtime_dispatch_frame(app, &last, &backend, &frame);
    assert(!frame.skipped_for_idle && render_calls == 1 && app->runtime_frame_presented);
    // Invalid street bounds after first presentation must settle, including hiking views.
    assert(!app->tile_state_bridge.queue_valid && !app->tile_state_bridge.visible_valid);
    assert(app_runtime_compute_wait_timeout_ms(app, 10.0) == 1000);
    app_runtime_dispatch_frame(app, &last, &backend, &frame);
    assert(frame.skipped_for_idle && render_calls == 1 && update_calls == 1 && last == 10.0);
    assert(app->tile_state_bridge.loading_done == 3); // Idle cannot erase retained metrics.
    // Consumed UI events and exposure/resize still require a draw even with no normalized action.
    last = 9.0;
    test_input.raw.wait_call_count = 1;
    test_input.raw.wait_blocked_ms = 1000.0;
    test_input.raw.sdl_event_count = 1;
    app_runtime_dispatch_frame(app, &last, &backend, &frame);
    assert(!frame.skipped_for_idle && render_calls == 2);
    assert(update_previous_time == 10.0); // Wake input cannot fast-forward the camera by a second.
    memset(&test_input, 0, sizeof(test_input));
    app->runtime_redraw_requested = true;
    assert(app_runtime_compute_wait_timeout_ms(app, 10.0) == 0);
    app_runtime_dispatch_frame(app, &last, &backend, &frame);
    assert(!frame.skipped_for_idle && !app->runtime_redraw_requested && render_calls == 3);
    app->view_state_bridge.camera.x_target = 20;
    assert(app_runtime_compute_wait_timeout_ms(app, 10.0) == 0);
    app->view_state_bridge.camera.x_target = 0;
    app->route_state_bridge.playback_playing = true;
    assert(app_runtime_frame_needed(app, &test_input, 10.0));
    app->route_state_bridge.playback_playing = false;
    app->terrain_pending = true; assert(app_runtime_frame_needed(app, &test_input, 10.0));
    app->terrain_pending = false;
    app->history_pending=true;app->ui_state_bridge.utility_workspace=2;
    assert(app_runtime_compute_wait_timeout_ms(app,10.0)==0);
    app->ui_state_bridge.utility_workspace=0;assert(app_runtime_compute_wait_timeout_ms(app,10.0)==1000);
    app->history_pending=false;
    app->route_state_bridge.route_recompute_scheduled = true;
    app->route_state_bridge.route_recompute_due_time = 10.125;
    assert(app_runtime_compute_wait_timeout_ms(app, 10.0) == 125);
    assert(app_runtime_frame_needed(app, &test_input, 10.125));
    app->route_state_bridge.route_recompute_scheduled = false;
    app->lifetime.tile_loader_initialized = true; loader_stats.res_count = 1;
    assert(app_runtime_compute_wait_timeout_ms(app, 10.0) == 8);
    app_runtime_dispatch_frame(app, &last, &backend, &frame);
    assert(!frame.skipped_for_idle && render_calls == 4);
    loader_stats.res_count = 0; app->lifetime.tile_loader_initialized = false;
    app->view_state_bridge.layer_user_enabled[0] = true;
    app->tile_state_bridge.tile_queues[0].count = 1;
    assert(app_runtime_has_background_work(app));
    app->tile_state_bridge.tile_queues[0].index = 1;
    assert(!app_runtime_has_background_work(app));
    app->lifetime.vk_poly_prep_initialized = true; prep_stats.busy = true;
    assert(app_runtime_has_background_work(app)); // Worker has popped a job but not finished.
    prep_stats.busy = false; prep_stats.enqueued_count = 1;
    assert(!app_runtime_has_background_work(app)); // Canceled queue history must not keep polling forever.
    app->ingest_import_running = true;
    assert(app_runtime_frame_needed(app, &test_input, 10.0));
    app->ingest_import_running = false;
    assert(app_runtime_compute_wait_timeout_ms(app, 10.0) == 1000);
    free(app); puts("app_runtime_idle_test: success"); return 0;
}
