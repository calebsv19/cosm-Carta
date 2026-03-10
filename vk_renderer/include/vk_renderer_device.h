#ifndef VK_RENDERER_DEVICE_H
#define VK_RENDERER_DEVICE_H

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>

#include "vk_renderer_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VkRendererDevice {
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
} VkRendererDevice;

VkResult vk_renderer_device_init(VkRendererDevice* device,
                                 SDL_Window* window,
                                 const VkRendererConfig* config);
void vk_renderer_device_shutdown(VkRendererDevice* device);
void vk_renderer_device_wait_idle(VkRendererDevice* device);

#ifdef __cplusplus
}
#endif

#endif // VK_RENDERER_DEVICE_H
