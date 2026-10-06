#ifndef MAPFORGE_HISTORY_UI_H
#define MAPFORGE_HISTORY_UI_H
#include "app/app_internal.h"
// Handles native history browser, import forms and reversible archive controls.
bool history_ui_event(AppState *app, const SDL_Event *event);
// Draws segment-preserving history geometry inside the active map viewport.
void history_ui_map_draw(AppState *app);
// Draws the responsive history browser and explicit preview/import forms.
void history_ui_panel_draw(AppState *app);
// Restores a selected hike or collection view after street-region startup initializes its camera.
void history_ui_restore_view(AppState *app);
// Releases process-owned history display assets during app shutdown.
void history_ui_shutdown(void);
// Returns only currently drawable semantic action geometry.
SDL_FRect history_ui_control_rect(AppState *app, unsigned action);
// Preserves app action bodies behind shared activation.
void history_ui_activate(AppState *app, unsigned action);
// Exposes current modal action geometry and its existing preview/type/save policy.
SDL_FRect history_ui_form_control_rect(AppState *,unsigned);
void history_ui_form_control_activate(AppState *,unsigned);
// Reports whether text/modal input must suppress map motion.
bool history_ui_modal(void);
// Returns a visible history control rectangle for diagnostics and pointer tests.
SDL_FRect history_ui_action_rect(AppState *app, int action);
// Labels peak markers within the visible hiking map.
void history_ui_peak_labels_draw(AppState *app);
// Reports whether a personal record passes the active list and collection filter.
bool history_ui_record_visible(AppState *app, struct json_object *record);
// Reports whether a personal record should appear as a map overlay.
bool history_ui_overlay_visible(AppState *app, struct json_object *record);
// Exposes the neutral geographic-name preference independently of personal history overlays.
bool history_ui_geographic_labels_enabled(AppState *app);
#endif
