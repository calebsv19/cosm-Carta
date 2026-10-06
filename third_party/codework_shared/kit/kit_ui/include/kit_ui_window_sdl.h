#ifndef KIT_UI_WINDOW_SDL_H
#define KIT_UI_WINDOW_SDL_H
#include <SDL.h>
#include <stdint.h>
#include "core_base.h"
/* Optional observation/coordinate adapter; hosts retain windows, event loops,
 * renderer resources, fullscreen policy and domain cancellation. */
typedef struct KitUiWindowState {
    int logical_width, logical_height, drawable_width, drawable_height;
    int display, initialized, presentable;
    Uint32 flags;
    uint64_t generation;
} KitUiWindowState;
enum {
    KIT_UI_WINDOW_GEOMETRY = 1u,
    KIT_UI_WINDOW_VISIBILITY = 2u,
    KIT_UI_WINDOW_FOCUS = 4u,
    KIT_UI_WINDOW_MODE = 8u,
    KIT_UI_WINDOW_DISPLAY = 16u
};
CoreResult kit_ui_window_refresh_sdl(KitUiWindowState *state, SDL_Window *window,
                                    uint32_t *changes);
static inline int kit_ui_window_event_invalidates_sdl(const SDL_Event *e) {
    if (!e || e->type!=SDL_WINDOWEVENT) return 0;
    switch(e->window.event) {
        case SDL_WINDOWEVENT_RESIZED: case SDL_WINDOWEVENT_SIZE_CHANGED:
        case SDL_WINDOWEVENT_MOVED: case SDL_WINDOWEVENT_DISPLAY_CHANGED:
        case SDL_WINDOWEVENT_MINIMIZED: case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED: case SDL_WINDOWEVENT_SHOWN:
        case SDL_WINDOWEVENT_HIDDEN: case SDL_WINDOWEVENT_FOCUS_LOST: return 1;
        default:return 0;
    }
}
/* Render dimensions are explicit: logical for command UI, actual renderer
 * output for SDL UI. Offscreen reference canvases may use a bounded extent.
 * Negative/outside capture points remain outside; no hit-changing clamping. */
int kit_ui_window_map_point_sdl(const KitUiWindowState *state, int render_width,
    int render_height, int window_x, int window_y, int *render_x, int *render_y);
/* F11 toggles desktop fullscreen; no event loop or domain shortcut ownership. */
int kit_ui_window_fullscreen_key_sdl(SDL_Window *window, const SDL_Event *event);
#endif
