#include "ui/text_draw.h"

#include "kit_render_external_text.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>

#if defined(MAPFORGE_HAVE_VK)
#include <vulkan/vulkan.h>
#include "vk_renderer.h"
#endif

static int map_forge_text_measure_with_ttf(TTF_Font *font,
                                           const char *text,
                                           int *out_w,
                                           int *out_h) {
    int width = 0;
    int height = 0;

    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!font || !text) {
        return 0;
    }
    if (text[0] == '\0') {
        height = TTF_FontHeight(font);
        if (height <= 0) {
            return 0;
        }
        if (out_h) *out_h = height;
        return 1;
    }
    if (TTF_SizeUTF8(font, text, &width, &height) != 0) {
        return 0;
    }
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
    return 1;
}

#if defined(MAPFORGE_HAVE_VK)
static float map_forge_text_vulkan_raster_scale(Renderer *renderer) {
    const VkRenderer *vk = NULL;
    float logical_w = 0.0f;
    float logical_h = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float raster_scale = 1.0f;

    if (!renderer || renderer->backend != RENDERER_BACKEND_VULKAN || !renderer->vk) {
        return 1.0f;
    }
    vk = (const VkRenderer *)renderer->vk;
    logical_w = vk->draw_state.logical_size[0];
    logical_h = vk->draw_state.logical_size[1];
    if (logical_w > 0.0f) {
        scale_x = (float)vk->context.swapchain.extent.width / logical_w;
    }
    if (logical_h > 0.0f) {
        scale_y = (float)vk->context.swapchain.extent.height / logical_h;
    }
    raster_scale = (scale_x > scale_y) ? scale_x : scale_y;
    if (!isfinite(raster_scale) || raster_scale < 1.0f) {
        raster_scale = 1.0f;
    }
    if (raster_scale > 4.0f) {
        raster_scale = 4.0f;
    }
    return raster_scale;
}

static VkFilter map_forge_text_upload_filter(float raster_scale) {
    if (isfinite(raster_scale) && raster_scale > 1.0f) {
        return VK_FILTER_NEAREST;
    }
    return VK_FILTER_LINEAR;
}

static int map_forge_text_draw_utf8_clipped_vulkan(Renderer *renderer,
                                                   TTF_Font *font,
                                                   const char *text,
                                                   SDL_Color color,
                                                   SDL_Rect *io_dst,
                                                   int max_width) {
    SDL_Surface *surface = NULL;
    VkRendererTexture texture = {0};
    SDL_Rect src = {0};
    SDL_Rect dst = {0};
    int logical_w = 0;
    int logical_h = 0;
    float raster_scale = 1.0f;

    if (!renderer || !renderer->vk || !font || !text || !text[0] || !io_dst || max_width <= 0) {
        return 0;
    }

    raster_scale = map_forge_text_vulkan_raster_scale(renderer);
    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        return 0;
    }

    if (!map_forge_text_measure_with_ttf(font, text, &logical_w, &logical_h)) {
        logical_w = surface->w;
        logical_h = surface->h;
    }
    dst = *io_dst;
    if (dst.w <= 0) {
        dst.w = logical_w;
    }
    if (dst.h <= 0) {
        dst.h = logical_h;
    }
    if (dst.w > max_width) {
        dst.w = max_width;
    }

    src.x = 0;
    src.y = 0;
    src.w = surface->w;
    src.h = surface->h;
    if (logical_w > 0 && dst.w < logical_w) {
        src.w = (int)lroundf((float)surface->w * ((float)dst.w / (float)logical_w));
        if (src.w < 1) {
            src.w = 1;
        }
    }

    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer->vk,
                                                   surface,
                                                   &texture,
                                                   map_forge_text_upload_filter(raster_scale)) != VK_SUCCESS) {
        SDL_FreeSurface(surface);
        return 0;
    }
    vk_renderer_draw_texture((VkRenderer *)renderer->vk, &texture, &src, &dst);
    vk_renderer_queue_texture_destroy((VkRenderer *)renderer->vk, &texture);
    SDL_FreeSurface(surface);
    *io_dst = dst;
    return 1;
}
#endif

static int map_forge_text_draw_utf8_sdl(Renderer *renderer,
                                        TTF_Font *font,
                                        const char *text,
                                        SDL_Color color,
                                        SDL_Rect *io_dst,
                                        int max_width) {
    SDL_Surface *surface = NULL;
    SDL_Texture *texture = NULL;
    SDL_Rect src = {0};
    SDL_Rect dst = {0};

    if (!renderer || !renderer->sdl || !font || !text || !text[0] || !io_dst) {
        return 0;
    }

    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        return 0;
    }
    texture = SDL_CreateTextureFromSurface(renderer->sdl, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        return 0;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    dst = *io_dst;
    if (dst.w <= 0 || dst.h <= 0) {
        if (!map_forge_text_measure_with_ttf(font, text, &dst.w, &dst.h)) {
            SDL_DestroyTexture(texture);
            return 0;
        }
    }
    if (max_width > 0 && dst.w > max_width) {
        dst.w = max_width;
    }

    src.x = 0;
    src.y = 0;
    src.w = dst.w;
    src.h = dst.h;
    if (max_width <= 0) {
        src.w = 0;
        src.h = 0;
        SDL_RenderCopy(renderer->sdl, texture, NULL, &dst);
    } else {
        if (src.w < 1) src.w = 1;
        if (src.h < 1) src.h = 1;
        SDL_RenderCopy(renderer->sdl, texture, &src, &dst);
    }
    SDL_DestroyTexture(texture);
    *io_dst = dst;
    return 1;
}

void map_forge_text_register_font_source(TTF_Font *font,
                                         const char *path,
                                         int logical_point_size,
                                         int loaded_point_size,
                                         int kerning_enabled) {
    kit_render_external_text_register_font_source(font,
                                                  path,
                                                  logical_point_size,
                                                  loaded_point_size,
                                                  kerning_enabled);
}

void map_forge_text_unregister_font_source(TTF_Font *font) {
    kit_render_external_text_unregister_font_source(font);
}

void map_forge_text_reset_renderer(Renderer *renderer) {
#if defined(MAPFORGE_HAVE_VK)
    if (renderer && renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk) {
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
#if defined(MAPFORGE_HAVE_VK)
    if (renderer && renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk) {
        return kit_render_external_text_measure_utf8((SDL_Renderer *)renderer->vk,
                                                     font,
                                                     text,
                                                     out_w,
                                                     out_h);
    }
#else
    (void)renderer;
#endif
    return map_forge_text_measure_with_ttf(font, text, out_w, out_h);
}

int map_forge_text_draw_utf8(Renderer *renderer,
                             TTF_Font *font,
                             const char *text,
                             SDL_Color color,
                             SDL_Rect *io_dst) {
#if defined(MAPFORGE_HAVE_VK)
    if (renderer && renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk) {
        return kit_render_external_text_draw_utf8((SDL_Renderer *)renderer->vk,
                                                  font,
                                                  text,
                                                  color,
                                                  io_dst);
    }
#endif
    return map_forge_text_draw_utf8_sdl(renderer, font, text, color, io_dst, 0);
}

int map_forge_text_draw_utf8_clipped(Renderer *renderer,
                                     TTF_Font *font,
                                     const char *text,
                                     SDL_Color color,
                                     SDL_Rect *io_dst,
                                     int max_width) {
    /* Clipped draw stays local for now because the shared external text
       runtime does not expose host-specific source-rect crop semantics. */
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
    return map_forge_text_draw_utf8_sdl(renderer, font, text, color, io_dst, max_width);
}
