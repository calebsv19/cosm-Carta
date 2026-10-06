#include "app/app_internal.h"
#include "app/app_utility_workspace.h"
#include "app/terrain.h"
#include "map/terrain_pack.h"
#include "map/mercator.h"
#include "render/image.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Tags textures with source and level so wrong-area fallback cannot pass the fixture.
typedef struct Image { int area, lod, col, row; } Image;
static pthread_t owner;
static int current_area=1, draws, fine_draws, uploads, releases;
static bool fail_upload, damaged;
static size_t gpu_allocation = TERRAIN_RGBA_BYTES;
// Supplies two non-overlapping admitted areas without depending on arbitrary filesystem state.
bool terrain_catalog_load(TerrainCatalog *c,const char *region,const char *storage,bool base,
                          char *error,size_t cap) {
    (void)region;(void)storage;(void)base;(void)error;(void)cap;
    memset(c,0,sizeof(*c));c->count=2;
    for(int i=0;i<2;i++) {
        TerrainArea *a=&c->areas[i];a->bytes=4096;a->resolution_m=10;
        snprintf(a->id,sizeof(a->id),"area-%d",i);snprintf(a->raster_path,sizeof(a->raster_path),"%c",'A'+i);
        a->extent[0]=i*10000-1000;a->extent[2]=i*10000+1000;a->extent[1]=-1000;a->extent[3]=1000;
    }
    return true;
}
// Selects solely by the matching geographic view, never by the previous resident source.
int terrain_catalog_select(const TerrainCatalog *c,double x0,double y0,double x1,double y1) {
    (void)c;(void)y0;(void)x1;(void)y1;return x0>20000?-1:x0>5000?1:0;
}
// Keeps removal outside this non-destructive render fixture.
bool terrain_catalog_remove(TerrainCatalog *c,size_t i,char *e,size_t n){(void)c;(void)i;(void)e;(void)n;return false;}
// Supplies an exact source footprint and five levels using production-style immutable metadata.
bool terrain_pack_open(TerrainPack *p,const char *path,char *e,size_t n) {
    (void)e;(void)n;memset(p,0,sizeof(*p));int area=!strcmp(path,"A")?1:2;
    p->db=(sqlite3 *)(uintptr_t)area;p->bytes=4096;p->style_count=2;
    for(int i=0;i<2;i++) {
        TerrainStyle *s=&p->styles[i];s->mode=i;
        s->extent[0]=(area-1)*10000-1000;s->extent[2]=(area-1)*10000+1000;
        s->extent[1]=-1000;s->extent[3]=1000;
        for(int l=0;l<5;l++){int w=512>>l;s->levels[l]=(TerrainLevel){w,w,(w+255)/256,(w+255)/256};}
    }
    return true;
}
// Releases only the fixture metadata; the actual worker owns the read lifetime.
void terrain_pack_close(TerrainPack *p){memset(p,0,sizeof(*p));}
// Keeps the same style in this coverage fixture to isolate geographic cache identities.
int terrain_pack_select(const TerrainPack *p,int mode,double x0,double y0,double x1,double y1){(void)p;(void)x0;(void)y0;(void)x1;(void)y1;return mode;}
// Proves decompression is on the worker and tags all geographic key dimensions in the result.
bool terrain_pack_tile(TerrainPack *p,int style,int lod,int col,int row,unsigned char *rgba) {
    (void)style;assert(!pthread_equal(owner,pthread_self()));memset(rgba,0,TERRAIN_RGBA_BYTES);
    rgba[0]=(unsigned char)(uintptr_t)p->db;rgba[1]=lod;rgba[2]=col;rgba[3]=row;return true;
}
const char *region_data_root(void){return "/fixture";}
// Uses a small visible footprint centered on the selected synthetic geographic area.
bool camera_visible_world_aabb(const Camera *c,int w,int h,float *x0,float *y0,float *x1,float *y1){(void)w;(void)h;*x0=c->x-100;*x1=c->x+100;*y0=-100;*y1=100;return true;}
float camera_pixels_per_meter(const Camera *c){(void)c;return 1;}
bool app_map_world_to_viewport_local(const AppState *a,float x,float y,float *sx,float *sy){*sx=x-a->view_state_bridge.camera.x+100;*sy=100-y;return true;}
SDL_FRect app_map_viewport_rect(const AppState *a){(void)a;return (SDL_FRect){0,0,200,200};}
bool renderer_image_ready(Renderer *r,RendererImage *i){(void)r;return i->gpu!=NULL;}
// Requires texture creation on the renderer owner and supports a terminal upload failure.
bool renderer_image_upload(Renderer *r,RendererImage *i,const unsigned char *rgba,int w,int h) {
    (void)r;assert(pthread_equal(owner,pthread_self()));assert(w==258&&h==258);
    if(fail_upload){fail_upload=false;return false;}
    Image *im=malloc(sizeof(*im));assert(im);*im=(Image){rgba[0],rgba[1],rgba[2],rgba[3]};
    i->gpu=im;i->gpu_bytes=gpu_allocation;uploads++;return true;
}
// Rejects wrong-source texels, stretched footprints and detail appearing without a backdrop.
bool renderer_image_draw(Renderer *r,RendererImage *i,const SDL_FPoint p[4]) {
    (void)r;if(!i->gpu)return false;Image *im=i->gpu;assert(im->area==current_area);
    float size=(float)(2000.0/(512>>im->lod)*256);
    assert(fabsf(p[1].x-p[0].x-size)<.01f&&fabsf(p[3].y-p[0].y-size)<.01f);
    assert(fabsf(p[0].x-(-900+im->col*size))<.01f&&fabsf(p[0].y-(-900+im->row*size))<.01f);
    if(im->lod<4){
        TerrainPresentationStats s=terrain_presentation_stats();
        if(!damaged && s.detail_ready<s.detail_needed)assert(draws>0);
        fine_draws++;
    }draws++;return true;
}
void renderer_image_release(Renderer *r,RendererImage *i){(void)r;assert(pthread_equal(owner,pthread_self()));if(i->gpu){free(i->gpu);releases++;}memset(i,0,sizeof(*i));}
void renderer_set_draw_color(Renderer *r,uint8_t a,uint8_t b,uint8_t c,uint8_t d){(void)r;(void)a;(void)b;(void)c;(void)d;}
void renderer_fill_rect(Renderer *r,const SDL_FRect *p){(void)r;(void)p;}
void ui_draw_text_clipped(Renderer *r,int x,int y,const char *s,SDL_Color c,float z,int w){(void)r;(void)x;(void)y;(void)s;(void)c;(void)z;(void)w;}
void terrain_vectors_shutdown(void){}
MercatorMeters mercator_from_latlon(LatLon p){return (MercatorMeters){p.lon,p.lat};}
// Settles asynchronous loading while enforcing cache, decode and per-frame GPU budgets.
static void settle(AppState *a) {
    for(int n=0;n<2000;n++) {
        draws=fine_draws=0;int before=uploads;terrain_draw(a);
        TerrainPresentationStats s=terrain_presentation_stats();
        assert(s.resident_tiles<=48&&s.worker_slots<=8&&s.gpu_bytes<=32u*1024*1024);
        assert(uploads-before<=2);
        if(fine_draws&&!damaged)assert(s.coarse_ready==s.coarse_needed);
        if(!a->terrain_pending)return;SDL_Delay(1);
    }
    assert(!"terrain did not settle");
}
// Proves coarse-first presentation, warm A/B/A return, missing coverage, failures and clean shutdown.
int main(void) {
    owner=pthread_self();unsetenv("MAPFORGE_TERRAIN_PATH");AppState *a=calloc(1,sizeof(*a));assert(a);
    a->ui_state_bridge.utility_workspace=APP_UTILITY_HIKING_HISTORY;a->renderer.width=a->renderer.height=200;
    terrain_set_style(0);settle(a);assert(terrain_presentation_stats().coarse_ready==1&&fine_draws>0);
    current_area=2;a->view_state_bridge.camera.x=10000;settle(a);
    int before=uploads;current_area=1;a->view_state_bridge.camera.x=0;draws=fine_draws=0;terrain_draw(a);
    assert(draws>0&&!a->terrain_pending&&uploads==before); /* warm cache survives source switching */
    a->view_state_bridge.camera.x=30000;draws=0;terrain_draw(a);assert(!draws&&!terrain_active(a)&&!a->terrain_pending);
    a->view_state_bridge.camera.x=0;terrain_set_style(2);terrain_draw(a);assert(!a->terrain_pending);
    terrain_set_style(0);assert(terrain_reload(a));fail_upload=damaged=true;settle(a);assert(!a->terrain_pending);
    assert(terrain_reload(a));damaged=false;settle(a);assert(terrain_presentation_stats().coarse_ready==1);
    gpu_allocation=10u*1024*1024;assert(terrain_reload(a));settle(a);
    assert(terrain_presentation_stats().gpu_bytes<=32u*1024*1024&&!a->terrain_pending);
    gpu_allocation=TERRAIN_RGBA_BYTES;assert(terrain_reload(a));settle(a);
    terrain_shutdown(a);assert(uploads==releases);free(a);
    puts("terrain presentation: coarse-before-detail, exact geographic footprints, warm A/B/A return, missing coverage, two-upload bound, terminal failure/reload and clean disposal passed");
}
