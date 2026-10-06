#include "app/history_picker.h"
#include "app/app_internal.h"
#include "app/terrain_vectors.h"
#include <string.h>
static SDL_FRect rect(SDL_FRect b,int action) {
    if(action<6)return (SDL_FRect){b.x,b.y+42+action*38,b.w,34};
    return (SDL_FRect){b.x+(action==7?b.w*.52f:0),b.y+280,b.w*.48f,28};
}
void history_picker_cancel(HistoryPickerControls *c){kit_ui_surface_set_scope(&c->surface,++c->scope);}
static void sync(HistoryPickerControls *c,AppState *a,const char *query,int offset,SDL_FRect base) {
    char names[6][128]={{0}};double lat,lon;
    for(int i=0;i<6;i++)if(!terrain_vectors_peak_query(query,offset+i,names[i],sizeof(names[i]),&lat,&lon))break;
    if(strcmp(c->query,query) || c->offset!=offset || memcmp(c->names,names,sizeof(names))) {
        SDL_strlcpy(c->query,query,sizeof(c->query));c->offset=offset;memcpy(c->names,names,sizeof(names));history_picker_cancel(c);
    }
    carta_modal_controls_begin(&c->surface,c->scope?c->scope:1);
    SDL_FRect clip={0,0,a->width,a->height};
    for(int i=0;i<8;i++)if(i>=6 || *c->names[i])carta_modal_controls_add(&c->surface,i,rect(base,i),clip,true);
    carta_modal_controls_end(&c->surface);
}
int history_picker_event(HistoryPickerControls *c,AppState *a,const char *query,int offset,SDL_FRect base,const SDL_Event *e) {
    sync(c,a,query,offset,base);
    // The query remains the keyboard owner; pointer controls use shared release.
    if(e->type==SDL_KEYDOWN || e->type==SDL_KEYUP)return -1;
    uint64_t action;uint32_t modifiers;(void)carta_modal_controls_event(&c->surface,e,&action,&modifiers);
    return action==UINT64_MAX?-1:(int)action;
}
void history_picker_draw(HistoryPickerControls *c,AppState *a,const char *query,int offset,SDL_FRect base) {
    sync(c,a,query,offset,base);
    for(int i=0;i<8;i++)if(i>=6 || *c->names[i])
        carta_ui_control_draw_surface(a,&c->surface,rect(base,i),i<6?c->names[i]:i==6?"Previous":"Next",false,(KitUiSurfaceKey){1,i},.78f);
}
