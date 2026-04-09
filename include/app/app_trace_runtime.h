#ifndef MAPFORGE_APP_TRACE_RUNTIME_H
#define MAPFORGE_APP_TRACE_RUNTIME_H

#include <stdbool.h>

struct AppState;

bool app_trace_session_start(struct AppState *app);
void app_trace_emit_frame_samples(struct AppState *app, double rel_time_s);
void app_trace_emit_queue_markers(struct AppState *app, double rel_time_s);
void app_trace_shutdown(struct AppState *app);

#endif
