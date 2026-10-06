#include "ui/text_input.h"
#include <string.h>

bool carta_text_bind(CartaTextInput *input, char *text, size_t capacity, bool reset) {
    if (!input || !text) return false;
    if (reset) { memset(&input->edit,0,sizeof(input->edit)); input->suspended=false; }
    if (kit_ui_text_bind(&input->edit,text,capacity,KIT_UI_TEXT_SINGLE_LINE).code==CORE_OK) return true;
    carta_text_end(input);return false;
}
KitUiTextEventResult carta_text_event(CartaTextInput *input, const SDL_Event *event) {
    KitUiTextEventResult result={0};
    if (!input || !event || !input->edit.text) return result;
    if (event->type==SDL_WINDOWEVENT) {
        if (event->window.event==SDL_WINDOWEVENT_FOCUS_LOST ||
            event->window.event==SDL_WINDOWEVENT_HIDDEN) input->suspended=true;
        if (event->window.event==SDL_WINDOWEVENT_FOCUS_GAINED ||
            event->window.event==SDL_WINDOWEVENT_SHOWN) input->suspended=false;
    }
    if (input->suspended && (event->type==SDL_TEXTINPUT || event->type==SDL_TEXTEDITING ||
                             event->type==SDL_KEYDOWN)) { result.consumed=1; return result; }
    return kit_ui_text_event_sdl(&input->edit,event);
}
void carta_text_end(CartaTextInput *input) {
    if (!input) return;
    kit_ui_text_cancel_composition(&input->edit);
    input->edit.text=NULL;
    input->view.count=0;
    input->suspended=false;
}
