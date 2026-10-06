#include "kit_ui_window_sdl.h"
#include <SDL_vulkan.h>
#include <limits.h>
#include <math.h>

CoreResult kit_ui_window_refresh_sdl(KitUiWindowState *s, SDL_Window *w, uint32_t *changes) {
    if (!s || !w || !changes) return (CoreResult){CORE_ERR_INVALID_ARG,"missing window state"};
    KitUiWindowState next = {0};
    SDL_GetWindowSize(w,&next.logical_width,&next.logical_height);
    next.flags=SDL_GetWindowFlags(w);
    next.display=SDL_GetWindowDisplayIndex(w);
    if (next.flags & SDL_WINDOW_VULKAN)
        SDL_Vulkan_GetDrawableSize(w,&next.drawable_width,&next.drawable_height);
    else {
        SDL_Renderer *renderer=SDL_GetRenderer(w);
        if (!renderer || SDL_GetRendererOutputSize(renderer,&next.drawable_width,&next.drawable_height)!=0) {
            next.drawable_width=next.logical_width;next.drawable_height=next.logical_height;
        }
    }
    next.presentable=next.logical_width>0 && next.logical_height>0 &&
        next.drawable_width>0 && next.drawable_height>0 &&
        !(next.flags & (SDL_WINDOW_HIDDEN|SDL_WINDOW_MINIMIZED));
    uint32_t bits=0;
    if (!s->initialized || next.logical_width!=s->logical_width || next.logical_height!=s->logical_height ||
        next.drawable_width!=s->drawable_width || next.drawable_height!=s->drawable_height) bits|=KIT_UI_WINDOW_GEOMETRY;
    if (!s->initialized || next.presentable!=s->presentable) bits|=KIT_UI_WINDOW_VISIBILITY;
    if (!s->initialized || ((next.flags^s->flags)&SDL_WINDOW_INPUT_FOCUS)) bits|=KIT_UI_WINDOW_FOCUS;
    if (!s->initialized || ((next.flags^s->flags)&(SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_MAXIMIZED))) bits|=KIT_UI_WINDOW_MODE;
    if (!s->initialized || next.display!=s->display) bits|=KIT_UI_WINDOW_DISPLAY;
    next.initialized=1;next.generation=s->generation+(bits!=0);
    *s=next;*changes=bits;return core_result_ok();
}
int kit_ui_window_map_point_sdl(const KitUiWindowState *s,int w,int h,int x,int y,int *rx,int *ry) {
    if (!s || s->logical_width<=0 || s->logical_height<=0 || w<=0 || h<=0 || !rx || !ry) return 0;
    double a=(double)x*w/s->logical_width,b=(double)y*h/s->logical_height;
    if (a<INT_MIN || a>INT_MAX || b<INT_MIN || b>INT_MAX) return 0;
    *rx=(int)lround(a);*ry=(int)lround(b);return 1;
}

int kit_ui_window_fullscreen_key_sdl(SDL_Window *w,const SDL_Event *e) {
    if(!w || !e || e->type!=SDL_KEYDOWN || e->key.keysym.sym!=SDLK_F11) return 0;
    if(e->key.repeat)return 1;
    Uint32 mode=(SDL_GetWindowFlags(w)&SDL_WINDOW_FULLSCREEN)?0:SDL_WINDOW_FULLSCREEN_DESKTOP;
    return SDL_SetWindowFullscreen(w,mode)==0?1:-1;
}
