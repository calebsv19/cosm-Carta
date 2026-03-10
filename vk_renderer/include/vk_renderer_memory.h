#ifndef VK_RENDERER_MEMORY_H
#define VK_RENDERER_MEMORY_H

#include <vulkan/vulkan.h>

#include "vk_renderer_context.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VkAllocatedBuffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    void* mapped;
} VkAllocatedBuffer;

typedef struct VkAllocatedImage {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkFormat format;
    VkExtent3D extent;
} VkAllocatedImage;

VkResult vk_renderer_memory_create_buffer(VkRendererContext* ctx,
                                          VkDeviceSize size,
                                          VkBufferUsageFlags usage,
                                          VkMemoryPropertyFlags properties,
                                          VkAllocatedBuffer* out_buffer);
void vk_renderer_memory_destroy_buffer(VkRendererContext* ctx,
                                       VkAllocatedBuffer* buffer);

VkResult vk_renderer_memory_create_image(VkRendererContext* ctx,
                                         VkExtent3D extent,
                                         VkFormat format,
                                         VkImageTiling tiling,
                                         VkImageUsageFlags usage,
                                         VkMemoryPropertyFlags properties,
                                         VkAllocatedImage* out_image);
void vk_renderer_memory_destroy_image(VkRendererContext* ctx,
                                      VkAllocatedImage* image);

uint32_t vk_renderer_memory_find_type(VkRendererContext* ctx,
                                      uint32_t type_filter,
                                      VkMemoryPropertyFlags properties);

#ifdef __cplusplus
}
#endif

#endif // VK_RENDERER_MEMORY_H
