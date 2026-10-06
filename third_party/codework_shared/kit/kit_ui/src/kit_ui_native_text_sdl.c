#include "kit_ui_native_text_sdl.h"
#include <math.h>
#include <limits.h>
CoreResult kit_ui_native_text_rect_sdl(SDL_Window *window,const KitUiTextPresentation *p,
    float width,float height,SDL_Rect *out) {
    if(!p||!isfinite(width)||!isfinite(height)||width<=0||height<=0)
        return (CoreResult){CORE_ERR_INVALID_ARG,"invalid native text viewport"};
    KitRenderRect v=p->options.viewport,c=p->caret;
    if(!isfinite(v.x)||!isfinite(v.y)||!isfinite(v.width)||!isfinite(v.height)||
       !isfinite(c.x)||!isfinite(c.y)||!isfinite(c.width)||!isfinite(c.height)||
       v.width<0||v.height<0||c.width<0||c.height<0)
        return (CoreResult){CORE_ERR_INVALID_ARG,"invalid native text geometry"};
    if(!window)window=SDL_GetKeyboardFocus();
    if(!window||!p->options.active)return core_result_ok();
    int w,h;SDL_GetWindowSize(window,&w,&h);
    if(w<=0||h<=0)return (CoreResult){CORE_ERR_INVALID_ARG,"invalid native window"};
    /* A scrolled caret may fall just outside the field. Clamp its anchor to the
     * visible field/window instead of placing native candidates off-screen. */
    double x=fmax(0,fmin(width,fmax(v.x,fmin(v.x+v.width,c.x))));
    double y=fmax(0,fmin(height,fmax(v.y,fmin(v.y+v.height,c.y))));
    double right=fmax(x,fmin(width,fmin(v.x+v.width,c.x+c.width)));
    double bottom=fmax(y,fmin(height,fmin(v.y+v.height,c.y+c.height)));
    SDL_Rect rect={(int)floor(x*w/width),(int)floor(y*h/height),0,0};
    int rx=(int)ceil(right*w/width),by=(int)ceil(bottom*h/height);
    rect.x=rect.x>=w?w-1:rect.x;rect.y=rect.y>=h?h-1:rect.y;
    rect.w=rx-rect.x;rect.h=by-rect.y;if(rect.w<1)rect.w=1;if(rect.h<1)rect.h=1;
    SDL_SetTextInputRect(&rect);if(out)*out=rect;return core_result_ok();
}
