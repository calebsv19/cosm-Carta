#include "app/history_views.h"
#include "app/app_internal.h"
#include "app/history_status.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
// Gives each group an accessible bright path tint; colors never imply durable progress.
static const SDL_Color palette[HISTORY_VIEW_COLORS] = {
    {66,202,177,255}, {70,208,122,255}, {87,167,255,255}, {63,216,226,255},
    {250,195,77,255}, {191,145,255,255}, {244,130,166,255}, {255,143,102,255},
    {164,151,255,255}, {113,202,255,255}, {227,205,113,255}, {239,157,220,255}
};
// Names presets independently of their underlying progress/source query.
static const char *presets[] = {"My hikes", "Completed", "To do + partial", "Recorded activities",
    "Reference routes", "Peak catalog", "Visit history"};
// Reads a strict string field without coercing unknown metadata.
static const char *text(struct json_object *v, const char *key) {
    struct json_object *s = NULL;
    return v && json_object_object_get_ex(v,key,&s) && json_object_is_type(s,json_type_string)
        ? json_object_get_string(s) : "";
}
// Retrieves a catalog array without introducing mutations.
static struct json_object *array(struct AppState *a, const char *key) {
    struct json_object *v = NULL;
    json_object_object_get_ex(a->pins_file.source_json,key,&v);
    return v && json_object_is_type(v,json_type_array) ? v : NULL;
}
// Computes a stable default saved-list palette slot from its identity.
static uint8_t default_color(int filter, const char *id) {
    if (filter >= 0) return (uint8_t)filter;
    uint32_t hash = 2166136261u;
    for (const unsigned char *p=(const unsigned char *)id; *p; p++) hash=(hash^*p)*16777619u;
    return (uint8_t)(7+hash%5);
}
void history_views_single(struct AppState *a, int filter, const char *id) {
    HistoryViewSet *s = &a->ui_state_bridge.history_views;
    memset(s,0,sizeof(*s)); s->initialized=true; s->count=1;
    HistoryViewSelection *v=&s->items[0];
    v->filter=id && *id ? -1 : filter>=0 && filter<7 ? filter : 0;
    snprintf(v->collection,sizeof(v->collection),"%s",v->filter<0?id:"");
    v->map_visible=true; v->color=default_color(v->filter,v->collection);
    a->ui_state_bridge.history_filter=v->filter<0?0:v->filter;
    snprintf(a->ui_state_bridge.history_collection_id,sizeof(a->ui_state_bridge.history_collection_id),"%s",v->collection);
}
void history_views_ensure(struct AppState *a) {
    if (!a->ui_state_bridge.history_views.initialized) {
        char id[129]; snprintf(id,sizeof(id),"%s",a->ui_state_bridge.history_collection_id);
        history_views_single(a,a->ui_state_bridge.history_filter,id);
    }
}
int history_views_choice_count(struct AppState *a) {
    struct json_object *g=array(a,"hiking_collections");
    return 7+(g?(int)json_object_array_length(g):0);
}
HistoryViewSelection history_views_choice(struct AppState *a, int n) {
    HistoryViewSelection v={.filter=n<7?n:-1,.map_visible=true};
    if(n>=7)snprintf(v.collection,sizeof(v.collection),"%s",text(json_object_array_get_idx(array(a,"hiking_collections"),n-7),"id"));
    v.color=default_color(v.filter,v.collection); return v;
}
int history_views_find(struct AppState *a, HistoryViewSelection v) {
    history_views_ensure(a); HistoryViewSet *s=&a->ui_state_bridge.history_views;
    for(size_t i=0;i<s->count;i++)
        if(s->items[i].filter==v.filter && !strcmp(s->items[i].collection,v.collection))return (int)i;
    return -1;
}
bool history_views_toggle(struct AppState *a, HistoryViewSelection v) {
    int n=history_views_find(a,v); HistoryViewSet *s=&a->ui_state_bridge.history_views;
    if(n>=0) { memmove(&s->items[n],&s->items[n+1],(s->count-n-1)*sizeof(v)); s->count--; return true; }
    if(s->count==HISTORY_VIEWS_MAX)return false;
    // Prefer an unused palette tint for newly combined groups; retained choices never drift.
    for(int attempt=0;attempt<HISTORY_VIEW_COLORS;attempt++) {
        bool used=false;
        for(size_t i=0;i<s->count;i++)if(s->items[i].color==v.color)used=true;
        if(!used)break;
        v.color=(v.color+1)%HISTORY_VIEW_COLORS;
    }
    s->items[s->count++]=v; return true;
}
void history_views_name(struct AppState *a, HistoryViewSelection v, char *name, size_t cap) {
    if(v.filter>=0 && v.filter<7){snprintf(name,cap,"%s",presets[v.filter]);return;}
    struct json_object *g=array(a,"hiking_collections");
    for(size_t i=0;g && i<json_object_array_length(g);i++) {
        struct json_object *r=json_object_array_get_idx(g,i);
        if(!strcmp(v.collection,text(r,"id"))){snprintf(name,cap,"%s",text(r,"name"));return;}
    }
    snprintf(name,cap,"Unavailable saved list");
}
SDL_Color history_views_color(HistoryViewSelection v) { return palette[v.color%HISTORY_VIEW_COLORS]; }
SDL_Color history_views_tint(SDL_Color b, SDL_Color c, float amount) {
    float t=isfinite(amount)?fminf(1,fmaxf(0,amount)):0;
    return (SDL_Color){b.r+(c.r-b.r)*t,b.g+(c.g-b.g)*t,b.b+(c.b-b.b)*t,255};
}
// Tests direct membership by stable source collection ID.
static bool collection_member(struct json_object *v, const char *id) {
    struct json_object *g=NULL;json_object_object_get_ex(v,"collections",&g);
    if(!g || !json_object_is_type(g,json_type_array))return false;
    for(size_t i=0;i<json_object_array_length(g);i++) {
        struct json_object *s=json_object_array_get_idx(g,i);
        if(json_object_is_type(s,json_type_string) && !strcmp(json_object_get_string(s),id))return true;
    }
    return false;
}
// Evaluates one group independently, so several choices compose by union rather than intersection.
static bool matches(struct AppState *a, struct json_object *v, HistoryViewSelection group) {
    const char *kind=text(v,"kind"), *p=history_progress(a->pins_file.source_json,v);
    if(group.filter<0) {
        if(collection_member(v,group.collection))return true;
        struct json_object *h=array(a,"hiking");
        for(size_t i=0;!strcmp(kind,"recorded") && h && i<json_object_array_length(h);i++) {
            struct json_object *other=json_object_array_get_idx(h,i);
            if(!strcmp(text(other,"recording_id"),text(v,"id")) && collection_member(other,group.collection))return true;
        }
        return false;
    }
    if(!strcmp(kind,"peak") && group.filter!=5 && group.filter!=4 && !history_peak_tracked(a->pins_file.source_json,v))return false;
    switch(group.filter) {
        case 0:return !strcmp(p,"completed") || !strcmp(p,"planned") || !strcmp(p,"partial");
        case 1:return !strcmp(p,"completed");
        case 2:return !strcmp(p,"planned") || !strcmp(p,"partial");
        case 3:return !strcmp(kind,"recorded") || *text(v,"recording_id");
        case 4:return !strcmp(kind,"recommended");
        case 5:return !strcmp(kind,"peak");
        case 6:return !strcmp(kind,"visit");
        default:return false;
    }
}
uint32_t history_views_membership(struct AppState *a, struct json_object *record, bool map_only) {
    history_views_ensure(a); HistoryViewSet *s=&a->ui_state_bridge.history_views; uint32_t mask=0;
    for(size_t i=0;i<s->count;i++)
        if((!map_only || s->items[i].map_visible) && matches(a,record,s->items[i]))mask|=UINT32_C(1)<<i;
    return mask;
}
SDL_Color history_views_record_color(struct AppState *a, struct json_object *v) {
    uint32_t mask=history_views_membership(a,v,true);
    if(!mask)mask=history_views_membership(a,v,false);
    for(size_t i=0;i<a->ui_state_bridge.history_views.count;i++)
        if(mask&(UINT32_C(1)<<i))return history_views_color(a->ui_state_bridge.history_views.items[i]);
    return palette[0];
}
void history_views_load(struct AppState *a, struct json_object *root) {
    HistoryViewSet *s=&a->ui_state_bridge.history_views; memset(s,0,sizeof(*s));
    struct json_object *values=NULL;
    if(!json_object_object_get_ex(root,"history_views",&values) || !json_object_is_type(values,json_type_array)) {history_views_ensure(a);return;}
    s->initialized=true;
    for(size_t i=0;i<json_object_array_length(values) && s->count<HISTORY_VIEWS_MAX;i++) {
        struct json_object *v=json_object_array_get_idx(values,i),*f=NULL,*show=NULL,*color=NULL;
        if(!json_object_is_type(v,json_type_object) || !json_object_object_get_ex(v,"filter",&f) || !json_object_is_type(f,json_type_int))continue;
        int filter=json_object_get_int(f);const char *id=text(v,"collection");
        if(filter < -1 || filter > 6 || (filter<0 && (!*id || strlen(id)>128)))continue;
        HistoryViewSelection choice={.filter=filter,.map_visible=true};
        snprintf(choice.collection,sizeof(choice.collection),"%s",filter<0?id:"");choice.color=default_color(filter,choice.collection);
        if(json_object_object_get_ex(v,"map_visible",&show) && json_object_is_type(show,json_type_boolean))choice.map_visible=json_object_get_boolean(show);
        if(json_object_object_get_ex(v,"color",&color) && json_object_is_type(color,json_type_int) && json_object_get_int(color)>=0 && json_object_get_int(color)<HISTORY_VIEW_COLORS)choice.color=json_object_get_int(color);
        if(history_views_find(a,choice)<0)s->items[s->count++]=choice;
    }
}
void history_views_save(const struct AppState *a, struct json_object *root) {
    HistoryViewSet snapshot=a->ui_state_bridge.history_views;
    if(!snapshot.initialized) {
        snapshot.count=1;HistoryViewSelection *v=&snapshot.items[0];
        v->filter=*a->ui_state_bridge.history_collection_id?-1:a->ui_state_bridge.history_filter;
        snprintf(v->collection,sizeof(v->collection),"%s",a->ui_state_bridge.history_collection_id);
        v->map_visible=true;v->color=default_color(v->filter,v->collection);
    }
    const HistoryViewSet *s=&snapshot;
    struct json_object *values=json_object_new_array();
    for(size_t i=0;i<s->count;i++) {
        HistoryViewSelection v=s->items[i];struct json_object *o=json_object_new_object();
        json_object_object_add(o,"filter",json_object_new_int(v.filter));
        json_object_object_add(o,"collection",json_object_new_string(v.collection));
        json_object_object_add(o,"map_visible",json_object_new_boolean(v.map_visible));
        json_object_object_add(o,"color",json_object_new_int(v.color));json_object_array_add(values,o);
    }
    json_object_object_add(root,"history_views",values);
}
