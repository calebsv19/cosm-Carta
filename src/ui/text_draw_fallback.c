#include "text_draw_fallback.h"
#include "text_draw_sdl.h"
#include "kit_render_native_font.h"
#include <math.h>
#include <string.h>
#if defined(MAPFORGE_HAVE_VK) && defined(__APPLE__)
#include "vk_renderer.h"
/* Own bounded native fallback textures separately from ordinary shared text runs. */
typedef struct NativeRun {
    VkRenderer *renderer; TTF_Font *font; VkRendererTexture texture;
    float sx,sy; int width,height; SDL_Color ink; uint64_t used;
    char text[512];
} NativeRun;
static NativeRun runs[32];
static uint64_t clock_tick;
/* Resolve actual drawable-to-logical geometry independently on each axis. */
static void density(VkRenderer *vk,float *sx,float *sy) {
    *sx=*sy=1;
    if (vk->draw_state.logical_size[0]>0) *sx=vk->context.swapchain.extent.width/vk->draw_state.logical_size[0];
    if (vk->draw_state.logical_size[1]>0) *sy=vk->context.swapchain.extent.height/vk->draw_state.logical_size[1];
    if (!isfinite(*sx)||*sx<=0) *sx=1;
    if (!isfinite(*sy)||*sy<=0) *sy=1;
}
/* Identify a registered missing-glyph run without mutating either backend. */
static int native_run(Renderer *renderer,TTF_Font *font,const char *text,const char **path,int *points) {
    return renderer && renderer->backend==RENDERER_BACKEND_VULKAN && renderer->vk && font && text &&
        map_forge_native_font_source(font,path,points) && kit_render_native_font_needs_fallback(font,text);
}
/* Enclose the same CoreText raster admitted by native drawing. */
int map_forge_fallback_measure(Renderer *renderer,TTF_Font *font,const char *text,int *w,int *h) {
    const char *path; int points,width,height;
    if (!native_run(renderer,font,text,&path,&points)) return 0;
    if (!kit_render_native_font_measure(path,points,text,&width,&height)) return 0;
    float sx,sy;density(renderer->vk,&sx,&sy);float scale=fmaxf(sx,sy);
    double rw=ceil(fmax(width,1)*scale),rh=ceil(fmax(height,1)*scale);
    if (rw>65536 || rh>8192 || rw*rh>(16u<<20)) return -1;
    if(w)*w=(int)ceil(rw/sx);if(h)*h=(int)ceil(rh/sy);return 1;
}
/* Queue texture retirement behind submitted frames instead of freeing in-flight resources. */
static void retire(NativeRun *run) {
    if(run->renderer)vk_renderer_queue_texture_destroy(run->renderer,&run->texture);
    memset(run,0,sizeof(*run));
}
/* Release one font's runs or all runs of a retiring backend. */
void map_forge_fallback_reset(Renderer *renderer,TTF_Font *font) {
    for(unsigned i=0;i<32;++i)if(runs[i].renderer &&
        ((!renderer || runs[i].renderer==renderer->vk) && (!font || runs[i].font==font))) retire(&runs[i]);
}
/* Submit whole native texels through float geometry and preserve caller clipping. */
static void paint(NativeRun *run,SDL_Rect *dst) {
    SDL_FRect quad={roundf(dst->x*run->sx)/run->sx,roundf(dst->y*run->sy)/run->sy,
        run->width/run->sx,run->height/run->sy};
    const float uv0[2]={0,0},uv1[2]={1,1},white[4]={1,1,1,1};
    vk_renderer_draw_textured_quad(run->renderer,&run->texture,&quad,uv0,uv1,white);
    dst->w=(int)ceilf(quad.w);dst->h=(int)ceilf(quad.h);
}
/* Cache ordinary native runs; oversized labels use the same transient path. */
int map_forge_fallback_draw(Renderer *renderer,TTF_Font *font,const char *text,SDL_Color ink,SDL_Rect *dst) {
    const char *path;int points;
    if(!native_run(renderer,font,text,&path,&points))return 0;
    VkRenderer *vk=renderer->vk;float sx,sy;density(vk,&sx,&sy);
    for(unsigned i=0;i<32;++i)if(runs[i].renderer==vk && runs[i].font==font && runs[i].sx==sx && runs[i].sy==sy &&
        !memcmp(&runs[i].ink,&ink,sizeof(ink)) && !strcmp(runs[i].text,text)) {
        runs[i].used=++clock_tick;paint(&runs[i],dst);return 1;
    }
    SDL_Surface *surface=kit_render_native_font_rasterize(path,points,text,ink,fmaxf(sx,sy));
    if(!surface)return -1;
    NativeRun run={0};run.renderer=vk;run.font=font;run.sx=sx;run.sy=sy;run.ink=ink;
    run.width=surface->w;run.height=surface->h;
    VkResult result=vk_renderer_upload_sdl_surface_with_filter(vk,surface,&run.texture,VK_FILTER_NEAREST);
    SDL_FreeSurface(surface);if(result!=VK_SUCCESS)return -1;
    paint(&run,dst);
    if(strlen(text)<sizeof(run.text) && (size_t)run.width*run.height<=262144) {
        unsigned slot=0;
        for(unsigned i=0;i<32;++i) {
            if(!runs[i].renderer){slot=i;break;}
            if(runs[i].used<runs[slot].used)slot=i;
        }
        retire(&runs[slot]);strcpy(run.text,text);run.used=++clock_tick;runs[slot]=run;
    } else retire(&run);
    return 1;
}
#else
/* Keep SDL-only builds free of Vulkan ownership. */
int map_forge_fallback_measure(Renderer *r,TTF_Font *f,const char *s,int *w,int *h){(void)r;(void)f;(void)s;(void)w;(void)h;return 0;}
/* SDL-only dispatch already owns its native fallback. */
int map_forge_fallback_draw(Renderer *r,TTF_Font *f,const char *s,SDL_Color c,SDL_Rect *d){(void)r;(void)f;(void)s;(void)c;(void)d;return 0;}
/* No Vulkan resources exist in an SDL-only build. */
void map_forge_fallback_reset(Renderer *r,TTF_Font *f){(void)r;(void)f;}
#endif
