#include "ui/text_input.h"
#include <assert.h>
#include <string.h>
// Builds actual SDL keys against the production shared adapter.
static KitUiTextEventResult key(CartaTextInput *t,SDL_Keycode k,Uint16 mod,int repeat) {
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;e.key.keysym.mod=mod;e.key.repeat=repeat;
    return carta_text_event(t,&e);
}
static void insert(CartaTextInput *t,const char *s) {
    SDL_Event e={0};e.type=SDL_TEXTINPUT;strcpy(e.text.text,s);assert(carta_text_event(t,&e).changed);
}
int main(void) {
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO)==0);
    CartaTextInput t={0};char a[16]="Aé🙂Z",b[8]="other";
    assert(carta_text_bind(&t,a,sizeof(a),true));key(&t,SDLK_LEFT,0,0);key(&t,SDLK_BACKSPACE,0,0);
    assert(!strcmp(a,"AéZ"));key(&t,SDLK_HOME,0,0);key(&t,SDLK_RIGHT,KMOD_SHIFT,0);
    insert(&t,"B");assert(!strcmp(a,"BéZ"));
    key(&t,SDLK_a,KMOD_GUI,0);assert(!strcmp(a,"BéZ"));insert(&t,"new");assert(!strcmp(a,"new"));
    key(&t,SDLK_a,KMOD_GUI,0);key(&t,SDLK_c,KMOD_GUI,0);key(&t,SDLK_x,KMOD_GUI,0);assert(!a[0]);
    key(&t,SDLK_v,KMOD_GUI,1);assert(!a[0]);key(&t,SDLK_v,KMOD_GUI,0);assert(!strcmp(a,"new"));
    assert(carta_text_bind(&t,b,sizeof(b),true));assert(t.edit.cursor==5 && t.edit.anchor==5);
    assert(!strcmp(a,"new"));key(&t,SDLK_HOME,0,0);assert(carta_text_bind(&t,b,sizeof(b),true));assert(t.edit.cursor==5);
    SDL_Event e={0};e.type=SDL_TEXTEDITING;strcpy(e.edit.text,"候補");e.edit.length=2;
    assert(carta_text_event(&t,&e).consumed && !strcmp(b,"other"));assert(!key(&t,SDLK_RETURN,0,0).submit);
    assert(!key(&t,SDLK_ESCAPE,0,0).cancel && !t.edit.composition[0]);
    assert(!key(&t,SDLK_RETURN,0,1).submit);assert(key(&t,SDLK_RETURN,0,0).submit);
    e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;carta_text_event(&t,&e);
    e.type=SDL_TEXTINPUT;strcpy(e.text.text,"x");assert(carta_text_event(&t,&e).consumed && !strcmp(b,"other"));
    e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;carta_text_event(&t,&e);
    e.type=SDL_TEXTINPUT;strcpy(e.text.text,"toolong");assert(carta_text_event(&t,&e).status.code!=CORE_OK && !strcmp(b,"other"));
    char invalid[2]={(char)0xc3,0};assert(!carta_text_bind(&t,invalid,sizeof(invalid),true) && !t.edit.text);
    carta_text_end(&t);assert(!t.edit.text);SDL_Quit();return 0;
}
