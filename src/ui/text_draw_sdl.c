#include "text_draw_sdl.h"
#include "kit_render_native_font.h"
#include <math.h>
#include <string.h>

/* Own one optional high-density raster for each live base font. */
typedef struct NativeFont {
    TTF_Font *font, *raster;
    int points, loaded_points, kerning;
    char path[256];
} NativeFont;
static NativeFont sources[64];
/* Find the metadata registered by the existing font bank. */
static NativeFont *source_for(TTF_Font *font) {
    for (unsigned i=0;i<64;++i) if (sources[i].font==font) return &sources[i];
    return NULL;
}
/* Clear a base font's cached SDL raster before that base is closed. */
void map_forge_sdl_font_unregister(TTF_Font *font) {
    NativeFont *source=source_for(font);
    if (!source) return;
    if (source->raster) TTF_CloseFont(source->raster);
    memset(source,0,sizeof(*source));
}
/* Register only bounded paths; shared registration remains the Vulkan owner. */
void map_forge_sdl_font_register(TTF_Font *font,const char *path,int points,int kerning) {
    if (!font || !path || strlen(path)>=sizeof(sources[0].path)) return;
    map_forge_sdl_font_unregister(font);
    for (unsigned i=0;i<64;++i) if (!sources[i].font) {
        sources[i].font=font; sources[i].points=points; sources[i].kerning=kerning;
        strcpy(sources[i].path,path); return;
    }
}
/* Resolve the SDL scale without changing viewport or caller clipping. */
static void pixel_scale(Renderer *renderer,float *sx,float *sy) {
    *sx=*sy=1;
    if (renderer && renderer->sdl) SDL_RenderGetScale(renderer->sdl,sx,sy);
    if (!isfinite(*sx)||*sx<=0) *sx=1;
    if (!isfinite(*sy)||*sy<=0) *sy=1;
}
/* Open a replacement density raster before releasing the last working one. */
static TTF_Font *raster_font(NativeFont *source,TTF_Font *font,float scale) {
    if (!source || scale==1) return font;
    int points=(int)lroundf(source->points*scale);
    if (points<1 || points>512) return NULL;
    if (source->raster && source->loaded_points==points) return source->raster;
    TTF_Font *next=TTF_OpenFont(source->path,points);
    if (!next) return NULL;
    TTF_SetFontStyle(next,TTF_STYLE_NORMAL); TTF_SetFontHinting(next,TTF_HINTING_LIGHT);
    TTF_SetFontKerning(next,source->kerning);
    if (source->raster) TTF_CloseFont(source->raster);
    source->raster=next; source->loaded_points=points; return next;
}
/* Shape the same registered fallback run used by SDL drawing. */
static SDL_Surface *surface_for(NativeFont *source,TTF_Font *font,const char *text,SDL_Color ink,float scale) {
    SDL_Surface *surface=NULL;
    if (source && kit_render_native_font_needs_fallback(font,text))
        surface=kit_render_native_font_rasterize(source->path,source->points,text,ink,scale);
    return surface?surface:TTF_RenderUTF8_Blended(font,text,ink);
}
/* Enclose the exact density raster in logical SDL coordinates. */
int map_forge_sdl_text_measure(Renderer *renderer,TTF_Font *font,const char *text,int *w,int *h) {
    float sx,sy; int rw=0,rh=0;
    if (!font || !text) return 0;
    pixel_scale(renderer,&sx,&sy); NativeFont *source=source_for(font);
    TTF_Font *raster=raster_font(source,font,fmaxf(sx,sy)); if (!raster) return 0;
    if (!*text) rh=TTF_FontHeight(raster);
    else if (source && kit_render_native_font_needs_fallback(raster,text)) {
        SDL_Surface *surface=surface_for(source,raster,text,(SDL_Color){255,255,255,255},fmaxf(sx,sy));
        if (!surface) return 0;
        rw=surface->w;rh=surface->h;SDL_FreeSurface(surface);
    } else if (TTF_SizeUTF8(raster,text,&rw,&rh)!=0) return 0;
    if (w) *w=(int)ceilf(rw/sx);
    if (h) *h=(int)ceilf(rh/sy);
    return 1;
}
/* Keep whole glyph texels and intersected visibility rather than resampling a crop. */
int map_forge_sdl_text_draw(Renderer *renderer,TTF_Font *font,const char *text,SDL_Color ink,SDL_Rect *dst,int limit) {
    if (!renderer || !renderer->sdl || !font || !text || !*text || !dst) return 0;
    float sx,sy;pixel_scale(renderer,&sx,&sy);
    NativeFont *source=source_for(font);TTF_Font *raster=raster_font(source,font,fmaxf(sx,sy));
    if (!raster) return 0;
    SDL_Surface *surface=surface_for(source,raster,text,ink,fmaxf(sx,sy));if (!surface) return 0;
    SDL_Texture *texture=SDL_CreateTextureFromSurface(renderer->sdl,surface);
    int rw=surface->w,rh=surface->h;SDL_FreeSurface(surface);if (!texture) return 0;
    SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);SDL_SetTextureScaleMode(texture,SDL_ScaleModeNearest);
    SDL_Rect old_clip,viewport;SDL_RenderGetViewport(renderer->sdl,&viewport);
    SDL_bool clipped=SDL_RenderIsClipEnabled(renderer->sdl);SDL_RenderGetClipRect(renderer->sdl,&old_clip);
    SDL_Rect visible={dst->x,dst->y,(int)ceilf(rw/sx),(int)ceilf(rh/sy)};
    if (limit>0 && visible.w>limit) visible.w=limit;
    SDL_Rect clip=visible;int result=0;
    if (!clipped || SDL_IntersectRect(&clip,&old_clip,&clip)) {
        /* SDL software float quads can truncate a native texel after fractional scaling. */
        SDL_Rect quad={(int)lroundf((dst->x+viewport.x)*sx),
                       (int)lroundf((dst->y+viewport.y)*sy),rw,rh};
        SDL_Rect physical_clip=quad;
        if (limit>0) {
            int right=(int)((dst->x+viewport.x)*sx)+(int)(limit*sx);
            if (right<physical_clip.x+physical_clip.w) physical_clip.w=right-physical_clip.x;
        }
        if (clipped) {
            SDL_Rect caller={(int)((old_clip.x+viewport.x)*sx),(int)((old_clip.y+viewport.y)*sy),
                             (int)(old_clip.w*sx),(int)(old_clip.h*sy)};
            if (!SDL_IntersectRect(&physical_clip,&caller,&physical_clip)) physical_clip.w=physical_clip.h=0;
        }
        SDL_RenderSetScale(renderer->sdl,1,1);
        SDL_RenderSetViewport(renderer->sdl,NULL);
        SDL_RenderSetClipRect(renderer->sdl,&physical_clip);
        result=SDL_RenderCopy(renderer->sdl,texture,NULL,&quad)==0;
        SDL_RenderSetScale(renderer->sdl,sx,sy);
        SDL_RenderSetViewport(renderer->sdl,&viewport);
    } else result=1;
    SDL_RenderSetClipRect(renderer->sdl,clipped?&old_clip:NULL);SDL_DestroyTexture(texture);
    if (result) *dst=visible;return result;
}
/* Borrow registered source identity only for the duration of the current call. */
int map_forge_native_font_source(TTF_Font *font,const char **path,int *points) {
    NativeFont *source=source_for(font);
    if (!source) return 0;
    *path=source->path;*points=source->points;return 1;
}
