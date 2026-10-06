#include "kit_ui_text_presentation.h"
typedef struct RenderPainter {KitRenderFrame *frame;CoreFontRoleId role;CoreFontTextSizeTier tier;CoreThemeColorToken token;float line_height;} RenderPainter;
static CoreResult rect(void *data,KitRenderRect bounds,KitRenderColor color) {
    RenderPainter *p=data;KitRenderRectCommand cmd={bounds,0,color,kit_render_identity_transform()};return kit_render_push_rect(p->frame,&cmd);
}
static CoreResult text(void *data,KitRenderVec2 origin,const char *value) {
    RenderPainter *p=data;origin.y+=p->line_height*.5f;KitRenderTextCommand cmd={origin,value,p->role,p->tier,p->token,kit_render_identity_transform()};return kit_render_push_text(p->frame,&cmd);
}
CoreResult kit_ui_text_presentation_render(KitUiContext *ui,KitRenderFrame *frame,
    const KitUiTextPresentation *p,CoreFontRoleId role,CoreFontTextSizeTier tier,
    CoreThemeColorToken token,const KitUiTextPresentationColors *colors) {
    if(!ui||!frame||!frame->command_buffer||!p||!colors)return (CoreResult){CORE_ERR_INVALID_ARG,"invalid text render adapter"};
    size_t count=frame->command_buffer->count;int depth=ui->clip_depth;
    CoreResult r=kit_ui_clip_push(ui,frame,p->options.viewport);
    if(r.code==CORE_OK) {
        RenderPainter user={frame,role,tier,token,p->options.line_height};KitUiTextPainter paint={&user,rect,text};
        r=kit_ui_text_presentation_paint(p,colors,&paint);
        if(r.code==CORE_OK)r=kit_ui_clip_pop(ui,frame);
    }
    if(r.code!=CORE_OK) {frame->command_buffer->count=count;ui->clip_depth=depth;}
    return r;
}
