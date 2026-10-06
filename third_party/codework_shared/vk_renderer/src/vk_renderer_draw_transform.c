#include "vk_renderer.h"

#include <math.h>

VkResult vk_renderer_set_draw_transform(VkRenderer *renderer, float tx, float ty, float sx, float sy) {
    if (!renderer || !isfinite(tx) || !isfinite(ty) || !isfinite(sx) || !isfinite(sy))
        return VK_ERROR_INITIALIZATION_FAILED;
    renderer->draw_state.transform_enabled = SDL_TRUE;
    renderer->draw_state.transform[0] = tx;
    renderer->draw_state.transform[1] = ty;
    renderer->draw_state.transform[2] = sx;
    renderer->draw_state.transform[3] = sy;
    return VK_SUCCESS;
}

void vk_renderer_reset_draw_transform(VkRenderer *renderer) {
    if (!renderer) return;
    renderer->draw_state.transform_enabled = SDL_FALSE;
    renderer->draw_state.transform[0] = 0;
    renderer->draw_state.transform[1] = 0;
    renderer->draw_state.transform[2] = 1;
    renderer->draw_state.transform[3] = 1;
}
