#include "app/app_internal.h"

#include "core/time.h"

void app_runtime_begin_frame(AppState *app, double *out_frame_begin, double *out_after_events) {
    if (!app) {
        if (out_frame_begin) {
            *out_frame_begin = 0.0;
        }
        if (out_after_events) {
            *out_after_events = 0.0;
        }
        return;
    }

    double frame_begin = time_now_seconds();
    memset(&app->frame_timings, 0, sizeof(app->frame_timings));
    input_begin_frame(&app->ui_state_bridge.input);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        input_handle_event(&app->ui_state_bridge.input, &event);
    }
    app_bridge_sync_to_legacy(app);
    app_bridge_sync_from_legacy(app);
    double after_events = time_now_seconds();

    if (out_frame_begin) {
        *out_frame_begin = frame_begin;
    }
    if (out_after_events) {
        *out_after_events = after_events;
    }
}
