#include "app/app_s5_diagnostics.h"
#include "app/history_ui.h"
#include "kit_ui_native_text_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Finite opt-in replay uses a disposable repository and never submits this draft.
void app_s5_text_probe(AppState *app,unsigned phase) {
    if(!getenv("MAPFORGE_S5_TEXT"))return;
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;
    if(phase==7){history_ui_event(app,&e);return;}
    if(phase!=6)return;
    history_ui_event(app,&e);history_ui_event(app,&e);
    SDL_FRect import=history_ui_action_rect(app,0);
    e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=import.x+4;e.button.y=import.y+4;history_ui_event(app,&e);
    e.type=SDL_MOUSEBUTTONUP;history_ui_event(app,&e);
    e=(SDL_Event){0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_TAB;history_ui_event(app,&e);
    SDL_RaiseWindow(app->window);
    for(unsigned i=0;i<12;i++) {
        e.type=SDL_TEXTINPUT;strcpy(e.text.text,"Café trail ");history_ui_event(app,&e);
    }
    e=(SDL_Event){0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_LEFT;e.key.keysym.mod=KMOD_SHIFT;
    history_ui_event(app,&e);history_ui_event(app,&e);
    e.type=SDL_TEXTEDITING;strcpy(e.edit.text,"候補");e.edit.start=0;e.edit.length=2;
    history_ui_event(app,&e);
}
// Records actual accepted presentation/native session mapping after a rendered frame.
void app_s5_text_report(AppState *app) {
    static int reported;
    if(reported || !getenv("MAPFORGE_S5_TEXT"))return;
    CartaTextInput *owner=app->ui_state_bridge.native_text_owner;
    if(!owner || !owner->edit.composition[0] || !owner->view.count)return;
    SDL_Rect rect={0};CoreResult result=kit_ui_native_text_rect_sdl(app->window,&owner->view,app->width,app->height,&rect);
    fprintf(stderr,"[s5-text] native_active=%d map_status=%d rect=%d,%d,%d,%d capacity=%zu bytes=%zu preedit=%zu scope=%u\n",
        SDL_IsTextInputActive(),result.code,rect.x,rect.y,rect.w,rect.h,owner->edit.capacity,
        strlen(owner->edit.text),strlen(owner->edit.composition),app->ui_state_bridge.controls.scope);
    reported=1;
}
