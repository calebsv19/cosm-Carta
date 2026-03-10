#ifndef VK_RENDERER_TEXTURES_H
#define VK_RENDERER_TEXTURES_H

#include <vulkan/vulkan.h>

#include "vk_renderer_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

struct VkRenderer;

typedef struct VkRendererTexture {
    VkAllocatedImage image;
    VkImageLayout current_layout;
    VkSampler sampler;
    VkDescriptorSet descriptor_set;
    uint32_t width;
    uint32_t height;
} VkRendererTexture;

VkResult vk_renderer_texture_create_from_rgba(struct VkRenderer* renderer,
                                              const void* pixels,
                                              uint32_t width,
                                              uint32_t height,
                                              VkFilter filter,
                                              VkRendererTexture* out_texture);
void vk_renderer_texture_destroy(struct VkRenderer* renderer,
                                 VkRendererTexture* texture);

#ifdef __cplusplus
}
#endif

#endif // VK_RENDERER_TEXTURES_H
