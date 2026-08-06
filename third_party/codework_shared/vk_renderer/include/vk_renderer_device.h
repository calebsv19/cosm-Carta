#ifndef VK_RENDERER_DEVICE_H
#define VK_RENDERER_DEVICE_H

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>

#include "vk_renderer_config.h"
#include "vk_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VkRendererDevice {
    VkRuntime runtime;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;

    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties physical_device_properties;
    VkPhysicalDeviceMemoryProperties memory_properties;

    VkDevice device;
    uint32_t graphics_queue_family;
    uint32_t present_queue_family;
    VkQueue graphics_queue;
    VkQueue present_queue;

    VkPipelineCache pipeline_cache;
    VkBool32 validation_enabled;
    VkSurfaceKHR pending_surface;
    SDL_Window* pending_surface_window;
} VkRendererDevice;

VkResult vk_renderer_device_init(VkRendererDevice* device,
                                 SDL_Window* window,
                                 const VkRendererConfig* config);
void vk_renderer_device_shutdown(VkRendererDevice* device);
void vk_renderer_device_wait_idle(VkRendererDevice* device);
VkSurfaceKHR vk_renderer_device_take_surface(VkRendererDevice* device,
                                             SDL_Window* window);

#ifdef __cplusplus
}
#endif

#endif // VK_RENDERER_DEVICE_H
