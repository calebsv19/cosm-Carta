#ifndef VK_RENDERER_COMPAT_SDL_H
#define VK_RENDERER_COMPAT_SDL_H

#include <SDL2/SDL.h>
#include "vk_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Optional compatibility helpers to redirect common SDL renderer calls to the
 * Vulkan renderer. Enable by defining VK_RENDERER_ENABLE_SDL_COMPAT before
 * including this header. Note that your render context must provide a pointer
 * to VkRenderer instead of SDL_Renderer when compatibility mode is enabled.
 */
#ifdef VK_RENDERER_ENABLE_SDL_COMPAT

#define SDL_Renderer VkRenderer

#define SDL_SetRenderDrawColor(renderer, r, g, b, a) \
    vk_renderer_set_draw_color((renderer), (r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a) / 255.0f)

#define SDL_RenderDrawLine(renderer, x0, y0, x1, y1) \
    vk_renderer_draw_line((renderer), (float)(x0), (float)(y0), (float)(x1), (float)(y1))

#define SDL_RenderDrawPoint(renderer, x, y) \
    vk_renderer_draw_point((renderer), (float)(x), (float)(y))

#define SDL_RenderDrawRect(renderer, rect) \
    vk_renderer_draw_rect((renderer), (rect))

#define SDL_RenderFillRect(renderer, rect) \
    vk_renderer_fill_rect((renderer), (rect))

#define SDL_RenderSetClipRect(renderer, rect) \
    vk_renderer_set_clip_rect((renderer), (rect))

#define SDL_RenderGetClipRect(renderer, rect) \
    vk_renderer_get_clip_rect((renderer), (rect))

#define SDL_RenderIsClipEnabled(renderer) \
    vk_renderer_is_clip_enabled((renderer))

#endif // VK_RENDERER_ENABLE_SDL_COMPAT

#ifdef __cplusplus
}
#endif

#endif // VK_RENDERER_COMPAT_SDL_H
