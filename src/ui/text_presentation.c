#include "ui/text_input.h"
#include "ui/font.h"
#include "app/app_ui_internal.h"
#include "kit_ui_native_text_sdl.h"
#include <math.h>

// One logical font scale is shared by measurement, hit testing and painting.
typedef struct TextPainter { AppState *app; SDL_FRect clip; float scale; } TextPainter;
static CoreResult measure(void *user,const char *text,float *width) {
    *width=ui_measure_text_width(text,*(float *)user);return core_result_ok();
}
static CoreResult paint_rect(void *user,KitRenderRect r,KitRenderColor color) {
    TextPainter *p=user;SDL_FRect box={r.x,r.y,r.width,r.height},clipped;
    if (SDL_IntersectFRect(&box,&p->clip,&clipped)) {
        renderer_set_draw_color(&p->app->renderer,color.r,color.g,color.b,color.a);
        renderer_fill_rect(&p->app->renderer,&clipped);
    }
    return core_result_ok();
}
static CoreResult paint_text(void *user,KitRenderVec2 origin,const char *text) {
    TextPainter *p=user;
    Renderer *renderer=&p->app->renderer;
    bool previous=renderer->viewport_enabled;
    int x=renderer->viewport_x,y=renderer->viewport_y,w=renderer->width,h=renderer->height;
    if (!renderer_set_viewport(renderer,(previous?x:0)+(int)p->clip.x,
        (previous?y:0)+(int)p->clip.y,(int)ceilf(p->clip.w),(int)ceilf(p->clip.h)))
        return (CoreResult){CORE_ERR_INVALID_ARG,"text clip unavailable"};
    // This text bridge takes absolute coordinates; viewport only supplies its crop.
    ui_draw_text_clipped(renderer,(int)origin.x+(previous?x:0),(int)origin.y+(previous?y:0),text,
        app_ui_theme_palette().text_primary,p->scale,(int)fmaxf(1,p->clip.x+p->clip.w-origin.x));
    if (previous) (void)renderer_set_viewport(renderer,x,y,w,h);
    else renderer_reset_viewport(renderer);
    return core_result_ok();
}
void carta_text_draw(AppState *app,CartaTextInput *input,SDL_FRect box,float scale) {
    if (!app || !input || !input->edit.text || box.w<=4 || box.h<=4) return;
    float height=ui_font_line_height(scale);
    if (height>box.h-4 && height>0) {scale*=(box.h-4)/height;height=ui_font_line_height(scale);}
    SDL_FRect viewport={box.x+6,box.y+fmaxf(2,(box.h-height)/2),fmaxf(1,box.w-12),height};
    MapForgeThemePalette palette=app_ui_theme_palette();
    renderer_set_draw_color(&app->renderer,palette.button_outline.r,palette.button_outline.g,palette.button_outline.b,255);
    renderer_fill_rounded_rect(&app->renderer,&box,kit_ui_corner_radius_clamp(6,box.w,box.h));
    SDL_FRect inner={box.x+1,box.y+1,box.w-2,box.h-2};
    renderer_set_draw_color(&app->renderer,palette.button_fill.r,palette.button_fill.g,palette.button_fill.b,255);
    renderer_fill_rounded_rect(&app->renderer,&inner,5);
    KitUiTextPresentationOptions options={.viewport={viewport.x,viewport.y,viewport.w,viewport.h},
        .line_height=height,.caret_width=1,.active=!input->suspended,.reveal_caret=1};
    if (kit_ui_text_presentation_build(&input->view,&input->edit,&options,measure,&scale).code!=CORE_OK) return;
    input->view_scale=scale;
    TextPainter context={app,viewport,scale};
    KitUiTextPainter painter={&context,paint_rect,paint_text};
    KitUiTextPresentationColors colors={{palette.button_active_primary.r,palette.button_active_primary.g,palette.button_active_primary.b,100},
        {palette.text_primary.r,palette.text_primary.g,palette.text_primary.b,255},
        {palette.text_primary.r,palette.text_primary.g,palette.text_primary.b,255}};
    // Paint is synchronous; caller-owned view also remains valid for hit testing.
    (void)kit_ui_text_presentation_paint(&input->view,&colors,&painter);
    app->ui_state_bridge.native_text_owner=input;
    if (app->window && !input->suspended && (SDL_GetWindowFlags(app->window)&SDL_WINDOW_INPUT_FOCUS)) {
        if(app->ui_state_bridge.native_text_previous!=input && SDL_IsTextInputActive())SDL_StopTextInput();
        if (!SDL_IsTextInputActive()) SDL_StartTextInput();
        (void)kit_ui_native_text_rect_sdl(app->window,&input->view,app->width,app->height,NULL);
    }
}
void carta_text_click(CartaTextInput *input,float x,float y,bool extend) {
    if (!input || !input->view.count) return;
    float scale=input->view_scale;
    size_t position;
    if (kit_ui_text_presentation_hit(&input->view,x,y,measure,&scale,&position).code==CORE_OK)
        (void)kit_ui_text_position(&input->edit,position,extend?input->edit.anchor:position);
}
void carta_text_frame_begin(AppState *app) {
    app->ui_state_bridge.native_text_previous=app->ui_state_bridge.native_text_owner;
    app->ui_state_bridge.native_text_owner=NULL;
}
void carta_text_frame_end(AppState *app) {
    if(app->ui_state_bridge.native_text_previous!=app->ui_state_bridge.native_text_owner &&
       app->ui_state_bridge.native_text_previous)
        kit_ui_text_cancel_composition(&app->ui_state_bridge.native_text_previous->edit);
    if (!app->ui_state_bridge.native_text_owner) {
        if(SDL_IsTextInputActive())SDL_StopTextInput();
    }
}
void carta_text_window_event(AppState *app,const SDL_Event *event) {
    if(!app || !event || (event->type!=SDL_WINDOWEVENT && event->type!=SDL_QUIT))return;
    CartaTextInput *owner=app->ui_state_bridge.native_text_owner;
    if (owner) (void)carta_text_event(owner,event);
    if(owner && event->type==SDL_WINDOWEVENT &&
       (event->window.event==SDL_WINDOWEVENT_SIZE_CHANGED || event->window.event==SDL_WINDOWEVENT_RESIZED))
        kit_ui_text_cancel_composition(&owner->edit);
    if (event->type==SDL_QUIT || (event->type==SDL_WINDOWEVENT &&
        (event->window.event==SDL_WINDOWEVENT_FOCUS_LOST || event->window.event==SDL_WINDOWEVENT_HIDDEN))) {
        if (owner) kit_ui_text_cancel_composition(&owner->edit);
        SDL_StopTextInput();
    }
}
