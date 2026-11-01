#include "ui/text_draw.h"

#include "kit_render_external_text.h"
#include "text_draw_sdl.h"
#include "text_draw_fallback.h"
static Renderer *measurement_renderer;

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>

#if defined(MAPFORGE_HAVE_VK)
#include <vulkan/vulkan.h>
#include "vk_renderer.h"
#endif

#if defined(MAPFORGE_HAVE_VK)
// Reuses the shared bounded text cache while preserving and intersecting the caller's crop.
static int map_forge_text_draw_utf8_clipped_vulkan(Renderer *renderer,
                                                   TTF_Font *font,
                                                   const char *text,
                                                   SDL_Color color,
                                                   SDL_Rect *io_dst,
                                                   int max_width) {
    if (!renderer || !renderer->vk || !font || !text || !text[0] || !io_dst || max_width <= 0) return 0;
    VkRenderer *vk = (VkRenderer *)renderer->vk;
    SDL_Rect dst = *io_dst, clip, previous = {0};
    int natural_w = 0, natural_h = 0;
    if ((dst.w <= 0 || dst.h <= 0) &&
        !map_forge_text_measure_utf8(renderer, font, text, &natural_w, &natural_h)) return 0;
    if (dst.w <= 0) dst.w = natural_w;
    if (dst.h <= 0) dst.h = natural_h;
    clip = dst;
    if (clip.w > max_width) clip.w = max_width;
    SDL_Rect visible = clip;
    SDL_bool clipped = vk_renderer_is_clip_enabled(vk);
    if (clipped) {
        vk_renderer_get_clip_rect(vk, &previous);
        if (!SDL_IntersectRect(&previous, &clip, &clip)) {
            *io_dst = visible;
            return 1;
        }
    }
    vk_renderer_set_clip_rect(vk, &clip);
    int fallback=map_forge_fallback_draw(renderer,font,text,color,&dst);
    int ok=fallback ? fallback>0 : kit_render_external_text_draw_utf8((SDL_Renderer *)vk,font,text,color,&dst);
    vk_renderer_set_clip_rect(vk, clipped ? &previous : NULL);
    if (ok) *io_dst = visible;
    return ok;
}

#endif

void map_forge_text_register_font_source(TTF_Font *font,
                                         const char *path,
                                         int logical_point_size,
                                         int loaded_point_size,
                                         int kerning_enabled) {
    map_forge_sdl_font_register(font, path, logical_point_size, kerning_enabled);
    kit_render_external_text_register_font_source_native_pixels(font,
                                                  path,
                                                  logical_point_size,
                                                  loaded_point_size,
                                                  kerning_enabled);
}

void map_forge_text_unregister_font_source(TTF_Font *font) {
    map_forge_fallback_reset(NULL,font);
    map_forge_sdl_font_unregister(font);
    kit_render_external_text_unregister_font_source(font);
}

/* Bind density-aware measurement before any measure-only UI layout. */
void map_forge_text_bind_renderer(Renderer *renderer) { measurement_renderer=renderer; }

void map_forge_text_reset_renderer(Renderer *renderer) {
    if (measurement_renderer==renderer) measurement_renderer=NULL;
#if defined(MAPFORGE_HAVE_VK)
    if (renderer && renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk) {
        /* Retire cached descriptors only after submitted text draws complete. */
        vk_renderer_wait_idle((VkRenderer *)renderer->vk);
        map_forge_fallback_reset(renderer,NULL);
        kit_render_external_text_reset_renderer((SDL_Renderer *)renderer->vk);
    }
#else
    (void)renderer;
#endif
}

int map_forge_text_measure_utf8(Renderer *renderer,
                                TTF_Font *font,
                                const char *text,
                                int *out_w,
                                int *out_h) {
    if (!renderer) renderer=measurement_renderer;
#if defined(MAPFORGE_HAVE_VK)
    if (renderer && renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk) {
        int fallback=map_forge_fallback_measure(renderer,font,text,out_w,out_h);
        if (fallback) return fallback>0;
        return kit_render_external_text_measure_utf8((SDL_Renderer *)renderer->vk,
                                                     font,
                                                     text,
                                                     out_w,
                                                     out_h);
    }
#else
    (void)renderer;
#endif
    return map_forge_sdl_text_measure(renderer, font, text, out_w, out_h);
}

int map_forge_text_draw_utf8(Renderer *renderer,
                             TTF_Font *font,
                             const char *text,
                             SDL_Color color,
                             SDL_Rect *io_dst) {
#if defined(MAPFORGE_HAVE_VK)
    if (renderer && renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk) {
        int fallback=map_forge_fallback_draw(renderer,font,text,color,io_dst);
        if (fallback) return fallback>0;
        return kit_render_external_text_draw_utf8((SDL_Renderer *)renderer->vk,
                                                  font,
                                                  text,
                                                  color,
                                                  io_dst);
    }
#endif
    return map_forge_sdl_text_draw(renderer, font, text, color, io_dst, 0);
}

int map_forge_text_draw_utf8_clipped(Renderer *renderer,
                                     TTF_Font *font,
                                     const char *text,
                                     SDL_Color color,
                                     SDL_Rect *io_dst,
                                     int max_width) {
    /* App-owned crop semantics wrap the existing shared cached text draw. */
    if (max_width <= 0) {
        return 0;
    }
#if defined(MAPFORGE_HAVE_VK)
    if (renderer && renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk) {
        return map_forge_text_draw_utf8_clipped_vulkan(renderer,
                                                       font,
                                                       text,
                                                       color,
                                                       io_dst,
                                                       max_width);
    }
#endif
    return map_forge_sdl_text_draw(renderer, font, text, color, io_dst, max_width);
}
