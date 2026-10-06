#ifndef KIT_UI_TEXT_EDIT_SDL_H
#define KIT_UI_TEXT_EDIT_SDL_H
#include "kit_ui_text_edit.h"
#include <SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct KitUiTextEventResult { int consumed, changed, submit, cancel; CoreResult status; } KitUiTextEventResult;
/* Route only to the current text owner. Enter/Escape intent has no app effect.
 * Host owns SDL_Start/StopTextInput and IME rectangle/window lifecycle. */
KitUiTextEventResult kit_ui_text_event_sdl(KitUiTextEdit *edit,const SDL_Event *event);
#ifdef __cplusplus
}
#endif
#endif
