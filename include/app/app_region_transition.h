#ifndef MAPFORGE_APP_REGION_TRANSITION_H
#define MAPFORGE_APP_REGION_TRANSITION_H
#include "app/app_internal.h"
// Starts one background map stage, refusing competing transitions or unfinished personal edits.
bool app_region_transition_start(AppState *app, const RegionInfo *info, int index);
// Integrates bounded owner-thread GPU work and publishes only a complete valid stage.
void app_region_transition_tick(AppState *app);
// Cancels staging through Escape or the visible Cancel button without blocking the map.
bool app_region_transition_handle_event(AppState *app, const SDL_Event *event);
// Shares current Cancel geometry and its direct product intent with semantic UI.
SDL_FRect app_region_transition_cancel_rect(const AppState *app);
void app_region_transition_cancel(AppState *app);
// Draws progress and cancellation above the current map using the existing theme and font.
void app_region_transition_draw(AppState *app);
// Cancels and joins private staging before application renderer teardown.
void app_region_transition_shutdown(AppState *app);
#endif
