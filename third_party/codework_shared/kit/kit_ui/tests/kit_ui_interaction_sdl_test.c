#include "kit_ui_interaction_sdl.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    SDL_Event raw = {0};
    KitUiInteractionEvent event;
    raw.type = SDL_MOUSEBUTTONDOWN; raw.button.button = SDL_BUTTON_RIGHT;
    assert(!kit_ui_interaction_event_from_sdl(&raw,&event));
    raw.button.button = SDL_BUTTON_LEFT; raw.button.x = 16; raw.button.y = 20;
    assert(kit_ui_interaction_event_from_sdl(&raw,&event) && event.x == 16 && event.y == 20);
    raw.type = SDL_KEYDOWN; raw.key.keysym.sym = SDLK_TAB; raw.key.keysym.mod = KMOD_SHIFT;
    assert(kit_ui_interaction_event_from_sdl(&raw,&event) && event.modifiers == KIT_UI_INTERACTION_MOD_SHIFT);
    raw.key.keysym.sym = SDLK_KP_ENTER;
    assert(kit_ui_interaction_event_from_sdl(&raw,&event) && event.key == KIT_UI_INTERACTION_KEY_ENTER);
    raw.type = SDL_WINDOWEVENT; raw.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    assert(kit_ui_interaction_event_from_sdl(&raw,&event) && event.type == KIT_UI_INTERACTION_CANCEL);
    raw.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    assert(kit_ui_interaction_event_from_sdl(&raw,&event) && event.type == KIT_UI_INTERACTION_CANCEL);
    raw.type = SDL_TEXTINPUT;
    assert(!kit_ui_interaction_event_from_sdl(&raw,&event));
    KitUiInteractionControl control = {1u,{10,10,50,24},1};
    KitUiInteractionContext ctx = {.focused_id=1u};
    for (int scale=1; scale<=2; ++scale) {
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0,80*scale,48*scale,32,SDL_PIXELFORMAT_RGBA32);
        assert(surface);
        SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
        assert(renderer);
        SDL_SetRenderDrawColor(renderer,0,0,0,255); SDL_RenderClear(renderer);
        SDL_RenderSetScale(renderer,(float)scale,(float)scale);
        SDL_SetRenderDrawColor(renderer,1,2,3,4);
        kit_ui_interaction_sdl_draw_focus(renderer,&ctx,&control,1,(KitRenderColor){80,180,255,255});
        Uint8 r,g,b,a;
        SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a);
        assert(r==1 && g==2 && b==3 && a==4);
        SDL_RenderPresent(renderer);
        Uint32 pixel=*((Uint32*)((char*)surface->pixels + 31*scale*surface->pitch)+20*scale);
        SDL_GetRGBA(pixel,surface->format,&r,&g,&b,&a);
        assert(r==80 && g==180 && b==255 && a==255);
        SDL_DestroyRenderer(renderer); SDL_FreeSurface(surface);
    }
    puts("kit_ui SDL interaction: normalization and real 1x/2x focus pixels pass");
    return 0;
}
