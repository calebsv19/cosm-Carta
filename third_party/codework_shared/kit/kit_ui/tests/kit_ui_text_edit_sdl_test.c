#include "kit_ui_text_edit_sdl.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
/* Deterministic clipboard fixture: never writes the user's system clipboard. */
static char clipboard[256];
static int publication_fail, retrieval_fail;
char *SDL_GetClipboardText(void) { return retrieval_fail?NULL:SDL_strdup(clipboard); }
int SDL_SetClipboardText(const char *s) { if(publication_fail)return -1;snprintf(clipboard,sizeof(clipboard),"%s",s);return 0; }
int main(void) {
    char text[32]="abc";KitUiTextEdit e={0};kit_ui_text_bind(&e,text,sizeof(text),1);SDL_Event event={0};
    event.type=SDL_TEXTEDITING;strcpy(event.edit.text,"候補");event.edit.length=2;
    assert(kit_ui_text_event_sdl(&e,&event).consumed && strcmp(text,"abc")==0);
    event=(SDL_Event){0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;
    assert(!kit_ui_text_event_sdl(&e,&event).submit);
    event.key.keysym.sym=SDLK_ESCAPE;assert(!kit_ui_text_event_sdl(&e,&event).cancel && !e.composition[0]);
    assert(kit_ui_text_event_sdl(&e,&event).cancel);
    event.key.keysym.sym=SDLK_RETURN;assert(kit_ui_text_event_sdl(&e,&event).submit);
    event.key.repeat=1;assert(!kit_ui_text_event_sdl(&e,&event).submit);event.key.repeat=0;
    event.type=SDL_TEXTINPUT;strcpy(event.text.text,"é");assert(kit_ui_text_event_sdl(&e,&event).changed && strcmp(text,"abcé")==0);
    event=(SDL_Event){0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_LEFT;event.key.keysym.mod=KMOD_SHIFT;
    kit_ui_text_event_sdl(&e,&event);assert(e.cursor==3 && e.anchor==5);
    event.key.keysym.sym=SDLK_BACKSPACE;event.key.keysym.mod=0;kit_ui_text_event_sdl(&e,&event);assert(strcmp(text,"abc")==0);
    e.flags=0;event.key.keysym.sym=SDLK_RETURN;assert(kit_ui_text_event_sdl(&e,&event).changed && strcmp(text,"abc\n")==0);
    strcpy(text,"aé😀z");kit_ui_text_bind(&e,text,sizeof(text),KIT_UI_TEXT_SINGLE_LINE);
    kit_ui_text_position(&e,7,1);event=(SDL_Event){0};event.type=SDL_KEYDOWN;event.key.keysym.mod=KMOD_GUI;event.key.keysym.sym=SDLK_c;
    assert(!kit_ui_text_event_sdl(&e,&event).changed && strcmp(clipboard,"é😀")==0);
    event.key.keysym.sym=SDLK_x;publication_fail=1;
    assert(kit_ui_text_event_sdl(&e,&event).status.code==CORE_ERR_IO && strcmp(text,"aé😀z")==0 && e.anchor==1);
    publication_fail=0;assert(kit_ui_text_event_sdl(&e,&event).changed && strcmp(text,"az")==0);
    event.key.keysym.sym=SDLK_v;assert(kit_ui_text_event_sdl(&e,&event).changed && strcmp(text,"aé😀z")==0);
    event.key.repeat=1;assert(!kit_ui_text_event_sdl(&e,&event).changed);event.key.repeat=0;
    strcpy(clipboard,"\xc0\xaf");assert(kit_ui_text_event_sdl(&e,&event).status.code==CORE_ERR_INVALID_ARG && strcmp(text,"aé😀z")==0);
    strcpy(clipboard,"\n");assert(kit_ui_text_event_sdl(&e,&event).status.code==CORE_ERR_INVALID_ARG);
    memset(clipboard,'x',40);clipboard[40]=0;assert(kit_ui_text_event_sdl(&e,&event).status.code==CORE_ERR_OUT_OF_MEMORY && strcmp(text,"aé😀z")==0);
    retrieval_fail=1;assert(kit_ui_text_event_sdl(&e,&event).status.code==CORE_ERR_IO);retrieval_fail=0;
    kit_ui_text_position(&e,7,1);assert(kit_ui_text_selection(&e,text,sizeof(text)).code==CORE_ERR_INVALID_ARG && strcmp(text,"aé😀z")==0);
    assert(kit_ui_text_compose(&e,"候補",3,0).code==CORE_ERR_INVALID_ARG);
    assert(kit_ui_text_compose(&e,"候補",1,2).code==CORE_ERR_INVALID_ARG);
    puts("SDL normalization, IME isolation, host intent and repeat policy pass");
    puts("Clipboard fixture: Unicode copy/cut/paste, publication/retrieval failure, repeat, invalid UTF8, newline and capacity refusal pass");
}
