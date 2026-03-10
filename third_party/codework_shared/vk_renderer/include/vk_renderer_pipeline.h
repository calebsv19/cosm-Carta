#ifndef VK_RENDERER_PIPELINE_H
#define VK_RENDERER_PIPELINE_H

#include <vulkan/vulkan.h>

#include "vk_renderer_context.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum VkRendererPipelineKind {
    VK_RENDERER_PIPELINE_LINES,
    VK_RENDERER_PIPELINE_SOLID,
    VK_RENDERER_PIPELINE_TEXTURED,
    VK_RENDERER_PIPELINE_COUNT
} VkRendererPipelineKind;

typedef struct VkRendererPipeline {
    VkPipelineLayout layout;
    VkPipeline pipeline;
} VkRendererPipeline;

VkResult vk_renderer_pipeline_create_all(VkRendererContext* ctx,
                                         VkRenderPass render_pass,
                                         VkDescriptorSetLayout sampler_layout,
                                         VkPipelineCache pipeline_cache,
                                         VkRendererPipeline pipelines[VK_RENDERER_PIPELINE_COUNT]);
void vk_renderer_pipeline_destroy_all(VkRendererContext* ctx,
                                      VkRendererPipeline pipelines[VK_RENDERER_PIPELINE_COUNT]);

#ifdef __cplusplus
}
#endif

#endif // VK_RENDERER_PIPELINE_H
