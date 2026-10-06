#ifndef KIT_UI_TEXT_PRESENTATION_SDL_H
#define KIT_UI_TEXT_PRESENTATION_SDL_H
#include "kit_ui_text_presentation.h"
#include <SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Optional synchronous SDL adapter. Text callback uses the enclosing clip.
 * Restores clip, color and blending before returning. */
CoreResult kit_ui_text_presentation_sdl(SDL_Renderer *, const KitUiTextPresentation *,
    const KitUiTextPresentationColors *, CoreResult (*)(void *, KitRenderVec2, const char *), void *);
#ifdef __cplusplus
}
#endif
#endif
