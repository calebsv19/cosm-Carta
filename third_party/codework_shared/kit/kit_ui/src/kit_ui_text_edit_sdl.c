#include "kit_ui_text_edit_sdl.h"
#include <string.h>
KitUiTextEventResult kit_ui_text_event_sdl(KitUiTextEdit *e,const SDL_Event *event) {
    KitUiTextEventResult r={0,0,0,0,{CORE_OK,NULL}};
    if(!e||!event||!e->text)return r;
    if(event->type==SDL_TEXTINPUT) { r.consumed=1;r.status=kit_ui_text_insert(e,event->text.text);r.changed=r.status.code==CORE_OK && event->text.text[0];return r; }
    if(event->type==SDL_TEXTEDITING) { r.consumed=1;r.status=kit_ui_text_compose(e,event->edit.text,event->edit.start,event->edit.length);return r; }
    if(event->type==SDL_WINDOWEVENT && (event->window.event==SDL_WINDOWEVENT_FOCUS_LOST || event->window.event==SDL_WINDOWEVENT_HIDDEN)) { kit_ui_text_cancel_composition(e);return r; }
    if(event->type!=SDL_KEYDOWN)return r;
    SDL_Keycode key=event->key.keysym.sym;Uint16 mods=event->key.keysym.mod;
    int command=(mods&(KMOD_CTRL|KMOD_GUI))!=0,shift=(mods&KMOD_SHIFT)!=0;
    if(e->composition[0] && key!=SDLK_ESCAPE) {r.consumed=1;return r;}
    if(mods&KMOD_ALT)return r;
    if(command && (key==SDLK_c || key==SDLK_x || key==SDLK_v)) {
        r.consumed=1;
        if(event->key.repeat)return r;
        if(key==SDLK_v) { char *s=SDL_GetClipboardText();if(!s)r.status=(CoreResult){CORE_ERR_IO,"clipboard unavailable"};else { r.status=kit_ui_text_insert(e,s);r.changed=r.status.code==CORE_OK && s[0];SDL_free(s); } }
        else if(e->cursor!=e->anchor) {
            char *s=SDL_malloc(e->capacity);
            if(!s)r.status=(CoreResult){CORE_ERR_OUT_OF_MEMORY,"clipboard scratch allocation failed"};
            else { r.status=kit_ui_text_selection(e,s,e->capacity);
                if(r.status.code==CORE_OK && SDL_SetClipboardText(s)!=0)r.status=(CoreResult){CORE_ERR_IO,"clipboard publication failed"};
                if(r.status.code==CORE_OK && key==SDLK_x) {r.status=kit_ui_text_command(e,KIT_UI_TEXT_DELETE,0);r.changed=r.status.code==CORE_OK;}
                SDL_free(s);
            }
        }
        return r;
    }
    KitUiTextCommand op;
    if(command && key==SDLK_a)op=KIT_UI_TEXT_SELECT_ALL;
    else if(command && key!=SDLK_RETURN && key!=SDLK_KP_ENTER)return r;
    else switch(key) {
        case SDLK_LEFT:op=KIT_UI_TEXT_LEFT;break;case SDLK_RIGHT:op=KIT_UI_TEXT_RIGHT;break;
        case SDLK_HOME:op=KIT_UI_TEXT_HOME;break;case SDLK_END:op=KIT_UI_TEXT_END;break;
        case SDLK_BACKSPACE:op=KIT_UI_TEXT_BACKSPACE;break;case SDLK_DELETE:op=KIT_UI_TEXT_DELETE;break;
        case SDLK_ESCAPE:r.consumed=1;if(e->composition[0])kit_ui_text_cancel_composition(e);else r.cancel=!event->key.repeat;return r;
        case SDLK_RETURN:case SDLK_KP_ENTER:
            r.consumed=1;if(event->key.repeat || e->composition[0])return r;
            if(!(e->flags&KIT_UI_TEXT_SINGLE_LINE) && !command) { r.status=kit_ui_text_insert(e,"\n");r.changed=r.status.code==CORE_OK; }
            else r.submit=1;
            return r;
        default:return r;
    }
    r.consumed=1;r.status=kit_ui_text_command(e,op,shift);
    r.changed=r.status.code==CORE_OK && (op==KIT_UI_TEXT_BACKSPACE || op==KIT_UI_TEXT_DELETE);return r;
}
