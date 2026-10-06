#ifndef KIT_UI_NATIVE_TEXT_SDL_H
#define KIT_UI_NATIVE_TEXT_SDL_H
#include "kit_ui_text_presentation.h"
#include <SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Maps a clipped render-space caret to SDL window coordinates, rounding outward.
 * Host owns focus/session eligibility and Start/StopTextInput. Optional rectangle
 * update never starts a session or synthesizes text. Null focus is a no-op.
 * It anchors platform candidate UI; SDL owns native IME behavior. */
CoreResult kit_ui_native_text_rect_sdl(SDL_Window *window,
    const KitUiTextPresentation *view,float render_width,float render_height,SDL_Rect *out_rect);
#ifdef __cplusplus
}
#endif
#endif
