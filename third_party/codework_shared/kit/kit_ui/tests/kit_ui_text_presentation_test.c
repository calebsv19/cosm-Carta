#include "kit_ui_text_presentation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static CoreResult measure(void *user,const char *s,float *out) {
    float scale=*(float*)user;*out=0;
    for(size_t i=0;s[i];++i)if(((unsigned char)s[i]&0xc0)!=0x80)*out+=s[i]=='W'?15*scale:8*scale;
    return core_result_ok();
}
static KitUiTextPresentation a,b;
int main(void) {
    float scale=1;char source[4096]="aéWz";KitUiTextEdit edit={0};
    assert(kit_ui_text_bind(&edit,source,sizeof(source),0).code==CORE_OK);
    kit_ui_text_position(&edit,4,1);
    KitUiTextPresentationOptions o={{10,10,90,80},16,1,0,1,1,1};
    assert(kit_ui_text_presentation_build(&a,&edit,&o,measure,&scale).code==CORE_OK);
    assert(a.count==1 && a.rows[0].selection.width==23 && a.caret.x==41);
    size_t hit;assert(kit_ui_text_presentation_hit(&a,25,15,measure,&scale,&hit).code==CORE_OK && hit==3);
    assert(kit_ui_text_compose(&edit,"中W",1,1).code==CORE_OK);
    assert(kit_ui_text_presentation_build(&b,&edit,&o,measure,&scale).code==CORE_OK);
    assert(!strcmp(source,"aéWz") && !strcmp(b.display,"a中Wz") && b.rows[0].preedit.width==23);
    assert(b.caret.x==26 && b.rows[0].selection.width==15);
    kit_ui_text_cancel_composition(&edit);strcpy(source,"ab\nWéz\n");kit_ui_text_position(&edit,8,1);o.viewport.width=20;
    assert(kit_ui_text_presentation_build(&b,&edit,&o,measure,&scale).code==CORE_OK && b.count==4);
    assert(!strcmp(b.row_text+b.rows[0].text_offset,"ab") && !strcmp(b.row_text+b.rows[1].text_offset,"W"));
    assert(kit_ui_text_presentation_hit(&b,12,45,measure,&scale,&hit).code==CORE_OK && hit==4);
    KitUiTextPresentation saved=b;
    memset(source,'a',300);source[300]=0;o.viewport.width=1;
    assert(kit_ui_text_presentation_build(&b,&edit,&o,measure,&scale).code==CORE_ERR_OUT_OF_MEMORY && !memcmp(&b,&saved,sizeof(b)));
    o.viewport.width=NAN;assert(kit_ui_text_presentation_build(&b,&edit,&o,measure,&scale).code==CORE_ERR_INVALID_ARG);
    KitRenderContext render;KitUiContext ui;KitRenderFrame frame;KitRenderCommand commands[32];KitRenderCommandBuffer buf={commands,32,0};
    assert(kit_render_context_init(&render,KIT_RENDER_BACKEND_NULL,CORE_THEME_PRESET_DAW_DEFAULT,CORE_FONT_PRESET_DAW_DEFAULT).code==CORE_OK);
    assert(kit_ui_context_init(&ui,&render).code==CORE_OK);assert(kit_render_begin_frame(&render,200,150,&buf,&frame).code==CORE_OK);
    KitUiTextPresentationColors colors={{20,80,120,72},{80,180,255,255},{80,180,255,255}};
    assert(kit_ui_clip_push(&ui,&frame,(KitRenderRect){0,0,180,120}).code==CORE_OK);
    buf.capacity=3;assert(kit_ui_text_presentation_render(&ui,&frame,&a,CORE_FONT_ROLE_UI_REGULAR,CORE_FONT_TEXT_SIZE_BASIC,CORE_THEME_COLOR_TEXT_PRIMARY,&colors).code!=CORE_OK && buf.count==1 && ui.clip_depth==1);
    buf.capacity=32;assert(kit_ui_text_presentation_render(&ui,&frame,&a,CORE_FONT_ROLE_UI_REGULAR,CORE_FONT_TEXT_SIZE_BASIC,CORE_THEME_COLOR_TEXT_PRIMARY,&colors).code==CORE_OK);
    const char *queued=NULL;for(unsigned i=0;i<buf.count;++i)if(commands[i].kind==KIT_RENDER_CMD_TEXT)queued=commands[i].data.text.text;
    assert(queued && !strcmp(queued,"aéWz"));strcpy(source,"mutated host storage");assert(!strcmp(queued,"aéWz"));
    assert(ui.clip_depth==1 && commands[buf.count-1].kind==KIT_RENDER_CMD_SET_CLIP);
    assert(kit_ui_clip_pop(&ui,&frame).code==CORE_OK);assert(kit_render_end_frame(&render,&frame).code==CORE_OK);
    puts("Text presentation: measured UTF8 rows/hit, wrapping/newlines, preedit isolation, transactional bounds, nested clip rollback and queued storage lifetime pass");
}
