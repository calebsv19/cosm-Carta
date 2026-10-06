#ifndef CARTA_MODAL_CONTROLS_H
#define CARTA_MODAL_CONTROLS_H
#include "ui/control_surface.h"
// One surface per modal; inventory changes cancel outstanding activation.
void carta_modal_controls_begin(KitUiSurface *,unsigned scope);
void carta_modal_controls_add(KitUiSurface *,uint64_t action,SDL_FRect,SDL_FRect,bool enabled);
void carta_modal_controls_end(KitUiSurface *);
bool carta_modal_controls_event(KitUiSurface *,const SDL_Event *,uint64_t *action,uint32_t *modifiers);
void carta_ui_control_draw_surface(struct AppState *,KitUiSurface *,SDL_FRect,const char *,bool,KitUiSurfaceKey,float);
#endif
