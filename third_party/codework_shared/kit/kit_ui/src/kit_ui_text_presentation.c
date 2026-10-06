#include "kit_ui_text_presentation.h"
#include <math.h>
#include <string.h>
static CoreResult invalid(void) { return (CoreResult){CORE_ERR_INVALID_ARG,"invalid text presentation"}; }
static CoreResult full(void) { return (CoreResult){CORE_ERR_OUT_OF_MEMORY,"text presentation capacity exceeded"}; }
static size_t next_scalar(const char *s,size_t p) {
    ++p;while(s[p] && ((unsigned char)s[p]&0xc0)==0x80)++p;return p;
}
static size_t scalar_offset(const char *s,int count) {
    size_t p=0;while(count-- && s[p])p=next_scalar(s,p);return p;
}
static CoreResult width(char *s,size_t a,size_t b,KitUiTextMeasure fn,void *user,float *out) {
    char saved=s[b];s[b]=0;*out=0;
    CoreResult r=fn(user,s+a,out);s[b]=saved;
    if(r.code==CORE_OK && (!isfinite(*out)||*out<0))return invalid();return r;
}
static KitRenderRect intersection(KitRenderRect a,KitRenderRect b) {
    float x=fmaxf(a.x,b.x),y=fmaxf(a.y,b.y),r=fminf(a.x+a.width,b.x+b.width),d=fminf(a.y+a.height,b.y+b.height);
    return (KitRenderRect){x,y,fmaxf(0,r-x),fmaxf(0,d-y)};
}
CoreResult kit_ui_text_presentation_build(KitUiTextPresentation *out,const KitUiTextEdit *edit,
    const KitUiTextPresentationOptions *o,KitUiTextMeasure measure,void *user) {
    if(!out||!edit||!o||!measure || !isfinite(o->viewport.x)||!isfinite(o->viewport.y)||
       !isfinite(o->viewport.width)||!isfinite(o->viewport.height)||o->viewport.width<=0||o->viewport.height<=0||
       !isfinite(o->line_height)||o->line_height<=0||!isfinite(o->caret_width)||o->caret_width<=0||
       !isfinite(o->scroll_y)||o->scroll_y<0)return invalid();
    KitUiTextEdit checked=*edit;
    CoreResult r=kit_ui_text_bind(&checked,edit->text,edit->capacity,edit->flags);if(r.code!=CORE_OK)return r;
    KitUiTextPresentation p={0};p.options=*o;
    size_t len=strlen(checked.text),lo=checked.cursor<checked.anchor?checked.cursor:checked.anchor;
    size_t hi=checked.cursor>checked.anchor?checked.cursor:checked.anchor,sel_a=lo,sel_b=hi,pre_a=0,pre_b=0;
    size_t comp=0;
    if(o->active && checked.composition[0]) {
        if(!memchr(checked.composition,0,sizeof(checked.composition)))return invalid();
        r=kit_ui_text_compose(&checked,edit->composition,edit->composition_start,edit->composition_length);
        if(r.code!=CORE_OK)return r;
        comp=strlen(checked.composition);
    }
    if(len-(comp?hi-lo:0)+comp>=sizeof(p.display))return full();
    if(comp) {
        memcpy(p.display,checked.text,lo);memcpy(p.display+lo,checked.composition,comp);
        strcpy(p.display+lo+comp,checked.text+hi);pre_a=lo;pre_b=lo+comp;
        sel_a=lo+scalar_offset(checked.composition,checked.composition_start);
        sel_b=lo+scalar_offset(checked.composition,checked.composition_start+checked.composition_length);
        p.cursor=sel_a;p.replace_start=lo;p.replace_end=hi;p.composition_bytes=comp;
    } else {strcpy(p.display,checked.text);p.cursor=checked.cursor;}
    len=strlen(p.display);size_t start=0,used=0;int trailing=0;
    do {
        if(p.count==KIT_UI_TEXT_PRESENTATION_ROWS)return full();
        size_t end=start;float w=0;
        while(end<len && p.display[end]!='\n') {
            size_t candidate=next_scalar(p.display,end);float trial=0;
            r=width(p.display,start,candidate,measure,user,&trial);if(r.code!=CORE_OK)return r;
            if(o->wrap && trial>o->viewport.width && end>start)break;
            end=candidate;w=trial;
        }
        KitUiTextPresentationRow *row=&p.rows[p.count++];
        row->start=start;row->end=end;row->newline=end<len && p.display[end]=='\n';row->width=w;row->text_offset=used;
        memcpy(p.row_text+used,p.display+start,end-start);used+=end-start;p.row_text[used++]=0;
        row->origin=(KitRenderVec2){o->viewport.x,o->viewport.y+(float)(p.count-1)*o->line_height};
        size_t a=sel_a>start?sel_a:start,b=sel_b<end?sel_b:end;
        if(o->active && b>a) {
            float x0,x1;r=width(p.display,start,a,measure,user,&x0);if(r.code!=CORE_OK)return r;
            r=width(p.display,start,b,measure,user,&x1);if(r.code!=CORE_OK)return r;
            row->selection=(KitRenderRect){row->origin.x+x0,row->origin.y,fmaxf(0,x1-x0),o->line_height};
        }
        /* A selected hard newline still gets a small, visible end-of-line cell. */
        if(o->active && row->newline && sel_a<=end && sel_b>end && !row->selection.width)
            row->selection=(KitRenderRect){row->origin.x+w,row->origin.y,o->caret_width*3,o->line_height};
        a=pre_a>start?pre_a:start;b=pre_b<end?pre_b:end;
        if(comp && b>a) {
            float x0,x1;r=width(p.display,start,a,measure,user,&x0);if(r.code!=CORE_OK)return r;
            r=width(p.display,start,b,measure,user,&x1);if(r.code!=CORE_OK)return r;
            row->preedit=(KitRenderRect){row->origin.x+x0,row->origin.y+o->line_height-o->caret_width,fmaxf(0,x1-x0),o->caret_width};
        }
        if(p.cursor>=start && (p.cursor<end || (p.cursor==end && (row->newline || end==len)))) {
            float x;r=width(p.display,start,p.cursor,measure,user,&x);if(r.code!=CORE_OK)return r;
            p.caret=(KitRenderRect){row->origin.x+x,row->origin.y,o->caret_width,o->line_height};
        }
        trailing=row->newline && end+1==len;
        start=end+(row->newline?1:0);
    } while(start<len || trailing);
    p.content_height=p.count*o->line_height;
    if(!o->wrap && o->reveal_caret)p.scroll_x=fmaxf(0,p.caret.x+o->caret_width-(o->viewport.x+o->viewport.width));
    p.scroll_y=fminf(o->scroll_y,fmaxf(0,p.content_height-o->viewport.height));
    if(o->wrap && o->reveal_caret) {
        float y=p.caret.y-o->viewport.y;
        if(y<p.scroll_y)p.scroll_y=y;
        if(y+o->line_height>p.scroll_y+o->viewport.height)p.scroll_y=y+o->line_height-o->viewport.height;
    }
    for(size_t i=0;i<p.count;++i) {
        p.rows[i].origin.x-=p.scroll_x;p.rows[i].origin.y-=p.scroll_y;
        p.rows[i].selection.x-=p.scroll_x;p.rows[i].selection.y-=p.scroll_y;
        p.rows[i].preedit.x-=p.scroll_x;p.rows[i].preedit.y-=p.scroll_y;
    }
    p.caret.x-=p.scroll_x;p.caret.y-=p.scroll_y;
    *out=p;return core_result_ok();
}
CoreResult kit_ui_text_presentation_hit(const KitUiTextPresentation *p,float x,float y,
    KitUiTextMeasure measure,void *user,size_t *out) {
    if(!p||!measure||!out||!p->count||!isfinite(x)||!isfinite(y))return invalid();
    float index=floorf((y-p->options.viewport.y+p->scroll_y)/p->options.line_height);
    int row=index<0?0:(index>=(float)p->count?(int)p->count-1:(int)index);
    const KitUiTextPresentationRow *r=&p->rows[row];size_t result=r->end,offset=0;
    char run[KIT_UI_TEXT_PRESENTATION_BYTES];strcpy(run,p->row_text+r->text_offset);
    float before=0;
    while(run[offset]) {
        size_t end=next_scalar(run,offset);float after;
        CoreResult status=width(run,0,end,measure,user,&after);if(status.code!=CORE_OK)return status;
        if(x<r->origin.x+(before+after)*.5f) {result=r->start+offset;break;}
        before=after;offset=end;
    }
    if(p->composition_bytes && result>p->replace_start) {
        if(result<p->replace_start+p->composition_bytes)result=p->replace_start;
        else result=result-p->composition_bytes+p->replace_end-p->replace_start;
    }
    *out=result;return core_result_ok();
}
CoreResult kit_ui_text_presentation_paint(const KitUiTextPresentation *p,
    const KitUiTextPresentationColors *colors,const KitUiTextPainter *paint) {
    if(!p||!colors||!paint||!paint->rect||!paint->text)return invalid();
    for(size_t i=0;i<p->count;++i) {
        const KitUiTextPresentationRow *r=&p->rows[i];CoreResult result;
        KitRenderRect selection=intersection(r->selection,p->options.viewport);
        if(selection.width>0 && selection.height>0) {result=paint->rect(paint->user,selection,colors->selection);if(result.code!=CORE_OK)return result;}
        if(r->origin.y+p->options.line_height>p->options.viewport.y && r->origin.y<p->options.viewport.y+p->options.viewport.height && p->row_text[r->text_offset]) {
            result=paint->text(paint->user,r->origin,p->row_text+r->text_offset);if(result.code!=CORE_OK)return result;
        }
        KitRenderRect preedit=intersection(r->preedit,p->options.viewport);
        if(preedit.width>0 && preedit.height>0) {result=paint->rect(paint->user,preedit,colors->preedit);if(result.code!=CORE_OK)return result;}
    }
    KitRenderRect caret=intersection(p->caret,p->options.viewport);
    if(p->options.active && caret.width>0 && caret.height>0)return paint->rect(paint->user,caret,colors->caret);
    return core_result_ok();
}
