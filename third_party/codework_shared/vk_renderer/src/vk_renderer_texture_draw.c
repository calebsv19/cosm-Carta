#include "vk_renderer_draw_internal.h"

#include <math.h>

VkResult vk_renderer_draw_textured_quad(VkRenderer *renderer, const VkRendererTexture *texture,
                                         const SDL_FRect *dst, const float uv_min[2],
                                         const float uv_max[2], const float tint[4]) {
    if (!renderer || !texture || !dst || !uv_min || !uv_max || !tint ||
        !texture->width || !texture->height || !texture->descriptor_set ||
        !texture->image.view || !texture->sampler ||
        !isfinite(dst->x) || !isfinite(dst->y) || !isfinite(dst->w) || !isfinite(dst->h) ||
        !isfinite(dst->x + dst->w) || !isfinite(dst->y + dst->h) || dst->w < 0 || dst->h < 0)
        return VK_ERROR_INITIALIZATION_FAILED;
    for (int i = 0; i < 2; ++i)
        if (!isfinite(uv_min[i]) || !isfinite(uv_max[i])) return VK_ERROR_INITIALIZATION_FAILED;
    for (int i = 0; i < 4; ++i)
        if (!isfinite(tint[i]) || tint[i] < 0 || tint[i] > 1) return VK_ERROR_INITIALIZATION_FAILED;
    if (dst->w == 0 || dst->h == 0) return VK_SUCCESS;

    float x0 = dst->x, y0 = dst->y, x1 = dst->x + dst->w, y1 = dst->y + dst->h;
    float vertices[6][8] = {
        {x0, y0, uv_min[0], uv_min[1], tint[0], tint[1], tint[2], tint[3]},
        {x1, y0, uv_max[0], uv_min[1], tint[0], tint[1], tint[2], tint[3]},
        {x1, y1, uv_max[0], uv_max[1], tint[0], tint[1], tint[2], tint[3]},
        {x0, y0, uv_min[0], uv_min[1], tint[0], tint[1], tint[2], tint[3]},
        {x1, y1, uv_max[0], uv_max[1], tint[0], tint[1], tint[2], tint[3]},
        {x0, y1, uv_min[0], uv_max[1], tint[0], tint[1], tint[2], tint[3]},
    };
    return vk_renderer_emit_textured_vertices(renderer, texture, (const float (*)[8])vertices, 6);
}

/* Preserve the legacy integer source/destination API and its white tint. */
void vk_renderer_draw_texture(VkRenderer *renderer, const VkRendererTexture *texture,
                              const SDL_Rect *src, const SDL_Rect *dst) {
    if (!texture || !texture->width || !texture->height ||
        (src && (src->w <= 0 || src->h <= 0)) || (dst && (dst->w <= 0 || dst->h <= 0))) return;
    SDL_FRect bounds = {0, 0, (float)texture->width, (float)texture->height};
    if (dst) bounds = (SDL_FRect){(float)dst->x, (float)dst->y, (float)dst->w, (float)dst->h};
    float uv_min[2] = {0, 0}, uv_max[2] = {1, 1};
    if (src) {
        uv_min[0] = (float)src->x / texture->width;
        uv_min[1] = (float)src->y / texture->height;
        uv_max[0] = ((float)src->x + src->w) / texture->width;
        uv_max[1] = ((float)src->y + src->h) / texture->height;
    }
    const float tint[4] = {1, 1, 1, 1};
    (void)vk_renderer_draw_textured_quad(renderer, texture, &bounds, uv_min, uv_max, tint);
}
