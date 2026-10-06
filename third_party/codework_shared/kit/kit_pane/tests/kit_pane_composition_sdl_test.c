#include "kit_pane_composition_sdl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
 for(int scale=1;scale<=2;++scale) {
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,200*scale,100*scale,32,SDL_PIXELFORMAT_RGBA32);assert(s);
    SDL_Renderer *r=SDL_CreateSoftwareRenderer(s);assert(r);SDL_RenderSetScale(r,(float)scale,(float)scale);
    SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
    KitPaneCompositionSpec spec={8,{0,0,100,100},1,20,4,1};KitPaneComposition view={0};
    assert(kit_pane_composition_build(&view,&spec,1,(CorePaneRect){0,0,200,100}).code==CORE_OK);
    SDL_Rect parent={10,10,180,60};SDL_RenderSetClipRect(r,&parent);KitPaneSdlClip saved;
    assert(kit_pane_content_begin_sdl(r,&view.entries[0],&saved).code==CORE_OK);
    SDL_SetRenderDrawColor(r,255,0,0,255);SDL_Rect oversized={0,0,200,100};SDL_RenderFillRect(r,&oversized);
    assert(kit_pane_content_end_sdl(r,&saved).code==CORE_OK);SDL_Rect restored;SDL_RenderGetClipRect(r,&restored);
    assert(SDL_RenderIsClipEnabled(r)&&!memcmp(&restored,&parent,sizeof(parent)));SDL_RenderPresent(r);
    for(int y=0;y<s->h;++y)for(int x=0;x<s->w;++x){Uint32 value;memcpy(&value,(char*)s->pixels+y*s->pitch+x*4,4);Uint8 red,g,b,a;SDL_GetRGBA(value,s->format,&red,&g,&b,&a);
        int in=x>=10*scale&&x<95*scale&&y>=25*scale&&y<70*scale;assert(red==(in?255:0));}
    SDL_RenderSetClipRect(r,NULL);assert(kit_pane_content_begin_sdl(r,&view.entries[0],&saved).code==CORE_OK);
    assert(kit_pane_content_end_sdl(r,&saved).code==CORE_OK&&!SDL_RenderIsClipEnabled(r));
    SDL_DestroyRenderer(r);SDL_FreeSurface(s);
 }
 puts("SDL pane composition: exact content/header/neighbor exclusion at 1x and 2x, nested clip restoration passed");return 0;
}
