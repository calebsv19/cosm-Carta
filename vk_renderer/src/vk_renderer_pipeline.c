#include "vk_renderer_pipeline.h"
#include "vk_renderer_context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static VkShaderModule load_shader_module_from_path(VkDevice device, const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return VK_NULL_HANDLE;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    if (length <= 0) {
        fclose(file);
        return VK_NULL_HANDLE;
    }

    uint32_t* buffer = (uint32_t*)malloc(length);
    if (!buffer) {
        fclose(file);
        return VK_NULL_HANDLE;
    }
    fread(buffer, 1, length, file);
    fclose(file);

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = (size_t)length,
        .pCode = buffer,
    };

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &create_info, NULL, &module) != VK_SUCCESS) {
        module = VK_NULL_HANDLE;
    }

    free(buffer);
    return module;
}

static const char* shader_fallback_root(void) {
    static char cached[PATH_MAX];
    static int initialized = 0;
    if (initialized) return cached[0] ? cached : NULL;

    const char* file_path = __FILE__;
    const char* slash = strrchr(file_path, '/');
#ifdef _WIN32
    const char* backslash = strrchr(file_path, '\\');
    if (!slash || (backslash && backslash > slash)) {
        slash = backslash;
    }
#endif

    if (!slash) {
        cached[0] = '\0';
        initialized = 1;
        return NULL;
    }

    size_t length = (size_t)(slash - file_path);
    if (length >= sizeof(cached)) {
        length = sizeof(cached) - 1;
    }

    memcpy(cached, file_path, length);
    cached[length] = '\0';
    initialized = 1;
    return cached;
}

static VkShaderModule load_shader_module(VkDevice device,
                                         const char* path) {
    VkShaderModule module = load_shader_module_from_path(device, path);
    if (module != VK_NULL_HANDLE) {
        return module;
    }

#ifdef VK_RENDERER_SHADER_ROOT
    {
        char combined[PATH_MAX];
        snprintf(combined, sizeof(combined), "%s/%s", VK_RENDERER_SHADER_ROOT, path);
        module = load_shader_module_from_path(device, combined);
        if (module != VK_NULL_HANDLE) return module;
    }
#endif

    const char* root = shader_fallback_root();
    if (root && root[0]) {
        char combined[PATH_MAX];
        snprintf(combined, sizeof(combined), "%s/../%s", root, path);
        module = load_shader_module_from_path(device, combined);
        if (module != VK_NULL_HANDLE) return module;
    }

    fprintf(stderr, "Failed to load shader module: %s\n", path);
    return VK_NULL_HANDLE;
}

static void destroy_pipeline(VkDevice device, VkRendererPipeline* pipeline) {
    if (pipeline->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline->pipeline, NULL);
        pipeline->pipeline = VK_NULL_HANDLE;
    }
    if (pipeline->layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipeline->layout, NULL);
        pipeline->layout = VK_NULL_HANDLE;
    }
}

static VkPipelineColorBlendStateCreateInfo color_blend_state_single(void) {
    static VkPipelineColorBlendAttachmentState attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    static VkPipelineColorBlendStateCreateInfo state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &attachment,
    };
    return state;
}

static VkPipelineShaderStageCreateInfo shader_stage(VkShaderModule module,
                                                    VkShaderStageFlagBits stage,
                                                    const char* entry) {
    VkPipelineShaderStageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage,
        .module = module,
        .pName = entry,
    };
    return info;
}

typedef struct VkRendererVertexLayout {
    VkVertexInputBindingDescription binding;
    VkVertexInputAttributeDescription attributes[3];
    uint32_t attribute_count;
} VkRendererVertexLayout;

static VkRendererVertexLayout vertex_layout_solid(void) {
    VkRendererVertexLayout layout = {
        .binding = {
            .binding = 0,
            .stride = sizeof(float) * 6,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        .attributes = {
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0},
            {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = sizeof(float) * 2},
        },
        .attribute_count = 2,
    };
    return layout;
}

static VkRendererVertexLayout vertex_layout_textured(void) {
    VkRendererVertexLayout layout = {
        .binding = {
            .binding = 0,
            .stride = sizeof(float) * 8,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        .attributes = {
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0},
            {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = sizeof(float) * 2},
            {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = sizeof(float) * 4},
        },
        .attribute_count = 3,
    };
    return layout;
}

static VkRendererVertexLayout vertex_layout_lines(void) {
    return vertex_layout_solid();
}

static VkResult create_graphics_pipeline(VkDevice device,
                                         VkRenderPass render_pass,
                                         VkExtent2D extent,
                                         VkDescriptorSetLayout sampler_layout,
                                         VkPipelineCache pipeline_cache,
                                         const char* vert_path,
                                         const char* frag_path,
                                         const VkRendererVertexLayout* layout,
                                         VkPrimitiveTopology topology,
                                         VkPipelineLayout* out_layout,
                                         VkPipeline* out_pipeline,
                                         VkPushConstantRange* push_range,
                                         uint32_t push_count) {
    VkShaderModule vert_module = load_shader_module(device, vert_path);
    VkShaderModule frag_module = load_shader_module(device, frag_path);
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        if (vert_module) vkDestroyShaderModule(device, vert_module, NULL);
        if (frag_module) vkDestroyShaderModule(device, frag_module, NULL);
        fprintf(stderr, "create_graphics_pipeline shader load failed: %s | %s\n", vert_path, frag_path);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPipelineShaderStageCreateInfo stages[] = {
        shader_stage(vert_module, VK_SHADER_STAGE_VERTEX_BIT, "main"),
        shader_stage(frag_module, VK_SHADER_STAGE_FRAGMENT_BIT, "main"),
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = topology,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)extent.width,
        .height = (float)extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = extent,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = (uint32_t)(sizeof(dynamic_states) / sizeof(dynamic_states[0])),
        .pDynamicStates = dynamic_states,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blend = color_blend_state_single();

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &layout->binding,
        .vertexAttributeDescriptionCount = layout->attribute_count,
        .pVertexAttributeDescriptions = layout->attributes,
    };

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = sampler_layout ? 1u : 0u,
        .pSetLayouts = sampler_layout ? &sampler_layout : NULL,
        .pushConstantRangeCount = push_count,
        .pPushConstantRanges = push_range,
    };

    if (vkCreatePipelineLayout(device, &layout_info, NULL, out_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_module, NULL);
        vkDestroyShaderModule(device, frag_module, NULL);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pDynamicState = &dynamic_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = NULL,
        .pColorBlendState = &color_blend,
        .layout = *out_layout,
        .renderPass = render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    VkResult result = vkCreateGraphicsPipelines(device, pipeline_cache, 1,
                                                &pipeline_info, NULL, out_pipeline);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkCreateGraphicsPipelines failed for %s | %s: %d\n",
                vert_path, frag_path, (int)result);
    }

    vkDestroyShaderModule(device, vert_module, NULL);
    vkDestroyShaderModule(device, frag_module, NULL);

    return result;
}

VkResult vk_renderer_pipeline_create_all(VkRendererContext* ctx,
                                         VkRenderPass render_pass,
                                         VkDescriptorSetLayout sampler_layout,
                                         VkPipelineCache pipeline_cache,
                                         VkRendererPipeline pipelines[VK_RENDERER_PIPELINE_COUNT]) {
    if (!ctx || !ctx->device) return VK_ERROR_INITIALIZATION_FAILED;
    VkDevice device = ctx->device->device;
    VkExtent2D extent = ctx->swapchain.extent;

    VkRendererVertexLayout line_layout = vertex_layout_lines();

    VkPushConstantRange basic_push[1] = {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(float) * 16,
        },
    };

    VkResult result = create_graphics_pipeline(
        device, render_pass, extent, VK_NULL_HANDLE, pipeline_cache, "shaders/line.vert.spv",
        "shaders/line.frag.spv", &line_layout, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        &pipelines[VK_RENDERER_PIPELINE_LINES].layout,
        &pipelines[VK_RENDERER_PIPELINE_LINES].pipeline, basic_push, 1);
    if (result != VK_SUCCESS) return result;

    VkRendererVertexLayout solid_layout = vertex_layout_solid();

    result = create_graphics_pipeline(device, render_pass, extent, VK_NULL_HANDLE,
                                      pipeline_cache, "shaders/fill.vert.spv",
                                      "shaders/fill.frag.spv", &solid_layout,
                                      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                      &pipelines[VK_RENDERER_PIPELINE_SOLID].layout,
                                      &pipelines[VK_RENDERER_PIPELINE_SOLID].pipeline, basic_push, 1);
    if (result != VK_SUCCESS) return result;

    VkRendererVertexLayout textured_layout = vertex_layout_textured();

    VkPushConstantRange textured_push[1] = {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(float) * 16,
        },
    };

    result = create_graphics_pipeline(device, render_pass, extent, sampler_layout,
                                      pipeline_cache, "shaders/textured.vert.spv",
                                      "shaders/textured.frag.spv", &textured_layout,
                                      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                      &pipelines[VK_RENDERER_PIPELINE_TEXTURED].layout,
                                      &pipelines[VK_RENDERER_PIPELINE_TEXTURED].pipeline, textured_push, 1);

    return result;
}

void vk_renderer_pipeline_destroy_all(VkRendererContext* ctx,
                                      VkRendererPipeline pipelines[VK_RENDERER_PIPELINE_COUNT]) {
    if (!ctx || !ctx->device) return;
    VkDevice device = ctx->device->device;
    for (uint32_t i = 0; i < VK_RENDERER_PIPELINE_COUNT; ++i) {
        destroy_pipeline(device, &pipelines[i]);
    }
}
