#include "app/app_internal.h"
#include "app/terrain.h"
#include "app/terrain_vectors.h"
#include "app/history_ui.h"
#include "map/terrain_vectors.h"
#include "map/mercator.h"
#include "ui/font.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned submitted, reads;
static float zoom=12.5f;
// Supplies one coarse contour cell and progressively smaller incoming cells.
bool terrain_vectors_open(TerrainVectors *p,const char *path,char *e,size_t n) {
    (void)path;(void)e;(void)n;memset(p,0,sizeof(*p));p->db=(sqlite3 *)1;p->count=1;
    TerrainVectorArea *a=&p->areas[0];a->extent[0]=a->extent[1]=-100;a->extent[2]=a->extent[3]=100;
    a->unit[0]=50;a->unit[1]=100;a->unit[2]=200;return true;
}
void terrain_vectors_close(TerrainVectors *p){memset(p,0,sizeof(*p));}
int terrain_vectors_select(const TerrainVectors *p,double x0,double y0,double x1,double y1){(void)p;(void)x0;(void)y0;(void)x1;(void)y1;return 0;}
// Makes fine levels exceed the drawing budget while the coarse foundation remains valid.
bool terrain_vectors_tile(TerrainVectors *p,int a,int lod,int c,int r,TerrainVectorSegment **out,size_t *n) {
    (void)p;(void)a;(void)c;(void)r;reads++;*n=lod==2?4:2500;
    *out=malloc(*n*sizeof(**out));assert(*out);
    for(size_t i=0;i<*n;i++)(*out)[i]=(TerrainVectorSegment){10,-10,20,-20,100};return true;
}
bool terrain_active(AppState *a){(void)a;return true;}
int terrain_render_mode(void){return 0;}
const char *terrain_vector_path(void){return "/fixture";}
const TerrainCatalog *terrain_area_catalog(void){static TerrainCatalog c;return &c;}
bool terrain_covers(double a,double b,double c,double d){(void)a;(void)b;(void)c;(void)d;return true;}
bool camera_visible_world_aabb(const Camera *c,int w,int h,float *x0,float *y0,float *x1,float *y1){(void)c;(void)w;(void)h;*x0=*y0=-75;*x1=*y1=75;return true;}
bool app_map_world_to_viewport_local(const AppState *a,float x,float y,float *sx,float *sy){(void)a;*sx=x+100;*sy=100-y;return true;}
bool app_map_world_to_screen(const AppState *a,float x,float y,float *sx,float *sy){return app_map_world_to_viewport_local(a,x,y,sx,sy);}
SDL_FRect app_map_viewport_rect(const AppState *a){(void)a;return (SDL_FRect){0,0,200,200};}
void renderer_set_draw_color(Renderer *r,uint8_t a,uint8_t b,uint8_t c,uint8_t d){(void)r;(void)a;(void)b;(void)c;(void)d;}
void renderer_draw_lines(Renderer *r,const SDL_FPoint *points,int count){(void)r;(void)points;submitted+=count-1;}
void ui_draw_text_clipped(Renderer *r,int x,int y,const char *s,SDL_Color c,float z,int w){(void)r;(void)x;(void)y;(void)s;(void)c;(void)z;(void)w;}
int ui_measure_text_width(const char *s,float z){(void)s;(void)z;return 10;}
bool history_ui_geographic_labels_enabled(AppState *a){(void)a;return false;}
bool history_ui_overlay_visible(AppState *a,struct json_object *r){(void)a;(void)r;return false;}
MercatorMeters mercator_from_latlon(LatLon p){return (MercatorMeters){p.lon,p.lat};}
LatLon mercator_to_latlon(MercatorMeters p){return (LatLon){p.y,p.x};}
// Ensures cold refinement and line-budget fallback never erase the available coarse contour layer.
int main(void) {
    AppState *a=calloc(1,sizeof(*a));assert(a);a->renderer.width=a->renderer.height=200;
    for(int pass=0;pass<2;pass++) {
        a->view_state_bridge.camera.zoom=zoom;
        for(int frame=0;frame<20;frame++) {
            submitted=0;unsigned before=reads;a->terrain_pending=false;terrain_vectors_draw(a);
            assert(reads-before<=2);assert(submitted>0&&submitted<=8000);
            assert(submitted==terrain_vectors_drawn_lines());
        }
        assert(!a->terrain_pending);zoom=14;
    }
    unsigned before=reads;terrain_vectors_draw(a);assert(reads==before);
    terrain_vectors_shutdown();free(a);
    puts("terrain contours: bounded cold refinement and dense-level fallback retain visible foundation; settled redraw performs no reads");
}
