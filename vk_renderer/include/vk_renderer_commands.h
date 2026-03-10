#ifndef VK_RENDERER_COMMANDS_H
#define VK_RENDERER_COMMANDS_H

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

struct VkRenderer;

typedef struct VkRendererCommandPool {
    VkCommandPool pool;
    VkCommandBuffer* buffers;
    VkFence* fences;
    VkSemaphore* image_available;
    VkSemaphore* render_finished;
    uint32_t count;
} VkRendererCommandPool;

VkResult vk_renderer_commands_init(struct VkRenderer* renderer,
                                   VkRendererCommandPool* out_pool,
                                   uint32_t frames_in_flight);
void vk_renderer_commands_destroy(struct VkRenderer* renderer,
                                  VkRendererCommandPool* pool);

VkResult vk_renderer_commands_begin_frame(struct VkRenderer* renderer,
                                          uint32_t* frame_index,
                                          VkCommandBuffer* out_cmd);
VkResult vk_renderer_commands_end_frame(struct VkRenderer* renderer,
                                        uint32_t frame_index,
                                        VkCommandBuffer cmd);

#ifdef __cplusplus
}
#endif

#endif // VK_RENDERER_COMMANDS_H
