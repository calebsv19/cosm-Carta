#include "vk_renderer.h"

VkResult vk_renderer_recover_surface(VkRenderer* renderer,
                                     SDL_Window* window,
                                     VkResult surface_result) {
    if (surface_result == VK_SUCCESS) {
        return VK_SUCCESS;
    }
    if (surface_result == VK_ERROR_OUT_OF_DATE_KHR ||
        surface_result == VK_SUBOPTIMAL_KHR) {
        return vk_renderer_recreate_swapchain(renderer, window);
    }
    return surface_result;
}
