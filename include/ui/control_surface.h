#ifndef CARTA_UI_CONTROL_SURFACE_H
#define CARTA_UI_CONTROL_SURFACE_H
#include "kit_ui_surface.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
struct AppState;
/* Durable command domains; values identify actions independently of layout. */
enum { CARTA_CONTROL_WORKSPACE = 1u, CARTA_CONTROL_HISTORY = 2u, CARTA_CONTROL_PLACES = 3u, CARTA_CONTROL_ROUTES = 4u, CARTA_CONTROL_HISTORY_FORM = 5u };
/* Synchronizes visible geometry and cancels capture when its owner changes. */
void carta_ui_controls_sync(struct AppState *app);
/* Routes shared release/keyboard activation to existing product action bodies. */
bool carta_ui_controls_event(struct AppState *app, const SDL_Event *event);
/* Returns visible app-owned places/route geometry and executes its existing command. */
SDL_FRect carta_places_control_rect(struct AppState *,uint32_t,uint64_t);
void carta_places_control_activate(struct AppState *,uint32_t,uint64_t);
/* Draws a synchronous rounded control through the current native backend. */
void carta_ui_control_draw(struct AppState *app, SDL_FRect rect, const char *label,
                           bool selected, KitUiSurfaceKey key, float text_scale);
#endif
