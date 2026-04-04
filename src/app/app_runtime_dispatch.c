#include "app/app_internal.h"

#include <string.h>

void app_runtime_dispatch_frame(AppState *app,
                                double *io_last_time,
                                RendererBackend *io_last_backend,
                                AppRuntimeDispatchFrame *out_frame) {
    AppRuntimeDispatchFrame local = {0};
    if (!out_frame) {
        out_frame = &local;
    }
    memset(out_frame, 0, sizeof(*out_frame));

    if (!app || !io_last_time || !io_last_backend) {
        return;
    }

    app_runtime_process_input_frame(app,
                                    &out_frame->input,
                                    &out_frame->frame_begin,
                                    &out_frame->after_events);
    if (app_runtime_handle_global_controls(app)) {
        out_frame->skipped_for_global_controls = true;
        return;
    }

    app_runtime_update_frame(app,
                             io_last_time,
                             &out_frame->dt,
                             &out_frame->after_update,
                             &out_frame->after_queue,
                             &out_frame->after_integrate,
                             &out_frame->after_route);

    AppVisibleTileRenderStats tile_stats = {0};
    app_runtime_render_frame(app,
                             io_last_backend,
                             &out_frame->input,
                             &tile_stats,
                             &out_frame->after_render_derive,
                             &out_frame->render_draw_pass_count,
                             &out_frame->render_invalidation_reason_bits,
                             &out_frame->before_present,
                             &out_frame->after_render);
    out_frame->visible_tiles = tile_stats.visible_tiles;
    out_frame->loading_expected = tile_stats.loading_expected;
    out_frame->loading_done = tile_stats.loading_done;
    out_frame->vk_asset_misses = tile_stats.vk_asset_misses;

    app->tile_state_bridge.loading_expected = tile_stats.loading_expected;
    app->tile_state_bridge.loading_done = tile_stats.loading_done;
    app->tile_state_bridge.vk_asset_misses = tile_stats.vk_asset_misses;
    if (tile_stats.loading_expected > 0 && tile_stats.loading_done == 0) {
        app->tile_state_bridge.loading_no_data_time += out_frame->dt;
    } else {
        app->tile_state_bridge.loading_no_data_time = 0.0f;
    }
}
