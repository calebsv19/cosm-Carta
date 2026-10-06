#include "kit_ui_text_presentation_sdl.h"
#include <math.h>
typedef struct Painter {SDL_Renderer *renderer;CoreResult (*text)(void *,KitRenderVec2,const char *);void *user;} Painter;
static CoreResult rectangle(void *data,KitRenderRect r,KitRenderColor c) {
    Painter *p=data;SDL_Rect bounds={(int)floorf(r.x),(int)floorf(r.y),(int)ceilf(r.x+r.width)-(int)floorf(r.x),(int)ceilf(r.y+r.height)-(int)floorf(r.y)};
    if(SDL_SetRenderDrawColor(p->renderer,c.r,c.g,c.b,c.a)!=0 || SDL_RenderFillRect(p->renderer,&bounds)!=0)
        return (CoreResult){CORE_ERR_IO,"SDL text presentation rectangle failed"};return core_result_ok();
}
static CoreResult text(void *data,KitRenderVec2 origin,const char *value) {Painter *p=data;return p->text(p->user,origin,value);}
CoreResult kit_ui_text_presentation_sdl(SDL_Renderer *renderer,const KitUiTextPresentation *p,
    const KitUiTextPresentationColors *colors,CoreResult (*draw)(void *,KitRenderVec2,const char *),void *user) {
    if(!renderer||!p||!colors||!draw)return (CoreResult){CORE_ERR_INVALID_ARG,"invalid SDL text presentation"};
    SDL_Rect old;SDL_RenderGetClipRect(renderer,&old);SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);
    SDL_BlendMode blend;SDL_GetRenderDrawBlendMode(renderer,&blend);Uint8 r,g,b,a;SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a);
    KitRenderRect v=p->options.viewport;
    if(clipped) {float x=fmaxf(v.x,old.x),y=fmaxf(v.y,old.y),right=fminf(v.x+v.width,old.x+old.w),bottom=fminf(v.y+v.height,old.y+old.h);v=(KitRenderRect){x,y,fmaxf(0,right-x),fmaxf(0,bottom-y)};}
    SDL_Rect clip={(int)ceilf(v.x),(int)ceilf(v.y),(int)floorf(v.x+v.width)-(int)ceilf(v.x),(int)floorf(v.y+v.height)-(int)ceilf(v.y)};
    if(clip.w<0)clip.w=0;if(clip.h<0)clip.h=0;
    CoreResult status=core_result_ok();
    if(SDL_RenderSetClipRect(renderer,&clip)!=0 || SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND)!=0)status=(CoreResult){CORE_ERR_IO,"SDL text presentation clip failed"};
    else {Painter context={renderer,draw,user};KitUiTextPainter painter={&context,rectangle,text};status=kit_ui_text_presentation_paint(p,colors,&painter);}
    SDL_RenderSetClipRect(renderer,clipped?&old:NULL);SDL_SetRenderDrawBlendMode(renderer,blend);SDL_SetRenderDrawColor(renderer,r,g,b,a);return status;
}
