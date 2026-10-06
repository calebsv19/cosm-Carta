#ifndef VK_RENDERER_DRAW_INTERNAL_H
#define VK_RENDERER_DRAW_INTERNAL_H
#include "vk_renderer.h"
// Copies bounded texture vertices into the current fenced frame.
VkResult vk_renderer_emit_textured_vertices(VkRenderer *renderer,const VkRendererTexture *texture,const float (*vertices)[8],uint32_t vertex_count);
#endif
