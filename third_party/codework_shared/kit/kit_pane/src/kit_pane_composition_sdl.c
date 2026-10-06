#include "kit_pane_composition_sdl.h"
#include <math.h>
#include <limits.h>
CoreResult kit_pane_content_begin_sdl(SDL_Renderer *r,const KitPaneCompositionEntry *p,KitPaneSdlClip *saved) {
    if(!r||!p||!saved)return (CoreResult){CORE_ERR_INVALID_ARG,"invalid SDL pane scope"};
    CorePaneRect v=p->visible_content;
    double x=ceil(v.x),y=ceil(v.y),right=floor((double)v.x+v.width),bottom=floor((double)v.y+v.height);
    if(!isfinite(x)||!isfinite(y)||!isfinite(right)||!isfinite(bottom)||v.width<0||v.height<0||
       x<INT_MIN||x>INT_MAX||y<INT_MIN||y>INT_MAX||right<INT_MIN||right>INT_MAX||bottom<INT_MIN||bottom>INT_MAX||
       right-x>INT_MAX||bottom-y>INT_MAX)return (CoreResult){CORE_ERR_INVALID_ARG,"invalid SDL pane clip"};
    SDL_Rect clip={(int)x,(int)y,(int)fmax(0,right-x),(int)fmax(0,bottom-y)};
    saved->enabled=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&saved->previous);
    if(saved->enabled){SDL_Rect intersection={0};(void)SDL_IntersectRect(&clip,&saved->previous,&intersection);clip=intersection;}
    return SDL_RenderSetClipRect(r,&clip)==0?core_result_ok():(CoreResult){CORE_ERR_IO,"SDL pane clip failed"};
}
CoreResult kit_pane_content_end_sdl(SDL_Renderer *r,const KitPaneSdlClip *saved) {
    if(!r||!saved)return (CoreResult){CORE_ERR_INVALID_ARG,"invalid SDL pane restore"};
    return SDL_RenderSetClipRect(r,saved->enabled?&saved->previous:NULL)==0?core_result_ok():(CoreResult){CORE_ERR_IO,"SDL pane restore failed"};
}
