#include "render/image.h"
#include <stdlib.h>
#include <string.h>
#if defined(MAPFORGE_HAVE_VK)
#include "vk_renderer.h"
#endif
void renderer_image_release(Renderer *r, RendererImage *i) {
    if (!i)
        return;
    SDL_DestroyTexture(i->sdl);
#if defined(MAPFORGE_HAVE_VK)
    if (i->gpu && r && r->vk == i->owner &&
        i->device == (uintptr_t)((VkRenderer *)r->vk)->context.device->device) {
        VkRenderer *vk = r->vk;
        vk_renderer_device_wait_idle(vk->context.device);
        vk_renderer_texture_destroy(vk, i->gpu);
    }
#endif
    free(i->gpu);
    memset(i, 0, sizeof(*i));
}
bool renderer_image_upload(Renderer *r, RendererImage *i, const unsigned char *rgba, int w, int h) {
    if (!r || !i || !rgba || w <= 0 || h <= 0 || w > 258 || h > 258)
        return false;
    SDL_Surface *s =
        SDL_CreateRGBSurfaceWithFormatFrom((void *)rgba, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    if (!s)
        return false;
    bool ok = false;
#if defined(MAPFORGE_HAVE_VK)
    if (r->backend == RENDERER_BACKEND_VULKAN && r->vk) {
        VkRendererTexture *t = calloc(1, sizeof(*t));
        if (t && vk_renderer_upload_sdl_surface_with_filter(r->vk, s, t, VK_FILTER_LINEAR) == VK_SUCCESS) {
            i->gpu = t;
            i->owner = r->vk;
            i->device = (uintptr_t)((VkRenderer *)r->vk)->context.device->device;
            VkMemoryRequirements requirements;
            vkGetImageMemoryRequirements(((VkRenderer *)r->vk)->context.device->device, t->image.image,
                                         &requirements);
            i->gpu_bytes = (size_t)requirements.size;
            ok = true;
        } else
            free(t);
    } else
#endif
        if (r->sdl) {
        i->sdl = SDL_CreateTextureFromSurface(r->sdl, s);
        ok = i->sdl != NULL;
        if (ok) {
            SDL_SetTextureBlendMode(i->sdl, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(i->sdl, SDL_ScaleModeLinear);
        }
    }
    SDL_FreeSurface(s);
    return ok;
}
// Rejects textures left behind by backend/context recovery before dereferencing GPU handles.
bool renderer_image_ready(Renderer *r, RendererImage *i) {
    if (!r || !i)
        return false;
#if defined(MAPFORGE_HAVE_VK)
    if (r->backend == RENDERER_BACKEND_VULKAN)
        return r->vk && i->gpu && i->owner == r->vk &&
               i->device == (uintptr_t)((VkRenderer *)r->vk)->context.device->device;
#endif
    return i->sdl != NULL;
}
bool renderer_image_draw(Renderer *r, RendererImage *i, const SDL_FPoint p[4]) {
    if (!r || !i || !p)
        return false;
    const float uv0[2] = {1.0f / 258, 1.0f / 258}, uv1[2] = {257.0f / 258, 257.0f / 258};
#if defined(MAPFORGE_HAVE_VK)
    if (r->backend == RENDERER_BACKEND_VULKAN) {
        if (!r->vk || !r->vk_cmd || i->owner != r->vk ||
            i->device != (uintptr_t)((VkRenderer *)r->vk)->context.device->device || !i->gpu)
            return false;
        SDL_FPoint corners[4];
        for (int j = 0; j < 4; j++) {
            corners[j] = p[j];
            if (r->viewport_enabled) {
                corners[j].x += r->viewport_x;
                corners[j].y += r->viewport_y;
            }
        }
        return vk_renderer_draw_texture_corners(r->vk, i->gpu, corners, uv0, uv1) == VK_SUCCESS;
    }
#endif
    if (!i->sdl)
        return false;
    SDL_Vertex v[4];
    for (int j = 0; j < 4; j++) {
        v[j].position = p[j];
        v[j].color = (SDL_Color){255, 255, 255, 255};
        v[j].tex_coord = (SDL_FPoint){j == 1 || j == 2 ? uv1[0] : uv0[0], j >= 2 ? uv1[1] : uv0[1]};
    }
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    return SDL_RenderGeometry(r->sdl, i->sdl, v, 4, indices, 6) == 0;
}
