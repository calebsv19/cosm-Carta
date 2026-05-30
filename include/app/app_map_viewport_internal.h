#ifndef MAPFORGE_APP_MAP_VIEWPORT_INTERNAL_H
#define MAPFORGE_APP_MAP_VIEWPORT_INTERNAL_H

#include "app/app_internal.h"

SDL_FRect app_map_viewport_rect(const AppState *app);
bool app_map_viewport_contains_screen_point(const AppState *app, int screen_x, int screen_y);
bool app_map_screen_to_world(const AppState *app, float screen_x, float screen_y, float *out_world_x, float *out_world_y);
bool app_map_world_to_screen(const AppState *app, float world_x, float world_y, float *out_screen_x, float *out_screen_y);
bool app_map_world_to_viewport_local(const AppState *app, float world_x, float world_y, float *out_local_x, float *out_local_y);
bool app_map_viewport_activate(AppState *app);
void app_map_viewport_deactivate(AppState *app);

#endif
