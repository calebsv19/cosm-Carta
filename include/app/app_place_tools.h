#ifndef MAPFORGE_APP_PLACE_TOOLS_H
#define MAPFORGE_APP_PLACE_TOOLS_H
#include "app/app_internal.h"
// Returns whether a native personal-data dialog owns input and guards external mutations.
bool app_place_tools_active(void);
// Opens details, saved routes or recovery controls for the current personal repository.
void app_place_tools_open(AppState *app, unsigned kind);
// Releases dialog snapshots and text-input ownership.
void app_place_tools_close(void);
// Consumes dialog input and location-toolbar actions before map interaction.
bool app_place_tools_handle_event(AppState *app, const SDL_Event *event);
// Draws the native location toolbar and final modal overlay using existing theme/font adapters.
void app_place_tools_draw_toolbar(AppState *app);
void app_place_tools_draw(AppState *app);
// Opens a selected saved route through the common query/activation boundary.
void app_places_open_route(AppState *app, const char *route_id);
#endif
