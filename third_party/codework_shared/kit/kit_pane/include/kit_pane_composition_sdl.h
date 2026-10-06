#ifndef KIT_PANE_COMPOSITION_SDL_H
#define KIT_PANE_COMPOSITION_SDL_H
#include "kit_pane_composition.h"
#include <SDL.h>
typedef struct KitPaneSdlClip { SDL_Rect previous; int enabled; } KitPaneSdlClip;
/* Optional adapter, outside the generic archive. Begin intersects the existing
 * SDL clip; end restores it. Renderer coordinate scale remains host-owned. */
CoreResult kit_pane_content_begin_sdl(SDL_Renderer *renderer,const KitPaneCompositionEntry *pane,KitPaneSdlClip *saved);
CoreResult kit_pane_content_end_sdl(SDL_Renderer *renderer,const KitPaneSdlClip *saved);
#endif
