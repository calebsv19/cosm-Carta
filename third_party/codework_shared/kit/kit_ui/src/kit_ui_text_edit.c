#include "kit_ui_text_edit.h"
#include <stdint.h>
#include <string.h>
static CoreResult bad(void) { return (CoreResult){CORE_ERR_INVALID_ARG,"invalid UTF-8 text or boundary"}; }
static int scalar(const unsigned char *s,size_t n,size_t *width) {
    uint32_t c;size_t k;
    if (!n) return 0;
    if (s[0]<0x80) { *width=1;return s[0]!=0; }
    if (s[0]>=0xc2 && s[0]<=0xdf) { k=2;c=s[0]&31; }
    else if (s[0]>=0xe0 && s[0]<=0xef) { k=3;c=s[0]&15; }
    else if (s[0]>=0xf0 && s[0]<=0xf4) { k=4;c=s[0]&7; }
    else return 0;
    if (k>n) return 0;
    for(size_t i=1;i<k;++i) { if ((s[i]&0xc0)!=0x80) return 0;c=(c<<6)|(s[i]&63); }
    if ((k==2 && c<0x80)||(k==3 && c<0x800)||(k==4 && c<0x10000)||c>0x10ffff||(c>=0xd800 && c<=0xdfff)) return 0;
    *width=k;return 1;
}
static int valid(const char *s,size_t n) {
    for(size_t i=0,k;i<n;i+=k) if(!scalar((const unsigned char *)s+i,n-i,&k)) return 0;
    return 1;
}
static size_t boundary(const char *s,size_t n,size_t p) {
    if(p>n)p=n;
    while(p && p<n && ((unsigned char)s[p]&0xc0)==0x80)--p;
    return p;
}
static int ready(const KitUiTextEdit *e,size_t *n) {
    if(!e||!e->text||!e->capacity)return 0;
    const char *end=memchr(e->text,0,e->capacity);if(!end)return 0;
    *n=(size_t)(end-e->text);
    return valid(e->text,*n)&&e->cursor<=*n&&e->anchor<=*n&&boundary(e->text,*n,e->cursor)==e->cursor&&boundary(e->text,*n,e->anchor)==e->anchor;
}
CoreResult kit_ui_text_bind(KitUiTextEdit *e,char *s,size_t cap,unsigned flags) {
    if(!e||!s||!cap || (flags&~3u))return bad();
    const char *end=memchr(s,0,cap);if(!end||!valid(s,(size_t)(end-s)))return bad();
    size_t n=(size_t)(end-s);int same=e->text==s && e->capacity==cap;
    if(!same) { memset(e,0,sizeof(*e));e->cursor=e->anchor=n; }
    e->text=s;e->capacity=cap;e->flags=flags;
    e->cursor=boundary(s,n,e->cursor);e->anchor=boundary(s,n,e->anchor);return core_result_ok();
}
CoreResult kit_ui_text_position(KitUiTextEdit *e,size_t c,size_t a) {
    size_t n;if(!ready(e,&n))return bad();
    e->cursor=boundary(e->text,n,c);e->anchor=boundary(e->text,n,a);kit_ui_text_cancel_composition(e);return core_result_ok();
}
void kit_ui_text_cancel_composition(KitUiTextEdit *e) {
    if(e) { e->composition[0]=0;e->composition_start=e->composition_length=0; }
}
CoreResult kit_ui_text_insert(KitUiTextEdit *e,const char *s) {
    size_t n;if(!s||!ready(e,&n))return bad();size_t add=strlen(s);if(!valid(s,add))return bad();
    if(!add) { kit_ui_text_cancel_composition(e);return core_result_ok(); }
    for(size_t i=0;i<add;++i) {
        unsigned char c=(unsigned char)s[i];
        if((e->flags&KIT_UI_TEXT_DIGITS) && (c<'0'||c>'9'))return bad();
        if((e->flags&KIT_UI_TEXT_SINGLE_LINE) && (c=='\n'||c=='\r'))return bad();
    }
    size_t lo=e->cursor<e->anchor?e->cursor:e->anchor,hi=e->cursor>e->anchor?e->cursor:e->anchor;
    if(add>e->capacity-1-(n-(hi-lo)))return (CoreResult){CORE_ERR_OUT_OF_MEMORY,"text capacity exceeded; edit unchanged"};
    /* Overlapping input is rejected: callers stage clipboard/selection separately. */
    uintptr_t input=(uintptr_t)s,buffer=(uintptr_t)e->text;
    if(input>=buffer && input<buffer+e->capacity)return bad();
    memmove(e->text+lo+add,e->text+hi,n-hi+1);memcpy(e->text+lo,s,add);
    e->cursor=e->anchor=lo+add;kit_ui_text_cancel_composition(e);return core_result_ok();
}
CoreResult kit_ui_text_command(KitUiTextEdit *e,KitUiTextCommand cmd,int extend) {
    size_t n;if(!ready(e,&n))return bad();
    size_t lo=e->cursor<e->anchor?e->cursor:e->anchor,hi=e->cursor>e->anchor?e->cursor:e->anchor,p=e->cursor;
    switch(cmd) {
        case KIT_UI_TEXT_SELECT_ALL:e->anchor=0;e->cursor=n;break;
        case KIT_UI_TEXT_HOME:p=0;goto move;
        case KIT_UI_TEXT_END:p=n;goto move;
        case KIT_UI_TEXT_LEFT:
            if(!extend && lo!=hi)p=lo;else if(p)p=boundary(e->text,n,p-1);
            goto move;
        case KIT_UI_TEXT_RIGHT:
            if(!extend && lo!=hi)p=hi;else if(p<n){size_t k;scalar((unsigned char *)e->text+p,n-p,&k);p+=k;}
            goto move;
        case KIT_UI_TEXT_BACKSPACE:case KIT_UI_TEXT_DELETE:
            if(lo==hi) {
                if(cmd==KIT_UI_TEXT_BACKSPACE && lo)lo=boundary(e->text,n,lo-1);
                else if(cmd==KIT_UI_TEXT_DELETE && hi<n) {size_t k;scalar((unsigned char *)e->text+hi,n-hi,&k);hi+=k;}
            }
            memmove(e->text+lo,e->text+hi,n-hi+1);e->cursor=e->anchor=lo;break;
        default:return bad();
    }
    kit_ui_text_cancel_composition(e);return core_result_ok();
move:e->cursor=p;if(!extend)e->anchor=p;kit_ui_text_cancel_composition(e);return core_result_ok();
}
CoreResult kit_ui_text_selection(const KitUiTextEdit *e,char *out,size_t cap) {
    size_t n;if(!out||!ready(e,&n))return bad();
    size_t lo=e->cursor<e->anchor?e->cursor:e->anchor,hi=e->cursor>e->anchor?e->cursor:e->anchor;
    if(cap<=hi-lo)return (CoreResult){CORE_ERR_OUT_OF_MEMORY,"selection output too small"};
    uintptr_t output=(uintptr_t)out,buffer=(uintptr_t)e->text;
    /* Selection extraction must not mutate the borrowed editing buffer. */
    if(output>=buffer && output<buffer+e->capacity)return bad();
    memcpy(out,e->text+lo,hi-lo);out[hi-lo]=0;return core_result_ok();
}
CoreResult kit_ui_text_compose(KitUiTextEdit *e,const char *s,int start,int length) {
    size_t n;if(!s||!ready(e,&n)||start<0||length<0)return bad();size_t len=strlen(s);
    if(len>=sizeof(e->composition)||!valid(s,len))return bad();
    size_t count=0;for(size_t i=0,k;i<len;i+=k) {scalar((const unsigned char *)s+i,len-i,&k);++count;}
    if((size_t)start>count || (size_t)length>count-(size_t)start)return bad();
    memmove(e->composition,s,len+1);e->composition_start=start;e->composition_length=length;return core_result_ok();
}
