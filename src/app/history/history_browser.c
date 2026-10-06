#include "app/history_browser.h"
#include "app/app_ui_internal.h"
#include "ui/font.h"
#include "kit_ui.h"
#include "ui/modal_controls.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
float history_browser_scroll(SDL_FRect v, int total, float height, float offset, float wheel) {
    float content = fmaxf(0, total * height), limit = fmaxf(0, content - v.h);
    offset = isfinite(offset) ? fminf(limit, fmaxf(0, offset)) : 0;
    if (!isfinite(wheel)) wheel = 0;
    KitUiScrollResult result = kit_ui_eval_scroll((KitRenderRect){v.x,v.y,v.w,v.h}, offset, content, wheel);
    return fminf(limit, fmaxf(0, result.offset_y));
}
float history_browser_wheel(const SDL_Event *e) {
    float value = e->wheel.y;
#if SDL_VERSION_ATLEAST(2,0,18)
    if (e->wheel.preciseY != 0) value = e->wheel.preciseY;
#endif
    return e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -value : value;
}
int history_browser_row(SDL_FRect v, float offset, float height, int x, int y) {
    if (height <= 0 || x < v.x || x >= v.x + v.w || y < v.y || y >= v.y + v.h) return -1;
    return (int)floorf((y - v.y + offset) / height);
}
SDL_FRect history_browser_thumb(SDL_FRect v, int total, float height, float offset) {
    float content = total * height;
    if (content <= v.h || v.h <= 0) return (SDL_FRect){0};
    float h = fminf(v.h, fmaxf(18, v.h * v.h / content));
    return (SDL_FRect){v.x + v.w - 6, v.y + (v.h - h) * offset / (content - v.h), 4, h};
}
void history_browser_view_label(AppState *a, char *s, size_t cap) {
    history_views_ensure(a); HistoryViewSet *v=&a->ui_state_bridge.history_views;
    size_t visible=0;for(size_t i=0;i<v->count;i++)visible+=v->items[i].map_visible;
    if(v->count==1){char name[129];history_views_name(a,v->items[0],name,sizeof(name));snprintf(s,cap,"Views: %s%s",name,visible?"":" · map off");}
    else snprintf(s,cap,"Views: %zu selected · %zu on map",v->count,visible);
}
// Keeps the multi-group chooser bounded inside the current window.
static SDL_FRect box(AppState *a) {
    float w=fminf(560,a->width-24),h=fminf(430,a->height-40);
    return (SDL_FRect){(a->width-w)/2,(a->height-h)/2,w,h};
}
// Separates scrolling choices from the heading and keyboard help.
static SDL_FRect list(AppState *a) { SDL_FRect b=box(a);return (SDL_FRect){b.x+12,b.y+46,b.w-24,b.h-98}; }
// Action keys identify choices within a visible identity-bound modal snapshot.
static SDL_FRect cell(SDL_FRect v,float y,int action) {
    float right=v.x+v.w;
    if(action==3)return (SDL_FRect){right-48,y,40,29};
    if(action==1)return (SDL_FRect){right-101,y,50,29};
    if(action==2)return (SDL_FRect){right-129,y,25,29};
    return (SDL_FRect){v.x,y,fmaxf(1,v.w-132),29};
}
static void sync(HistoryBrowser *b,AppState *a) {
    SDL_FRect v=list(a),r=box(a);int total=history_views_choice_count(a);
    b->offset=history_browser_scroll(v,total,32,b->offset,0);
    char identities[16][140]={{0}};int first=(int)(b->offset/32),visible=0;
    for(int n=first;n<total && n*32-b->offset<v.h && visible<16;n++,visible++) {
        HistoryViewSelection c=history_views_choice(a,n);
        snprintf(identities[visible],sizeof(identities[visible]),"%d:%s",c.filter,c.collection);
    }
    if(memcmp(identities,b->identities,sizeof(identities))) {
        memcpy(b->identities,identities,sizeof(identities));kit_ui_surface_set_scope(&b->controls,++b->scope);
    }
    carta_modal_controls_begin(&b->controls,b->scope?b->scope:1);
    carta_modal_controls_add(&b->controls,0,(SDL_FRect){r.x+r.w-70,r.y+6,58,30},(SDL_FRect){0,0,a->width,a->height},true);
    for(int n=first;n<first+visible;n++) {
        HistoryViewSelection c=history_views_choice(a,n);int slot=history_views_find(a,c);
        for(int action=0;action<4;action++)
            carta_modal_controls_add(&b->controls,1+n*4+action,cell(v,v.y+n*32-b->offset,action),v,slot>=0 || action==0 || action==3);
    }
    carta_modal_controls_end(&b->controls);
    if(!b->controls.interaction.focused_id)for(unsigned i=0;i<b->controls.count;i++)
        if(b->controls.keys[i].value==1u+(unsigned)b->focus*4)b->controls.interaction.focused_id=b->controls.controls[i].id;
}
void history_browser_open(HistoryBrowser *b, AppState *a) {
    history_views_ensure(a);b->open=true;b->focus=0;b->offset=0;
    kit_ui_surface_set_scope(&b->controls,++b->scope);
    for(int i=0;i<history_views_choice_count(a);i++)
        if(history_views_find(a,history_views_choice(a,i))>=0){b->focus=i;break;}
    b->offset=history_browser_scroll(list(a),history_views_choice_count(a),32,fmaxf(0,b->focus*32-list(a).h+32),0);
}
// Applies additive checkbox, independent visibility, palette cycling or explicit Solo.
static bool change(HistoryBrowser *b, AppState *a, int n, int action) {
    if(n<0 || n>=history_views_choice_count(a))return false;
    b->focus=n;HistoryViewSelection choice=history_views_choice(a,n);
    int slot=history_views_find(a,choice);
    if(slot>=0)choice=a->ui_state_bridge.history_views.items[slot];
    if(action==3){history_views_single(a,choice.filter,choice.collection);a->ui_state_bridge.history_views.items[0].color=choice.color;return true;}
    if(action==0)return history_views_toggle(a,choice);
    if(slot<0)return false;
    HistoryViewSelection *selected=&a->ui_state_bridge.history_views.items[slot];
    if(action==1)selected->map_visible=!selected->map_visible;
    if(action==2)selected->color=(selected->color+1)%HISTORY_VIEW_COLORS;
    return true;
}
// Consumes modal input; toggles keep the chooser open for further group selection.
bool history_browser_event(HistoryBrowser *b, AppState *a, const SDL_Event *e) {
    if(e->type==SDL_DROPFILE){SDL_free(e->drop.file);return false;}
    sync(b,a);uint64_t action;uint32_t modifiers;
    SDL_Event normalized=*e;
    bool solo_key=(e->type==SDL_KEYDOWN || e->type==SDL_KEYUP) &&
        (e->key.keysym.sym==SDLK_RETURN || e->key.keysym.sym==SDLK_KP_ENTER) &&
        (e->key.keysym.mod&KMOD_SHIFT) && !(e->key.keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI));
    if(solo_key)normalized.key.keysym.mod=KMOD_NONE;
    (void)carta_modal_controls_event(&b->controls,&normalized,&action,&modifiers);
    if(solo_key)modifiers|=KIT_UI_INTERACTION_MOD_SHIFT;
    if(action!=UINT64_MAX) {
        if(action==0){b->open=false;kit_ui_surface_set_scope(&b->controls,++b->scope);return false;}
        int n=(int)((action-1)/4),kind=(int)((action-1)%4);
        if(kind==0 && (modifiers&KIT_UI_INTERACTION_MOD_SHIFT))kind=3;
        return change(b,a,n,kind);
    }
    SDL_FRect v=list(a),r=box(a);int total=history_views_choice_count(a);
    if(e->type==SDL_KEYDOWN) {
        SDL_Keycode k=e->key.keysym.sym;
        if(k==SDLK_ESCAPE && !e->key.repeat){b->open=false;kit_ui_surface_set_scope(&b->controls,++b->scope);}
        bool navigation=k==SDLK_UP || k==SDLK_DOWN || k==SDLK_HOME || k==SDLK_END;
        if(k==SDLK_UP)b->focus=fmax(0,b->focus-1);
        if(k==SDLK_DOWN)b->focus=fmin(total-1,b->focus+1);
        if(k==SDLK_HOME)b->focus=0;
        if(k==SDLK_END)b->focus=total-1;
        if(!e->key.repeat && k==SDLK_m)return change(b,a,b->focus,1);
        if(!e->key.repeat && k==SDLK_c)return change(b,a,b->focus,2);
        if(navigation) {
            b->offset=history_browser_scroll(v,total,32,fmaxf(b->focus*32-v.h+32,fminf(b->offset,b->focus*32)),0);sync(b,a);
            for(unsigned i=0;i<b->controls.count;i++)if(b->controls.keys[i].value==1u+(unsigned)b->focus*4)
                b->controls.interaction.focused_id=b->controls.controls[i].id;
        }
    } else if(e->type==SDL_MOUSEWHEEL) {
        b->offset=history_browser_scroll(v,total,32,b->offset,history_browser_wheel(e));sync(b,a);
    } else if(e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT &&
        (e->button.x<r.x || e->button.x>=r.x+r.w || e->button.y<r.y || e->button.y>=r.y+r.h)) {
        b->open=false;kit_ui_surface_set_scope(&b->controls,++b->scope);
    }
    return false;
}
// Draws an opaque rectangle in the renderer's current local coordinates.
static void fill(AppState *a, SDL_FRect r, SDL_Color c) {
    renderer_set_draw_color(&a->renderer,c.r,c.g,c.b,255);renderer_fill_rect(&a->renderer,&r);
}
void history_browser_draw(HistoryBrowser *b, AppState *a) {
    if(!b->open)return;
    sync(b,a);MapForgeThemePalette c=app_ui_theme_palette();SDL_FRect r=box(a),v=list(a);
    fill(a,r,c.route_panel_fill);
    ui_draw_text_clipped(&a->renderer,r.x+12,r.y+12,"Views & route colors",c.text_primary,.85f,r.w-84);
    carta_ui_control_draw_surface(a,&b->controls,(SDL_FRect){r.x+r.w-70,r.y+6,58,30},"Done",false,(KitUiSurfaceKey){1,0},.8f);
    int total=history_views_choice_count(a);b->offset=history_browser_scroll(v,total,32,b->offset,0);
    if(renderer_set_viewport(&a->renderer,v.x,v.y,v.w,v.h)) {
        for(int n=(int)(b->offset/32);n<total;n++) {
            float y=n*32-b->offset;if(y>=v.h)break;
            HistoryViewSelection choice=history_views_choice(a,n);int slot=history_views_find(a,choice);
            if(slot>=0)choice=a->ui_state_bridge.history_views.items[slot];
            SDL_Color color=history_views_color(choice);
            fill(a,(SDL_FRect){0,y,v.w-8,29},slot>=0?history_views_tint(c.button_fill,color,.22f):c.button_fill);
            fill(a,(SDL_FRect){7,y+8,13,13},c.text_muted);
            fill(a,(SDL_FRect){9,y+10,9,9},slot>=0?color:c.button_fill);
            char name[192],title[224];history_views_name(a,choice,name,sizeof(name));
            snprintf(title,sizeof(title),"%s%s",choice.filter<0?"List · ":"",name);
            ui_draw_text_clipped(&a->renderer,v.x+27,v.y+y+5,title,c.text_primary,.75f,fmaxf(1,v.w-163));
            fill(a,(SDL_FRect){v.w-124,y+7,18,16},color);
            carta_ui_control_draw_surface(a,&b->controls,cell(v,v.y+y,1),slot<0?"":choice.map_visible?"Hide":"Show",false,(KitUiSurfaceKey){1,1+n*4+1},.7f);
            carta_ui_control_draw_surface(a,&b->controls,cell(v,v.y+y,3),"Solo",false,(KitUiSurfaceKey){1,1+n*4+3},.7f);
            if(n==b->focus)fill(a,(SDL_FRect){0,y,2,29},c.text_primary);
        }
        renderer_reset_viewport(&a->renderer);
    }
    SDL_FRect thumb=history_browser_thumb(v,total,32,b->offset);if(thumb.w)fill(a,thumb,c.text_muted);
    ui_draw_text_clipped(&a->renderer,r.x+12,r.y+r.h-39,"Check groups · click swatches to change route colors",c.text_muted,.65f,r.w-24);
    ui_draw_text_clipped(&a->renderer,r.x+12,r.y+r.h-22,"Space: select · M: map · C: color · Shift+Enter: solo",c.text_muted,.65f,r.w-24);
}
