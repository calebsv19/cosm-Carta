#ifndef VK_RENDERER_CONFIG_H
#define VK_RENDERER_CONFIG_H

#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VkRendererConfig {
    VkBool32 enable_validation;
    VkBool32 prefer_discrete_gpu;
    VkSampleCountFlagBits msaa_samples;
    uint32_t frames_in_flight;
    float clear_color[4];
    const char** extra_instance_extensions;
    uint32_t extra_instance_extension_count;
    const char** extra_device_extensions;
    uint32_t extra_device_extension_count;
} VkRendererConfig;

void vk_renderer_config_set_defaults(VkRendererConfig* config);

#ifdef __cplusplus
}
#endif

#endif // VK_RENDERER_CONFIG_H
