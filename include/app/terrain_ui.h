#ifndef MAPFORGE_TERRAIN_UI_H
#define MAPFORGE_TERRAIN_UI_H
#include <SDL.h>
#include <stdbool.h>
struct AppState;
// Names the existing terrain panel controls for native input proof and shared hit geometry.
typedef enum TerrainUIControl {
    TERRAIN_UI_CLOSE, TERRAIN_UI_PREVIOUS, TERRAIN_UI_NEXT, TERRAIN_UI_RELOAD,
    TERRAIN_UI_SHOW, TERRAIN_UI_REMOVE, TERRAIN_UI_CONFIRM, TERRAIN_UI_CANCEL
} TerrainUIControl;
// Returns the actual visible terrain-control rectangle for input and diagnostics.
SDL_FRect terrain_ui_control_rect(const struct AppState *app,TerrainUIControl type,int row);
// Cancels outstanding controls while preserving the pending confirmation.
void terrain_ui_cancel_transient(void);
// Clears modal presentation state without touching background files or personal records.
void terrain_ui_close(void);
// Opens the native terrain coverage/storage panel without editing personal history.
void terrain_ui_open(void);
// Reports modal input ownership so background map gestures cannot leak through.
bool terrain_ui_active(void);
// Routes panel navigation and explicit two-step removal actions.
bool terrain_ui_event(struct AppState *app, const SDL_Event *event);
// Draws bounded terrain area sizes, protected baseline and removal confirmation.
void terrain_ui_draw(struct AppState *app);
#endif
