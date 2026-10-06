#include "kit_ui_native_text_sdl.h"
#include "kit_ui_text_edit_sdl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_Window *window=SDL_CreateWindow("native text contract",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,400,300,SDL_WINDOW_HIDDEN);
    assert(window);SDL_StartTextInput();assert(SDL_IsTextInputActive());
    KitUiTextPresentation p={0};p.options.active=1;p.options.viewport=(KitRenderRect){40,40,200,40};p.caret=(KitRenderRect){100.5f,40,1,40};
    SDL_Rect rect={0};assert(kit_ui_native_text_rect_sdl(window,&p,800,600,&rect).code==CORE_OK);
    assert(rect.x==50&&rect.y==20&&rect.w==1&&rect.h==20);
    p.caret.x=900;assert(kit_ui_native_text_rect_sdl(window,&p,800,600,&rect).code==CORE_OK&&rect.x==120);
    assert(kit_ui_native_text_rect_sdl(window,&p,0,600,&rect).code!=CORE_OK);
    p.options.active=0;SDL_Rect before=rect;assert(kit_ui_native_text_rect_sdl(window,&p,800,600,&rect).code==CORE_OK&&!memcmp(&before,&rect,sizeof(rect)));
    SDL_StopTextInput();assert(!SDL_IsTextInputActive());p.options.active=1;
    assert(kit_ui_native_text_rect_sdl(window,&p,800,600,&rect).code==CORE_OK&&!SDL_IsTextInputActive());
    SDL_DestroyWindow(window);SDL_Quit();puts("native SDL text bridge: real window, session lifecycle, 2x mapping, clipping, inactive no-op passed; OS IME acceptance not certified");return 0;
}
