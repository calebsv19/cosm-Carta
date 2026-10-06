#include "app/terrain_vectors.h"
#include "app/history_ui.h"
#include "app/app_internal.h"
#include "app/app_map_viewport_internal.h"
#include "app/terrain.h"
#include "map/terrain_vectors.h"
#include "map/mercator.h"
#include "ui/font.h"
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VECTOR_CACHE 24
#define VECTOR_DRAW_LIMIT 8000
// Retains only bounded decoded contour geometry, never a personal-history copy.
typedef struct VectorTile {
    TerrainVectorSegment *segments;
    size_t count;
    unsigned long used;
    int area,lod,col,row;
    bool occupied,failed;
} VectorTile;
// Owns read-only geometry, bounded residency, collision boxes and visible contour annotations.
static struct {
    TerrainVectors pack;
    VectorTile tiles[VECTOR_CACHE];
    bool attempted;
    unsigned long frame;
    unsigned drawn, pending, peak_lines, peak_entries;
    size_t peak_bytes;
    int min_lod;
    float anchor_zoom, anchor_x, anchor_y;
    bool anchored;
    SDL_FRect occupied[72];
    int occupied_count;
    struct { float x,y;int elevation; } elevations[12];
    int elevation_count;
    char error[160];
} vectors;
unsigned terrain_vectors_drawn_lines(void) { return vectors.drawn; }
void terrain_vectors_shutdown(void) {
    if(vectors.attempted) SDL_Log("[terrain vectors] high_water entries=%u decoded_bytes=%zu lines=%u",vectors.peak_entries,vectors.peak_bytes,vectors.peak_lines);
    for(int i=0;i<VECTOR_CACHE;i++)free(vectors.tiles[i].segments);
    terrain_vectors_close(&vectors.pack);memset(&vectors,0,sizeof(vectors));
}
// Opens the optional sidecar once; explicit Reload retries a missing or invalid file.
static bool ready(void) {
    if(!vectors.attempted) {
        const char *path=terrain_vector_path();
        if(!path || !*path)return false;
        vectors.attempted=true;
        if(terrain_vectors_open(&vectors.pack,path,vectors.error,sizeof(vectors.error))) {
            const TerrainCatalog *catalog=terrain_area_catalog();
            for(size_t i=0;i<catalog->count;i++)if(catalog->areas[i].removable && !strcmp(path,catalog->areas[i].vector_path)) {
                bool matched=vectors.pack.count==1;
                for(int k=0;matched&&k<4;k++)if(fabs(vectors.pack.areas[0].extent[k]-catalog->areas[i].extent[k])>1)matched=false;
                if(!matched){terrain_vectors_close(&vectors.pack);snprintf(vectors.error,sizeof(vectors.error),"Vector bounds differ from terrain catalog");return false;}
            }
            SDL_Log("[terrain vectors] verified bytes=%zu labels=%zu areas=%d",vectors.pack.bytes,vectors.pack.label_count,vectors.pack.count);
        }
    }
    return vectors.pack.db!=NULL;
}
// Finds a resident key or evicts only an older frame's geometry.
static VectorTile *tile(int area,int lod,int col,int row,bool *fresh) {
    VectorTile *old=NULL;*fresh=false;
    for(int i=0;i<VECTOR_CACHE;i++) {
        VectorTile *t=&vectors.tiles[i];
        if(t->occupied&&t->area==area&&t->lod==lod&&t->col==col&&t->row==row)return t;
        if(!t->occupied)old=t;
        else if(t->used+1<vectors.frame&&(!old||(old->occupied&&t->used<old->used)))old=t;
    }
    if(!old)return NULL;
    free(old->segments);*old=(VectorTile){.area=area,.lod=lod,.col=col,.row=row,.occupied=true};*fresh=true;return old;
}
// Describes one bounded geographic contour window shared by loading and refinement.
typedef struct VectorWindow { int c0, c1, r0, r1, keys; } VectorWindow;
// Computes exact tile keys at one contour level without loading or drawing geometry.
static VectorWindow window(const TerrainVectorArea *a, int lod, float x0, float y0, float x1, float y1) {
    double unit = a->unit[lod];
    VectorWindow w = {fmax(0,floor((x0-a->extent[0])/unit)),
        fmin(ceil((a->extent[2]-a->extent[0])/unit)-1,floor((x1-a->extent[0])/unit)),
        fmax(0,floor((a->extent[3]-y1)/unit)),
        fmin(ceil((a->extent[3]-a->extent[1])/unit)-1,floor((a->extent[3]-y0)/unit)),0};
    if (w.c1 >= w.c0 && w.r1 >= w.r0) w.keys = (w.c1-w.c0+1)*(w.r1-w.r0+1);
    return w;
}
// Loads at most two total tiles per frame and protects every visible coarse/fine key.
static int load_window(int area, int lod, VectorWindow w, VectorTile **out, unsigned *loaded,
                       unsigned *pending) {
    int count = 0, keys = 0;
    for (int row=w.r0;row<=w.r1&&keys<20;row++) for(int col=w.c0;col<=w.c1&&keys<20;col++,keys++) {
        bool fresh; VectorTile *t=tile(area,lod,col,row,&fresh);
        if(!t){(*pending)++;continue;}
        t->used=vectors.frame; if(t->failed)continue;
        if(fresh && *loaded>=2){t->occupied=false;(*pending)++;continue;}
        if(fresh) {
            (*loaded)++;
            if(!terrain_vectors_tile(&vectors.pack,area,lod,col,row,&t->segments,&t->count)) {
                t->failed=true;snprintf(vectors.error,sizeof(vectors.error),"Vector tile damaged; Reload terrain to retry");continue;
            }
        }
        out[count++]=t;
    }
    return count;
}
// Submits only connected equal-height contour points through the existing native line-strip API.
static void strip_draw(AppState *app, SDL_FPoint *points, int count, bool major) {
    if(count<2)return;
    renderer_set_draw_color(&app->renderer,major?78:95,major?95:112,major?76:90,major?170:100);
    renderer_draw_lines(&app->renderer,points,count);
}
void terrain_vectors_draw(AppState *app) {
    vectors.drawn=vectors.pending=vectors.elevation_count=0;
    if(!terrain_active(app)||terrain_render_mode()!=0||!ready())return;
    vectors.frame++;
    float xmin,ymin,xmax,ymax;
    if(!camera_visible_world_aabb(&app->view_state_bridge.camera,app->renderer.width,app->renderer.height,&xmin,&ymin,&xmax,&ymax))return;
    Camera *camera=&app->view_state_bridge.camera;
    if(!vectors.anchored || fabsf(camera->zoom-vectors.anchor_zoom)>.15f ||
       hypotf(camera->x-vectors.anchor_x,camera->y-vectors.anchor_y)>fmaxf(50,(xmax-xmin)*.05f)) {
        vectors.min_lod=0;vectors.anchor_zoom=camera->zoom;vectors.anchor_x=camera->x;vectors.anchor_y=camera->y;vectors.anchored=true;
    }
    int area=terrain_vectors_select(&vectors.pack,xmin,ymin,xmax,ymax),lod=app->view_state_bridge.camera.zoom<11.5f?2:app->view_state_bridge.camera.zoom<13.5f?1:0;
    lod=lod<vectors.min_lod?vectors.min_lod:lod;
    TerrainVectorArea *a=&vectors.pack.areas[area];
    VectorWindow fine=window(a,lod,xmin,ymin,xmax,ymax);
    while(lod<2 && fine.keys>20) fine=window(a,++lod,xmin,ymin,xmax,ymax);
    VectorWindow coarse=window(a,2,xmin,ymin,xmax,ymax);
    // Four foundation keys plus twenty detail keys fit the existing 24-entry cache.
    if(lod<2 && coarse.keys>4){lod=2;fine=coarse;}
    unsigned loaded=0,coarse_pending=0,fine_pending=0;
    SDL_FRect view=app_map_viewport_rect(app);
    VectorTile *fallback[20], *visible[20];
    int fallback_count=load_window(area,2,coarse,fallback,&loaded,&coarse_pending);
    int visible_count=fallback_count;
    memcpy(visible,fallback,(size_t)fallback_count*sizeof(*visible));
    if(lod<2 && !coarse_pending) {
        VectorTile *detail[20];
        int count=load_window(area,lod,fine,detail,&loaded,&fine_pending);
        // Retain coherent coarse contours until the complete incoming visible level is ready.
        if(!fine_pending){visible_count=count;memcpy(visible,detail,(size_t)count*sizeof(*visible));}
    }
    vectors.pending=coarse_pending+fine_pending;
    unsigned needed=0;
    for(int k=0;k<visible_count;k++)for(size_t i=0;i<visible[k]->count;i++) {
        TerrainVectorSegment seg=visible[k]->segments[i];float x0,y0,x1,y1;
        app_map_world_to_viewport_local(app,a->extent[0]+seg.x0,a->extent[3]+seg.y0,&x0,&y0);
        app_map_world_to_viewport_local(app,a->extent[0]+seg.x1,a->extent[3]+seg.y1,&x1,&y1);
        if(fmaxf(x0,x1)>=0&&fminf(x0,x1)<=view.w&&fmaxf(y0,y1)>=0&&fminf(y0,y1)<=view.h)needed++;
    }
    if(needed>VECTOR_DRAW_LIMIT && lod<2) {
        vectors.min_lod=lod+1;app->terrain_pending=true;
        visible_count=fallback_count;memcpy(visible,fallback,(size_t)fallback_count*sizeof(*visible));
    }
    for(int k=0;k<visible_count;k++) {
        VectorTile *t=visible[k];
        SDL_FPoint strip[1024];int strip_count=0;float height=-1;bool strip_major=false;
        for(size_t i=0;i<t->count&&vectors.drawn<VECTOR_DRAW_LIMIT;i++) {
            TerrainVectorSegment s=t->segments[i];float x0,y0,x1,y1;
            app_map_world_to_viewport_local(app,a->extent[0]+s.x0,a->extent[3]+s.y0,&x0,&y0);
            app_map_world_to_viewport_local(app,a->extent[0]+s.x1,a->extent[3]+s.y1,&x1,&y1);
            if(fmaxf(x0,x1)<0||fminf(x0,x1)>view.w||fmaxf(y0,y1)<0||fminf(y0,y1)>view.h){strip_draw(app,strip,strip_count,strip_major);strip_count=0;continue;}
            bool major=(int)s.elevation%100==0;
            if(strip_count && (height!=s.elevation || strip_count==1024 ||
               hypotf(strip[strip_count-1].x-x0,strip[strip_count-1].y-y0)>.02f)) {
                strip_draw(app,strip,strip_count,strip_major);strip_count=0;
            }
            if(!strip_count){strip[strip_count++]=(SDL_FPoint){x0,y0};height=s.elevation;strip_major=major;}
            strip[strip_count++]=(SDL_FPoint){x1,y1};vectors.drawn++;
            if(major&&vectors.elevation_count<12&&hypotf(x1-x0,y1-y0)>4) {
                float x=(x0+x1)*.5f+view.x,y=(y0+y1)*.5f+view.y;
                bool far=true;for(int j=0;j<vectors.elevation_count;j++)if(hypotf(x-vectors.elevations[j].x,y-vectors.elevations[j].y)<180)far=false;
                if(far){int n=vectors.elevation_count++;vectors.elevations[n].x=x;vectors.elevations[n].y=y;vectors.elevations[n].elevation=s.elevation;}
            }
        }
        strip_draw(app,strip,strip_count,strip_major);
    }
    if(vectors.pending)app->terrain_pending=true;
    size_t resident_bytes=0;unsigned entries=0;
    for(int i=0;i<VECTOR_CACHE;i++){resident_bytes+=vectors.tiles[i].count*sizeof(TerrainVectorSegment);entries+=vectors.tiles[i].occupied;}
    if(resident_bytes>vectors.peak_bytes)vectors.peak_bytes=resident_bytes;
    if(entries>vectors.peak_entries)vectors.peak_entries=entries;
    if(vectors.drawn>vectors.peak_lines)vectors.peak_lines=vectors.drawn;
    if(vectors.frame%300==0) {
        size_t bytes=0;unsigned entries=0;for(int i=0;i<VECTOR_CACHE;i++){bytes+=vectors.tiles[i].count*sizeof(TerrainVectorSegment);entries+=vectors.tiles[i].occupied;}
        SDL_Log("[terrain vectors] resident=%u decoded_bytes=%zu lines=%u pending=%u lod=%d",entries,bytes,vectors.drawn,vectors.pending,lod);
    }
}
// Rejects labels at pane boundaries, the attribution footer and earlier label placements.
static bool room(SDL_FRect box,SDL_FRect view) {
    if(box.x<view.x+10||box.y<view.y+10||box.x+box.w>view.x+view.w-10||box.y+box.h>view.y+view.h-50)return false;
    for(int i=0;i<vectors.occupied_count;i++)if(SDL_HasIntersectionF(&box,&vectors.occupied[i]))return false;
    return vectors.occupied_count<72;
}
// Draws a fixed-size native glyph label with a light halo rather than a stretched map-image label.
static void name_draw(AppState *app,float x,float y,const char *name,bool water) {
    float width=fminf(220,ui_measure_text_width(name,.72f));SDL_FRect box={x-3,y-2,width+8,21};
    if(!room(box,app_map_viewport_rect(app)))return;
    vectors.occupied[vectors.occupied_count++]=box;
    SDL_Color halo={244,244,226,255},ink=water?(SDL_Color){38,91,118,255}:(SDL_Color){44,62,47,255};
    ui_draw_text_clipped(&app->renderer,x-1,y,name,halo,.72f,width+2);ui_draw_text_clipped(&app->renderer,x+1,y,name,halo,.72f,width+2);
    ui_draw_text_clipped(&app->renderer,x,y-1,name,halo,.72f,width+2);ui_draw_text_clipped(&app->renderer,x,y+1,name,halo,.72f,width+2);
    ui_draw_text_clipped(&app->renderer,x,y,name,ink,.72f,width+2);
}
// Orders neutral names by distance from the visible center within each feature priority.
typedef struct LabelPriority { size_t index; double distance; } LabelPriority;
// Provides deterministic stable-ID tie breaking for equal geographic priority.
static int compare_labels(const void *a,const void *b) {
    const LabelPriority *x=a,*y=b;return x->distance<y->distance?-1:x->distance>y->distance?1:x->index<y->index?-1:x->index>y->index;
}
bool terrain_vectors_peak_query(const char *query,size_t ordinal,char *name,size_t cap,double *lat,double *lon) {
    if(!ready())return false;
    for(size_t i=0;i<vectors.pack.label_count;i++) {
        TerrainVectorLabel *l=&vectors.pack.labels[i];
        if(strcmp(l->kind,"Summit") || (*query&&!strcasestr(l->name,query)))continue;
        if(ordinal--!=0)continue;
        LatLon p=mercator_to_latlon((MercatorMeters){l->x,l->y});snprintf(name,cap,"%s",l->name);*lat=p.lat;*lon=p.lon;return true;
    }
    return false;
}
void terrain_vectors_labels_draw(AppState *app) {
    if(!terrain_active(app)||terrain_render_mode()!=0 || !history_ui_geographic_labels_enabled(app))return;
    SDL_FRect view=app_map_viewport_rect(app);vectors.occupied_count=0;
    if(!ready()) {
        ui_draw_text_clipped(&app->renderer,view.x+16,view.y+12,"Vector layer unavailable · Reload terrain",(SDL_Color){58,73,58,255},.7f,view.w-32);return;
    }
    // Reserve personal peak-name placements so basemap names cannot obscure visited-peak evidence.
    struct json_object *catalog=NULL;json_object_object_get_ex(app->pins_file.source_json,"hiking",&catalog);
    for(size_t i=0;catalog&&i<json_object_array_length(catalog)&&vectors.occupied_count<24;i++) {
        struct json_object *v=json_object_array_get_idx(catalog,i),*kind=NULL,*la=NULL,*lo=NULL,*name=NULL;
        json_object_object_get_ex(v,"kind",&kind);if(!kind||strcmp(json_object_get_string(kind),"peak") || !history_ui_overlay_visible(app,v))continue;
        json_object_object_get_ex(v,"lat",&la);json_object_object_get_ex(v,"lon",&lo);json_object_object_get_ex(v,"name",&name);
        MercatorMeters m=mercator_from_latlon((LatLon){json_object_get_double(la),json_object_get_double(lo)});float x,y;app_map_world_to_screen(app,m.x,m.y,&x,&y);
        SDL_FRect box={x-4,y-16,fminf(210,ui_measure_text_width(name?json_object_get_string(name):"",.75f)+24),32};
        if(box.x>=view.x&&box.y>=view.y&&box.x<view.x+view.w&&box.y<view.y+view.h)vectors.occupied[vectors.occupied_count++]=box;
    }
    LabelPriority order[TERRAIN_VECTOR_LABELS];
    for(size_t i=0;i<vectors.pack.label_count;i++){TerrainVectorLabel *l=&vectors.pack.labels[i];double dx=l->x-app->view_state_bridge.camera.x,dy=l->y-app->view_state_bridge.camera.y;order[i]=(LabelPriority){i,dx*dx+dy*dy};}
    qsort(order,vectors.pack.label_count,sizeof(order[0]),compare_labels);
    int named=0;
    for(int pass=0;pass<3;pass++)for(size_t i=0;i<vectors.pack.label_count&&named<32;i++) {
        TerrainVectorLabel *l=&vectors.pack.labels[order[i].index];bool summit=!strcmp(l->kind,"Summit"),water=!strcmp(l->kind,"Lake")||!strcmp(l->kind,"Reservoir");
        if((pass==0&&!summit)||(pass==1&&!water)||(pass==2&&(summit||water)))continue;
        float zoom=app->view_state_bridge.camera.zoom;if(zoom<(pass==0?10.5f:pass==1?11.5f:13.5f))continue;
        float x,y;app_map_world_to_screen(app,l->x,l->y,&x,&y);int before=vectors.occupied_count;
        name_draw(app,x+5,y-8,l->name,water);named+=vectors.occupied_count>before;
    }
    for(int i=0;i<vectors.elevation_count;i++){char text[32];snprintf(text,sizeof(text),"%d m",vectors.elevations[i].elevation);name_draw(app,vectors.elevations[i].x,vectors.elevations[i].y,text,false);}
    if(*vectors.error)ui_draw_text_clipped(&app->renderer,view.x+16,view.y+12,vectors.error,(SDL_Color){95,55,32,255},.7f,view.w-32);
}

// Bounds personal overlays to installed terrain while retaining far-away catalog metadata.
bool terrain_vectors_covers(double min_lat,double min_lon,double max_lat,double max_lon) {
    return terrain_covers(min_lat,min_lon,max_lat,max_lon);
}
