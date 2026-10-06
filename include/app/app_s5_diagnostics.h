#ifndef MAPFORGE_APP_S5_DIAGNOSTICS_H
#define MAPFORGE_APP_S5_DIAGNOSTICS_H
struct AppState;
// Replays a fixed public-area navigation corpus only in explicit S5 diagnostic mode.
void app_s5_update(struct AppState *app, double now);
// Requests bounded native captures before presentation.
void app_s5_capture(struct AppState *app);
// Writes bounded per-frame measurements for the explicit diagnostic session.
void app_s5_measure(struct AppState *app);
// Explicit diagnostic-only text draft and native caret report; no routine effect.
void app_s5_text_probe(struct AppState *,unsigned phase);
void app_s5_text_report(struct AppState *);
#endif
