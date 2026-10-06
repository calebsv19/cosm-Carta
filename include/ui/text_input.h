#ifndef CARTA_TEXT_INPUT_H
#define CARTA_TEXT_INPUT_H
#include "kit_ui_text_edit_sdl.h"
#include "kit_ui_text_presentation.h"
#include <stdbool.h>
struct AppState;
// Caller-owned editing and display storage; no product validation or persistence.
typedef struct CartaTextInput {
    KitUiTextEdit edit;
    KitUiTextPresentation view;
    bool suspended;
    float view_scale;
} CartaTextInput;
// Binds a field on ownership change; resetting deliberately positions at its end.
bool carta_text_bind(CartaTextInput *, char *, size_t, bool reset);
// Routes shared editing and reports app-owned submit/cancel intent.
KitUiTextEventResult carta_text_event(CartaTextInput *, const SDL_Event *);
// Cancels composition and forgets a field when its owner leaves.
void carta_text_end(CartaTextInput *);
// Draws a clipped editor with selection/preedit/caret using current font metrics.
void carta_text_draw(struct AppState *, CartaTextInput *, SDL_FRect, float);
// Places a pointer cursor inside the accepted, measured field presentation.
void carta_text_click(CartaTextInput *, float, float, bool);
// Starts a frame's visible text-owner inventory and ends stale native sessions.
void carta_text_frame_begin(struct AppState *);
void carta_text_frame_end(struct AppState *);
// Cancels native composition on focus loss, hiding or shutdown.
void carta_text_window_event(struct AppState *, const SDL_Event *);
#endif
