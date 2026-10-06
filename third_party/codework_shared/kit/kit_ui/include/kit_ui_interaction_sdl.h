#ifndef KIT_UI_INTERACTION_SDL_H
#define KIT_UI_INTERACTION_SDL_H
#include <SDL2/SDL.h>
#include "kit_ui_interaction.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Optional platform bridge. Map event coordinates into control space in the
 * host before calling. Resize/focus loss cancel ownership; non-left buttons,
 * unrelated keys and text entry are left with the host. */
int kit_ui_interaction_event_from_sdl(const SDL_Event *event,
                                     KitUiInteractionEvent *out_event);
void kit_ui_interaction_sdl_draw_focus(SDL_Renderer *renderer,
                                       const KitUiInteractionContext *ctx,
                                       const KitUiInteractionControl *controls,
                                       uint32_t count,
                                       KitRenderColor color);
#ifdef __cplusplus
}
#endif
#endif
