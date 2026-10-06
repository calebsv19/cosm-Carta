#ifndef VK_RENDERER_DRAW_INTERNAL_H
#define VK_RENDERER_DRAW_INTERNAL_H

#include "vk_renderer.h"

/* Copies vertices into the active frame before recording the draw. */
void vk_renderer_emit_solid_vertices(VkRenderer *renderer,
                                      const float (*vertices)[6],
                                      uint32_t vertex_count);
VkResult vk_renderer_emit_textured_vertices(VkRenderer *renderer, const VkRendererTexture *texture,
                                             const float (*vertices)[8], uint32_t vertex_count);
VkResult vk_renderer_retire_frame_buffer(VkRendererFrameState *frame, VkAllocatedBuffer *buffer);
/* Call only after the frame fence or device idle establishes completion. */
void vk_renderer_release_frame_buffers(VkRenderer *renderer, VkRendererFrameState *frame,
                                        int release_storage);

#endif
