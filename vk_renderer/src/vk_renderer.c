/*
 * vk_renderer.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "vk_renderer.h"

#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_vulkan.h>

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VK_RENDERER_CAPTURE_FILENAME "vk_frame.ppm"

#define VK_RENDERER_VERTEX_BUFFER_SIZE (8 * 1024 * 1024)

#if defined(VK_RENDERER_DEBUG) && VK_RENDERER_DEBUG
#define VK_RENDERER_DEBUG_ENABLED 1
#else
#define VK_RENDERER_DEBUG_ENABLED 0
#endif

#if defined(VK_RENDERER_FRAME_DEBUG) && VK_RENDERER_FRAME_DEBUG && VK_RENDERER_DEBUG_ENABLED
#define VK_RENDERER_FRAME_DEBUG_ENABLED 1
#else
#define VK_RENDERER_FRAME_DEBUG_ENABLED 0
#endif

#if VK_RENDERER_DEBUG_ENABLED
#define VK_RENDERER_DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define VK_RENDERER_DEBUG_LOG(...)
#endif

#if VK_RENDERER_FRAME_DEBUG_ENABLED
#define VK_RENDERER_FRAME_DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define VK_RENDERER_FRAME_DEBUG_LOG(...)
#endif

static int s_logged_vertex_buffer_failure = 0;
static int s_logged_logical_size = 0;
static int s_logged_capture_dump = 0;
#if VK_RENDERER_FRAME_DEBUG_ENABLED
static uint64_t s_total_vertices_logged = 0;
static uint64_t s_total_lines_logged = 0;
#endif

static VkResult create_descriptor_resources(VkRenderer* renderer) {
    if (!renderer->context.device) return VK_ERROR_INITIALIZATION_FAILED;
    VkDevice device = renderer->context.device->device;
    VkDescriptorSetLayoutBinding sampler_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sampler_binding,
    };

    VkResult result = vkCreateDescriptorSetLayout(device, &layout_info, NULL,
                                                  &renderer->sampler_set_layout);
    if (result != VK_SUCCESS) return result;

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 4096,
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 4096,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    return vkCreateDescriptorPool(device, &pool_info, NULL,
                                  &renderer->descriptor_pool);
}

static void flush_transient_textures(VkRenderer* renderer, VkRendererFrameState* frame) {
    if (!frame || !frame->transient_textures) {
        frame->transient_texture_count = 0;
        return;
    }
    for (uint32_t i = 0; i < frame->transient_texture_count; ++i) {
        vk_renderer_texture_destroy(renderer, &frame->transient_textures[i]);
    }
    frame->transient_texture_count = 0;
}

static VkResult create_render_pass(VkRenderer* renderer) {
    if (!renderer->context.device) return VK_ERROR_INITIALIZATION_FAILED;
    VkDevice device = renderer->context.device->device;
    VkAttachmentDescription color_attachment = {
        .format = renderer->context.swapchain.image_format,
        .samples = renderer->config.msaa_samples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_reference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_reference,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    return vkCreateRenderPass(device, &render_pass_info, NULL,
                              &renderer->render_pass);
}

static void destroy_framebuffers(VkRenderer* renderer) {
    if (!renderer->swapchain_framebuffers) return;
    for (uint32_t i = 0; i < renderer->swapchain_framebuffer_count; ++i) {
        if (renderer->swapchain_framebuffers[i]) {
            vkDestroyFramebuffer(renderer->context.device->device,
                                 renderer->swapchain_framebuffers[i], NULL);
        }
    }
    free(renderer->swapchain_framebuffers);
    renderer->swapchain_framebuffers = NULL;
    renderer->swapchain_framebuffer_count = 0;
}

static VkResult create_framebuffers(VkRenderer* renderer) {
    destroy_framebuffers(renderer);

    uint32_t count = renderer->context.swapchain.image_count;
    renderer->swapchain_framebuffers =
        (VkFramebuffer*)calloc(count, sizeof(VkFramebuffer));
    if (!renderer->swapchain_framebuffers) return VK_ERROR_OUT_OF_HOST_MEMORY;

    for (uint32_t i = 0; i < count; ++i) {
        VkImageView attachments[] = {renderer->context.swapchain.image_views[i]};

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderer->render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = renderer->context.swapchain.extent.width,
            .height = renderer->context.swapchain.extent.height,
            .layers = 1,
        };

        VkResult result = vkCreateFramebuffer(renderer->context.device->device, &framebuffer_info, NULL,
                                              &renderer->swapchain_framebuffers[i]);
        if (result != VK_SUCCESS) return result;
    }

    renderer->swapchain_framebuffer_count = count;
    return VK_SUCCESS;
}

static VkResult ensure_frame_vertex_buffer(VkRenderer* renderer,
                                           VkRendererFrameState* frame,
                                           VkDeviceSize required) {
    VkDeviceSize needed = frame->vertex_offset + required;
    if (frame->vertex_buffer.buffer != VK_NULL_HANDLE &&
        needed <= frame->vertex_buffer.size) {
        return VK_SUCCESS;
    }

    VkDeviceSize new_size = VK_RENDERER_VERTEX_BUFFER_SIZE;
    while (new_size < needed) new_size *= 2;

    VkDeviceSize previous_offset = frame->vertex_offset;

    VkAllocatedBuffer new_buffer = {0};
    VkResult result = vk_renderer_memory_create_buffer(
        &renderer->context, new_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &new_buffer);
    if (result != VK_SUCCESS) {
        if (!s_logged_vertex_buffer_failure) {
            fprintf(stderr,
                    "[vulkan] vk_renderer_memory_create_buffer failed (%d) when requesting %llu bytes "
                    "(frame_index=%u).\n",
                    result, (unsigned long long)new_size, renderer->current_frame_index);
            s_logged_vertex_buffer_failure = 1;
        }
        return result;
    }
    s_logged_vertex_buffer_failure = 0;

    if (frame->vertex_buffer.buffer != VK_NULL_HANDLE && frame->vertex_offset > 0) {
        if (new_buffer.mapped && frame->vertex_buffer.mapped) {
            memcpy(new_buffer.mapped, frame->vertex_buffer.mapped, (size_t)frame->vertex_offset);
        }
        vk_renderer_memory_destroy_buffer(&renderer->context, &frame->vertex_buffer);
    }

    frame->vertex_buffer = new_buffer;
    frame->vertex_offset = previous_offset;
    return VK_SUCCESS;
}

static void push_basic_constants_affine(VkRenderer* renderer,
                                        VkCommandBuffer cmd,
                                        VkRendererPipelineKind kind,
                                        float mode_x,
                                        float t0x,
                                        float t0y,
                                        float t0z,
                                        float t1x,
                                        float t1y,
                                        float t1z) {
    float logical_w = renderer->draw_state.logical_size[0];
    float logical_h = renderer->draw_state.logical_size[1];

    if (logical_w <= 0.0f || logical_h <= 0.0f) {
        logical_w = (float)renderer->context.swapchain.extent.width;
        logical_h = (float)renderer->context.swapchain.extent.height;
    }

    float push_data[16] = {
        logical_w,
        logical_h,
        mode_x,
        0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        t0x, t0y, t0z, 0.0f,
        t1x, t1y, t1z, 0.0f,
    };

    vkCmdPushConstants(cmd,
                       renderer->pipelines[kind].layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(push_data),
                       push_data);
}

static void push_basic_constants(VkRenderer* renderer,
                                 VkCommandBuffer cmd,
                                 VkRendererPipelineKind kind) {
    push_basic_constants_affine(renderer, cmd, kind, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
}

static VkRect2D compute_active_scissor(VkRenderer* renderer) {
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = renderer->context.swapchain.extent,
    };
    if (!renderer || renderer->draw_state.clip_enabled != SDL_TRUE) {
        return scissor;
    }

    int rect_x = renderer->draw_state.clip_rect.x;
    int rect_y = renderer->draw_state.clip_rect.y;
    int rect_w = renderer->draw_state.clip_rect.w;
    int rect_h = renderer->draw_state.clip_rect.h;

    float logical_w = renderer->draw_state.logical_size[0];
    float logical_h = renderer->draw_state.logical_size[1];
    if (logical_w <= 0.0f) logical_w = (float)renderer->context.swapchain.extent.width;
    if (logical_h <= 0.0f) logical_h = (float)renderer->context.swapchain.extent.height;
    if (logical_w <= 0.0f || logical_h <= 0.0f) {
        return scissor;
    }

    float scale_x = (float)renderer->context.swapchain.extent.width / logical_w;
    float scale_y = (float)renderer->context.swapchain.extent.height / logical_h;

    int x0 = (int)floorf((float)rect_x * scale_x);
    int y0 = (int)floorf((float)rect_y * scale_y);
    int x1 = (int)ceilf((float)(rect_x + rect_w) * scale_x);
    int y1 = (int)ceilf((float)(rect_y + rect_h) * scale_y);

    int max_w = (int)renderer->context.swapchain.extent.width;
    int max_h = (int)renderer->context.swapchain.extent.height;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x0 > max_w) x0 = max_w;
    if (y0 > max_h) y0 = max_h;
    if (x1 > max_w) x1 = max_w;
    if (y1 > max_h) y1 = max_h;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;

    scissor.offset.x = x0;
    scissor.offset.y = y0;
    scissor.extent.width = (uint32_t)(x1 - x0);
    scissor.extent.height = (uint32_t)(y1 - y0);
    return scissor;
}

static void apply_active_scissor(VkRenderer* renderer, VkCommandBuffer cmd) {
    if (!renderer || cmd == VK_NULL_HANDLE) return;
    VkRect2D scissor = compute_active_scissor(renderer);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

static void flush_vertex_range(VkRenderer* renderer,
                               const VkRendererFrameState* frame,
                               VkDeviceSize offset,
                               VkDeviceSize bytes) {
#if VK_RENDERER_FRAME_DEBUG_ENABLED
    static uint32_t s_last_flush_frame = UINT32_MAX;
    static unsigned s_flush_logs = 0;
    uint32_t frame_index = renderer->current_frame_index;
    if (frame_index != s_last_flush_frame) {
        s_last_flush_frame = frame_index;
        s_flush_logs = 0;
    }
    if (s_flush_logs < 8 && bytes >= sizeof(float) * 6) {
        const float* floats =
            (const float*)((const uint8_t*)frame->vertex_buffer.mapped + offset);
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[vulkan] flush_vertex_range frame=%u offset=%llu bytes=%llu firstVertex=(%.2f, %.2f, %.2f, %.2f, %.2f, %.2f)\n",
            frame_index,
            (unsigned long long)offset,
            (unsigned long long)bytes,
            floats[0], floats[1], floats[2], floats[3],
            floats[4], floats[5]);
        s_flush_logs++;
    }
#endif
    VkMappedMemoryRange range = {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = frame->vertex_buffer.memory,
        .offset = offset,
        .size = bytes,
    };
    vkFlushMappedMemoryRanges(renderer->context.device->device, 1, &range);
}

static void emit_filled_quad(VkRenderer* renderer,
                             VkRendererFrameState* frame,
                             const float quad[6][6],
                             VkRendererPipelineKind pipeline,
                             uint32_t vertex_count);

static VkResult vk_renderer_debug_capture_create(VkRenderer* renderer) {
    VkRendererDebugCapture* capture = &renderer->debug_capture;
    memset(capture, 0, sizeof(*capture));

    capture->width = renderer->context.swapchain.extent.width;
    capture->height = renderer->context.swapchain.extent.height;

    if (capture->width == 0 || capture->height == 0) {
        return VK_SUCCESS;
    }

    VkDeviceSize size = (VkDeviceSize)capture->width * capture->height * 4u;
    VkResult result = vk_renderer_memory_create_buffer(
        &renderer->context, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &capture->buffer);
    if (result != VK_SUCCESS) {
        VK_RENDERER_DEBUG_LOG("[vulkan] debug capture buffer allocation failed: %d\n", result);
        memset(capture, 0, sizeof(*capture));
        return result;
    }

    capture->pending = VK_FALSE;
    capture->requested = VK_FALSE;
    capture->dumped = VK_FALSE;
    capture->frame_counter = 0;
    capture->frame_trigger = 20;
    return VK_SUCCESS;
}

static void vk_renderer_debug_capture_destroy(VkRenderer* renderer) {
    VkRendererDebugCapture* capture = &renderer->debug_capture;
    if (capture->buffer.buffer != VK_NULL_HANDLE) {
        vk_renderer_memory_destroy_buffer(&renderer->context, &capture->buffer);
    }
    memset(capture, 0, sizeof(*capture));
}

static SDL_PixelFormatEnum select_capture_pixel_format(VkRenderer* renderer) {
    if (!renderer) return SDL_PIXELFORMAT_BGRA8888;
    VkFormat format = renderer->context.swapchain.image_format;
    if (format == VK_FORMAT_R8G8B8A8_UNORM ||
        format == VK_FORMAT_R8G8B8A8_SRGB) {
        return SDL_PIXELFORMAT_RGBA8888;
    }
    return SDL_PIXELFORMAT_BGRA8888;
}

static int capture_ends_with_bmp(const char* path) {
    if (!path) return 0;
    const char* dot = strrchr(path, '.');
    if (!dot) return 0;
    return (dot[1] == 'b' || dot[1] == 'B') &&
           (dot[2] == 'm' || dot[2] == 'M') &&
           (dot[3] == 'p' || dot[3] == 'P') &&
           dot[4] == '\0';
}

static void vk_renderer_debug_capture_dump(VkRenderer* renderer) {
    VkRendererDebugCapture* capture = &renderer->debug_capture;
    if (!capture->requested || capture->dumped) return;

    VkDevice device = renderer->context.device->device;
    vkDeviceWaitIdle(device);

    if (!capture->buffer.mapped || capture->width == 0 || capture->height == 0) {
        capture->dumped = VK_TRUE;
        return;
    }

    const uint8_t* pixels = (const uint8_t*)capture->buffer.mapped;
    uint32_t width = capture->width;
    uint32_t height = capture->height;

    const char* output_path = capture->output_path[0] ? capture->output_path : VK_RENDERER_CAPTURE_FILENAME;
    if (capture_ends_with_bmp(output_path)) {
        SDL_PixelFormatEnum format = select_capture_pixel_format(renderer);
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
            (void*)pixels,
            (int)width,
            (int)height,
            32,
            (int)width * 4,
            format);
        if (surface) {
            if (SDL_SaveBMP(surface, output_path) != 0) {
                VK_RENDERER_DEBUG_LOG("[vulkan] failed to write capture bmp %s: %s\n",
                                      output_path, SDL_GetError());
            } else if (!s_logged_capture_dump) {
                VK_RENDERER_DEBUG_LOG("[vulkan] wrote debug capture to %s\n", output_path);
                s_logged_capture_dump = 1;
            }
            SDL_FreeSurface(surface);
        } else {
            VK_RENDERER_DEBUG_LOG("[vulkan] failed to create capture surface: %s\n",
                                  SDL_GetError());
        }
    } else {
        FILE* file = fopen(output_path, "wb");
        if (file) {
            fprintf(file, "P6\n%u %u\n255\n", width, height);
            for (uint32_t y = 0; y < height; ++y) {
                const uint8_t* row = pixels + ((size_t)y * width * 4u);
                for (uint32_t x = 0; x < width; ++x) {
                    const uint8_t* p = row + x * 4u;
                    uint8_t rgb[3] = {p[2], p[1], p[0]};
                    fwrite(rgb, 1, sizeof(rgb), file);
                }
            }
            fclose(file);
            if (!s_logged_capture_dump) {
                VK_RENDERER_DEBUG_LOG("[vulkan] wrote debug capture to %s\n", output_path);
                s_logged_capture_dump = 1;
            }
        } else {
            VK_RENDERER_DEBUG_LOG("[vulkan] failed to write debug capture file %s\n", output_path);
        }
    }

#if VK_RENDERER_DEBUG_ENABLED
    const uint8_t* first = pixels;
    const uint8_t* mid = pixels + ((size_t)(height / 2) * width + (width / 2)) * 4u;
    const uint8_t* last = pixels + ((size_t)(height - 1) * width + (width - 1)) * 4u;
    VK_RENDERER_DEBUG_LOG(
        "[vulkan] capture sample pixels tl=(%u,%u,%u,%u) center=(%u,%u,%u,%u) br=(%u,%u,%u,%u)\n",
        first ? first[0] : 0, first ? first[1] : 0, first ? first[2] : 0, first ? first[3] : 0,
        mid ? mid[0] : 0, mid ? mid[1] : 0, mid ? mid[2] : 0, mid ? mid[3] : 0,
        last ? last[0] : 0, last ? last[1] : 0, last ? last[2] : 0, last ? last[3] : 0);
#endif

    capture->dumped = VK_TRUE;
}

VkResult vk_renderer_init(VkRenderer* renderer,
                          SDL_Window* window,
                          const VkRendererConfig* config) {
    if (!renderer || !window) return VK_ERROR_INITIALIZATION_FAILED;
    memset(renderer, 0, sizeof(*renderer));
    renderer->current_frame_index = UINT32_MAX;

    VkRendererConfig local_config;
    if (config) {
        local_config = *config;
    } else {
        vk_renderer_config_set_defaults(&local_config);
    }
    renderer->config = local_config;

    VkResult result =
        vk_renderer_context_create(&renderer->context, window, &renderer->config);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_context_create failed: %d\n", (int)result);
        vk_renderer_shutdown(renderer);
        return result;
    }

    result = create_descriptor_resources(renderer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "create_descriptor_resources failed: %d\n", (int)result);
        vk_renderer_shutdown(renderer);
        return result;
    }

    result = create_render_pass(renderer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "create_render_pass failed: %d\n", (int)result);
        vk_renderer_shutdown(renderer);
        return result;
    }

    result = vk_renderer_pipeline_create_all(&renderer->context, renderer->render_pass,
                                             renderer->sampler_set_layout,
                                             renderer->context.device->pipeline_cache,
                                             renderer->pipelines);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_pipeline_create_all failed: %d\n", (int)result);
        vk_renderer_shutdown(renderer);
        return result;
    }

    result = vk_renderer_commands_init(renderer, &renderer->command_pool,
                                       renderer->config.frames_in_flight);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_commands_init failed: %d\n", (int)result);
        vk_renderer_shutdown(renderer);
        return result;
    }

    for (uint32_t i = 0; i < renderer->frame_count; ++i) {
        result = ensure_frame_vertex_buffer(renderer, &renderer->frames[i],
                                            VK_RENDERER_VERTEX_BUFFER_SIZE);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "ensure_frame_vertex_buffer failed: %d\n", (int)result);
            vk_renderer_shutdown(renderer);
            return result;
        }
    }

    result = create_framebuffers(renderer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "create_framebuffers failed: %d\n", (int)result);
        vk_renderer_shutdown(renderer);
        return result;
    }

    renderer->draw_state.current_color[0] = 1.0f;
    renderer->draw_state.current_color[1] = 1.0f;
    renderer->draw_state.current_color[2] = 1.0f;
    renderer->draw_state.current_color[3] = 1.0f;
    renderer->draw_state.logical_size[0] = (float)renderer->context.swapchain.extent.width;
    renderer->draw_state.logical_size[1] = (float)renderer->context.swapchain.extent.height;
    renderer->draw_state.clip_enabled = SDL_FALSE;
    renderer->draw_state.clip_rect = (SDL_Rect){0, 0, 0, 0};
    renderer->draw_state.draw_call_count = 0;

    vk_renderer_debug_capture_create(renderer);

    return VK_SUCCESS;
}

VkResult vk_renderer_init_with_device(VkRenderer* renderer,
                                      VkRendererDevice* device,
                                      SDL_Window* window,
                                      const VkRendererConfig* config) {
    if (!renderer || !device || !window) return VK_ERROR_INITIALIZATION_FAILED;
    memset(renderer, 0, sizeof(*renderer));
    renderer->current_frame_index = UINT32_MAX;

    VkRendererConfig local_config;
    if (config) {
        local_config = *config;
    } else {
        vk_renderer_config_set_defaults(&local_config);
    }
    renderer->config = local_config;
    renderer->context.owns_device = VK_FALSE;

    VkResult result = vk_renderer_context_create_with_device(
        &renderer->context, device, window, &renderer->config);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown_surface(renderer);
        return result;
    }

    result = create_descriptor_resources(renderer);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown_surface(renderer);
        return result;
    }

    result = create_render_pass(renderer);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown_surface(renderer);
        return result;
    }

    result = vk_renderer_pipeline_create_all(&renderer->context, renderer->render_pass,
                                             renderer->sampler_set_layout,
                                             device->pipeline_cache,
                                             renderer->pipelines);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown_surface(renderer);
        return result;
    }

    result = vk_renderer_commands_init(renderer, &renderer->command_pool,
                                       renderer->config.frames_in_flight);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown_surface(renderer);
        return result;
    }

    for (uint32_t i = 0; i < renderer->frame_count; ++i) {
        result = ensure_frame_vertex_buffer(renderer, &renderer->frames[i],
                                            VK_RENDERER_VERTEX_BUFFER_SIZE);
        if (result != VK_SUCCESS) {
            vk_renderer_shutdown_surface(renderer);
            return result;
        }
    }

    result = create_framebuffers(renderer);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown_surface(renderer);
        return result;
    }

    renderer->draw_state.current_color[0] = 1.0f;
    renderer->draw_state.current_color[1] = 1.0f;
    renderer->draw_state.current_color[2] = 1.0f;
    renderer->draw_state.current_color[3] = 1.0f;
    renderer->draw_state.logical_size[0] = (float)renderer->context.swapchain.extent.width;
    renderer->draw_state.logical_size[1] = (float)renderer->context.swapchain.extent.height;
    renderer->draw_state.clip_enabled = SDL_FALSE;
    renderer->draw_state.clip_rect = (SDL_Rect){0, 0, 0, 0};
    renderer->draw_state.draw_call_count = 0;

    vk_renderer_debug_capture_create(renderer);

    return VK_SUCCESS;
}

static VkRendererFrameState* active_frame(VkRenderer* renderer) {
    if (!renderer || renderer->current_frame_index >= renderer->frame_count) return NULL;
    return &renderer->frames[renderer->current_frame_index];
}

VkResult vk_renderer_begin_frame(VkRenderer* renderer,
                                 VkCommandBuffer* out_cmd,
                                 VkFramebuffer* out_framebuffer,
                                 VkExtent2D* out_extent) {
    if (!renderer || !out_cmd || !out_framebuffer) return VK_ERROR_INITIALIZATION_FAILED;

#if VK_RENDERER_FRAME_DEBUG_ENABLED
    static unsigned s_begin_log_count = 0;
#endif

    uint32_t frame_index = 0;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkResult result = vk_renderer_commands_begin_frame(renderer, &frame_index, &cmd);
    if (result != VK_SUCCESS) return result;

    renderer->current_frame_index = frame_index;

    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame) return VK_ERROR_INITIALIZATION_FAILED;
    flush_transient_textures(renderer, frame);
    frame->vertex_offset = 0;
    renderer->draw_state.draw_call_count = 0;

    VkExtent2D extent = renderer->context.swapchain.extent;
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)extent.width,
        .height = (float)extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkClearValue clear = {
        .color = {
            .float32 = {renderer->config.clear_color[0], renderer->config.clear_color[1],
                        renderer->config.clear_color[2], renderer->config.clear_color[3]},
        },
    };

#if VK_RENDERER_FRAME_DEBUG_ENABLED
    VK_RENDERER_FRAME_DEBUG_LOG("[vulkan] clear_color = %.2f %.2f %.2f %.2f\n",
                                clear.color.float32[0], clear.color.float32[1],
                                clear.color.float32[2], clear.color.float32[3]);
#endif

    VkFramebuffer framebuffer =
        renderer->swapchain_framebuffers[renderer->swapchain_image_index];

#if VK_RENDERER_FRAME_DEBUG_ENABLED
    if (s_begin_log_count < 120) {
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[vulkan] begin_frame frame=%u imageIndex=%u cmd=%p framebuffer=%p extent=%ux%u format=%u render_pass=%p\n",
            frame_index,
            renderer->swapchain_image_index,
            (void*)cmd,
            (void*)framebuffer,
            extent.width,
            extent.height,
            renderer->context.swapchain.image_format,
            (void*)renderer->render_pass);
        s_begin_log_count++;
    }
#endif

    VkRenderPassBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderer->render_pass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = renderer->context.swapchain.extent,
        },
        .clearValueCount = 1,
        .pClearValues = &clear,
    };

    vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    apply_active_scissor(renderer, cmd);

    *out_cmd = cmd;
    *out_framebuffer = framebuffer;
    if (out_extent) *out_extent = renderer->context.swapchain.extent;
    return VK_SUCCESS;
}

VkResult vk_renderer_end_frame(VkRenderer* renderer,
                               VkCommandBuffer cmd) {
    if (!renderer) return VK_ERROR_INITIALIZATION_FAILED;
    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame) return VK_ERROR_INITIALIZATION_FAILED;

    vkCmdEndRenderPass(cmd);

    VkRendererDebugCapture* capture = &renderer->debug_capture;
    if (capture->frame_counter < capture->frame_trigger) {
        capture->frame_counter++;
    }

    if ((capture->pending ||
         (!capture->requested && capture->buffer.buffer != VK_NULL_HANDLE &&
          capture->frame_counter >= capture->frame_trigger)) &&
        capture->buffer.buffer != VK_NULL_HANDLE) {
        VkImage image = renderer->context.swapchain.images[renderer->swapchain_image_index];

        VkImageMemoryBarrier to_transfer = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        };

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0, NULL,
                             0, NULL,
                             1, &to_transfer);

        VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {
                .width = renderer->debug_capture.width,
                .height = renderer->debug_capture.height,
                .depth = 1,
            },
        };

        vkCmdCopyImageToBuffer(cmd,
                                image,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                renderer->debug_capture.buffer.buffer,
                                1,
                                &region);

        VkImageMemoryBarrier to_present = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .dstAccessMask = 0,
        };

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0,
                             0, NULL,
                             0, NULL,
                             1, &to_present);

        capture->requested = VK_TRUE;
        capture->pending = VK_FALSE;
    }

    VkResult result =
        vk_renderer_commands_end_frame(renderer, renderer->current_frame_index, cmd);
    renderer->current_frame_index = UINT32_MAX;
    if (capture->requested && !capture->dumped) {
        if (renderer->context.device) {
            vkWaitForFences(renderer->context.device->device,
                            1,
                            &frame->in_flight_fence,
                            VK_TRUE,
                            UINT64_MAX);
        }
        vk_renderer_debug_capture_dump(renderer);
    }
    return result;
}

VkResult vk_renderer_recreate_swapchain(VkRenderer* renderer, SDL_Window* window) {
    if (!renderer || !window) return VK_ERROR_INITIALIZATION_FAILED;
    if (!renderer->context.device) return VK_ERROR_INITIALIZATION_FAILED;
    vk_renderer_device_wait_idle(renderer->context.device);

    destroy_framebuffers(renderer);
    vk_renderer_debug_capture_destroy(renderer);
    vk_renderer_pipeline_destroy_all(&renderer->context, renderer->pipelines);

    VkResult result =
        vk_renderer_context_recreate_swapchain(&renderer->context, window, &renderer->config);
    if (result != VK_SUCCESS) return result;

    result = vk_renderer_pipeline_create_all(&renderer->context, renderer->render_pass,
                                             renderer->sampler_set_layout,
                                             renderer->context.device->pipeline_cache,
                                             renderer->pipelines);
    if (result != VK_SUCCESS) return result;

    result = create_framebuffers(renderer);
    if (result != VK_SUCCESS) return result;

    vk_renderer_debug_capture_create(renderer);

    if (renderer->draw_state.logical_size[0] <= 0.0f ||
        renderer->draw_state.logical_size[1] <= 0.0f) {
        renderer->draw_state.logical_size[0] =
            (float)renderer->context.swapchain.extent.width;
        renderer->draw_state.logical_size[1] =
            (float)renderer->context.swapchain.extent.height;
    }

    return VK_SUCCESS;
}

VkResult vk_renderer_request_capture(VkRenderer* renderer, const char* output_path) {
    if (!renderer) return VK_ERROR_INITIALIZATION_FAILED;
    VkRendererDebugCapture* capture = &renderer->debug_capture;

    if (capture->buffer.buffer == VK_NULL_HANDLE) {
        VkResult init_result = vk_renderer_debug_capture_create(renderer);
        if (init_result != VK_SUCCESS) {
            return init_result;
        }
    }
    if (capture->buffer.buffer == VK_NULL_HANDLE) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    capture->pending = VK_TRUE;
    capture->requested = VK_FALSE;
    capture->dumped = VK_FALSE;
    capture->frame_counter = 0;
    capture->frame_trigger = 0;
    if (output_path) {
        snprintf(capture->output_path, sizeof(capture->output_path), "%s", output_path);
    } else {
        capture->output_path[0] = '\0';
    }
    return VK_SUCCESS;
}

void vk_renderer_set_draw_color(VkRenderer* renderer, float r, float g, float b, float a) {
    if (!renderer) return;
#if VK_RENDERER_FRAME_DEBUG_ENABLED
    static uint32_t s_last_frame = UINT32_MAX;
    static unsigned s_color_logs = 0;
    uint32_t frame = renderer->current_frame_index;
    if (frame != s_last_frame) {
        s_last_frame = frame;
        s_color_logs = 0;
    }
    if (s_color_logs < 16) {
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[vulkan] set_draw_color frame=%u rgba=%.2f %.2f %.2f %.2f\n",
            frame,
            r, g, b, a);
        s_color_logs++;
    }
#endif
    renderer->draw_state.current_color[0] = r;
    renderer->draw_state.current_color[1] = g;
    renderer->draw_state.current_color[2] = b;
    renderer->draw_state.current_color[3] = a;
}

void vk_renderer_set_logical_size(VkRenderer* renderer, float width, float height) {
    if (!renderer) return;
    renderer->draw_state.logical_size[0] = (width > 0.0f) ? width : 0.0f;
    renderer->draw_state.logical_size[1] = (height > 0.0f) ? height : 0.0f;

    if (width <= 0.0f || height <= 0.0f) {
        if (s_logged_logical_size != 1) {
            VK_RENDERER_DEBUG_LOG(
                "[vulkan] vk_renderer_set_logical_size received non-positive dimensions "
                "(%.2f, %.2f); will fall back to swapchain extent.\n",
                width, height);
            s_logged_logical_size = 1;
        }
        return;
    }

    if (s_logged_logical_size != 2) {
#if VK_RENDERER_DEBUG_ENABLED
        float extent_w = (float)renderer->context.swapchain.extent.width;
        float extent_h = (float)renderer->context.swapchain.extent.height;
        float scaleX = extent_w > 0.0f ? extent_w / width : 1.0f;
        float scaleY = extent_h > 0.0f ? extent_h / height : 1.0f;
        VK_RENDERER_DEBUG_LOG(
            "[vulkan] vk_renderer_set_logical_size %.2f x %.2f (scale %.2f x %.2f).\n",
            width, height, scaleX, scaleY);
#endif
        s_logged_logical_size = 2;
    }
}

int vk_renderer_set_clip_rect(VkRenderer* renderer, const SDL_Rect* rect) {
    if (!renderer) return -1;
    if (!rect) {
        renderer->draw_state.clip_enabled = SDL_FALSE;
        renderer->draw_state.clip_rect = (SDL_Rect){0, 0, 0, 0};
    } else {
        renderer->draw_state.clip_enabled = SDL_TRUE;
        renderer->draw_state.clip_rect = *rect;
    }
    return 0;
}

void vk_renderer_get_clip_rect(VkRenderer* renderer, SDL_Rect* rect) {
    if (!rect) return;
    if (!renderer || renderer->draw_state.clip_enabled != SDL_TRUE) {
        *rect = (SDL_Rect){0, 0, 0, 0};
        return;
    }
    *rect = renderer->draw_state.clip_rect;
}

SDL_bool vk_renderer_is_clip_enabled(VkRenderer* renderer) {
    if (!renderer) return SDL_FALSE;
    return renderer->draw_state.clip_enabled;
}

void vk_renderer_draw_point(VkRenderer* renderer, float x, float y) {
    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame) return;

    const float color[4] = {
        renderer->draw_state.current_color[0],
        renderer->draw_state.current_color[1],
        renderer->draw_state.current_color[2],
        renderer->draw_state.current_color[3],
    };

    float x0 = x;
    float y0 = y;
    float x1 = x + 1.0f;
    float y1 = y + 1.0f;

    float quad[6][6] = {
        {x0, y0, color[0], color[1], color[2], color[3]},
        {x1, y0, color[0], color[1], color[2], color[3]},
        {x1, y1, color[0], color[1], color[2], color[3]},
        {x0, y0, color[0], color[1], color[2], color[3]},
        {x1, y1, color[0], color[1], color[2], color[3]},
        {x0, y1, color[0], color[1], color[2], color[3]},
    };

    emit_filled_quad(renderer, frame, quad, VK_RENDERER_PIPELINE_SOLID, 6);
}

void vk_renderer_draw_line(VkRenderer* renderer,
                           float x0,
                           float y0,
                           float x1,
                           float y1) {
    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame) return;

    float dx = x1 - x0;
    float dy = y1 - y0;
    if (fabsf(dx) < 1.0f && fabsf(dy) < 1.0f) {
        vk_renderer_draw_point(renderer, x0, y0);
        return;
    }

    static int logged_line = 0;
    if (!logged_line) {
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[vulkan] vk_renderer_draw_line first call (%.2f, %.2f) -> (%.2f, %.2f).\n",
            x0, y0, x1, y1);
        logged_line = 1;
    }

    const float vertices[] = {
        x0, y0, renderer->draw_state.current_color[0], renderer->draw_state.current_color[1],
        renderer->draw_state.current_color[2], renderer->draw_state.current_color[3],
        x1, y1, renderer->draw_state.current_color[0], renderer->draw_state.current_color[1],
        renderer->draw_state.current_color[2], renderer->draw_state.current_color[3],
    };

    VkDeviceSize bytes = sizeof(vertices);
    if (ensure_frame_vertex_buffer(renderer, frame, bytes) != VK_SUCCESS) return;
    if (!frame->vertex_buffer.mapped) {
        static int logged_unmapped = 0;
        if (!logged_unmapped) {
            VK_RENDERER_DEBUG_LOG("[vulkan] vk_renderer_draw_line skipped: vertex buffer not mapped.\n");
            logged_unmapped = 1;
        }
        return;
    }

    uint8_t* dst = (uint8_t*)frame->vertex_buffer.mapped + frame->vertex_offset;
    memcpy(dst, vertices, bytes);

    VkDeviceSize offset = frame->vertex_offset;
    frame->vertex_offset += bytes;

    flush_vertex_range(renderer, frame, offset, bytes);

    VkBuffer buffers[] = {frame->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {offset};
    VkCommandBuffer cmd = frame->command_buffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      renderer->pipelines[VK_RENDERER_PIPELINE_LINES].pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    push_basic_constants(renderer, cmd, VK_RENDERER_PIPELINE_LINES);
    apply_active_scissor(renderer, cmd);
    vkCmdDraw(cmd, 2, 1, 0, 0);
    renderer->draw_state.draw_call_count++;

    if (renderer->debug_capture.frame_counter == renderer->debug_capture.frame_trigger) {
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[vulkan] line vertices: (%.2f, %.2f) -> (%.2f, %.2f) color=%.2f %.2f %.2f %.2f (total=%llu)\n",
            x0, y0, x1, y1,
            renderer->draw_state.current_color[0],
            renderer->draw_state.current_color[1],
            renderer->draw_state.current_color[2],
            renderer->draw_state.current_color[3],
            (unsigned long long)++s_total_lines_logged);
    }
}

void vk_renderer_draw_line_strip(VkRenderer* renderer, const SDL_FPoint* points, uint32_t count) {
    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame || !points || count < 2u) {
        return;
    }

    uint32_t segments = count - 1u;
    uint32_t vertex_count = segments * 2u;
    VkDeviceSize bytes = (VkDeviceSize)vertex_count * (VkDeviceSize)(sizeof(float) * 6u);
    if (ensure_frame_vertex_buffer(renderer, frame, bytes) != VK_SUCCESS) {
        return;
    }
    if (!frame->vertex_buffer.mapped) {
        static int logged_unmapped = 0;
        if (!logged_unmapped) {
            VK_RENDERER_DEBUG_LOG("[vulkan] vk_renderer_draw_line_strip skipped: vertex buffer not mapped.\n");
            logged_unmapped = 1;
        }
        return;
    }

    uint8_t* dst = (uint8_t*)frame->vertex_buffer.mapped + frame->vertex_offset;
    float* out = (float*)dst;
    float r = renderer->draw_state.current_color[0];
    float g = renderer->draw_state.current_color[1];
    float b = renderer->draw_state.current_color[2];
    float a = renderer->draw_state.current_color[3];

    for (uint32_t i = 0; i < segments; ++i) {
        const SDL_FPoint p0 = points[i];
        const SDL_FPoint p1 = points[i + 1u];
        *out++ = p0.x; *out++ = p0.y; *out++ = r; *out++ = g; *out++ = b; *out++ = a;
        *out++ = p1.x; *out++ = p1.y; *out++ = r; *out++ = g; *out++ = b; *out++ = a;
    }

    VkDeviceSize offset = frame->vertex_offset;
    frame->vertex_offset += bytes;
    flush_vertex_range(renderer, frame, offset, bytes);

    VkBuffer buffers[] = {frame->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {offset};
    VkCommandBuffer cmd = frame->command_buffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      renderer->pipelines[VK_RENDERER_PIPELINE_LINES].pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    push_basic_constants(renderer, cmd, VK_RENDERER_PIPELINE_LINES);
    apply_active_scissor(renderer, cmd);
    vkCmdDraw(cmd, vertex_count, 1, 0, 0);
    renderer->draw_state.draw_call_count++;
}

VkResult vk_renderer_create_line_mesh(VkRenderer* renderer,
                                      const SDL_FPoint* points,
                                      uint32_t count,
                                      float r,
                                      float g,
                                      float b,
                                      float a,
                                      VkRendererLineMesh* out_mesh) {
    if (!renderer || !out_mesh || !points || count < 2u) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t segments = count - 1u;
    uint32_t vertex_count = segments * 2u;
    SDL_FPoint* line_vertices = (SDL_FPoint*)malloc(sizeof(SDL_FPoint) * (size_t)vertex_count);
    if (!line_vertices) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    uint32_t v = 0u;
    for (uint32_t i = 0; i < segments; ++i) {
        line_vertices[v++] = points[i];
        line_vertices[v++] = points[i + 1u];
    }

    VkResult result = vk_renderer_create_line_list_mesh(
        renderer, line_vertices, vertex_count, r, g, b, a, out_mesh);
    free(line_vertices);
    return result;
}

VkResult vk_renderer_create_line_list_mesh(VkRenderer* renderer,
                                           const SDL_FPoint* vertices,
                                           uint32_t vertex_count,
                                           float r,
                                           float g,
                                           float b,
                                           float a,
                                           VkRendererLineMesh* out_mesh) {
    if (!renderer || !out_mesh || !vertices || vertex_count < 2u) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    memset(out_mesh, 0, sizeof(*out_mesh));
    if ((vertex_count & 1u) != 0u) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkDeviceSize bytes = (VkDeviceSize)vertex_count * (VkDeviceSize)(sizeof(float) * 6u);

    VkResult result = vk_renderer_memory_create_buffer(
        &renderer->context,
        bytes,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &out_mesh->vertex_buffer);
    if (result != VK_SUCCESS) {
        return result;
    }
    if (!out_mesh->vertex_buffer.mapped) {
        vk_renderer_memory_destroy_buffer(&renderer->context, &out_mesh->vertex_buffer);
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    float* out = (float*)out_mesh->vertex_buffer.mapped;
    for (uint32_t i = 0; i < vertex_count; ++i) {
        const SDL_FPoint p = vertices[i];
        *out++ = p.x; *out++ = p.y; *out++ = r; *out++ = g; *out++ = b; *out++ = a;
    }

    out_mesh->vertex_count = vertex_count;
    return VK_SUCCESS;
}

void vk_renderer_destroy_line_mesh(VkRenderer* renderer, VkRendererLineMesh* mesh) {
    if (!renderer || !mesh) {
        return;
    }
    if (mesh->vertex_buffer.buffer != VK_NULL_HANDLE) {
        vk_renderer_memory_destroy_buffer(&renderer->context, &mesh->vertex_buffer);
    }
    memset(mesh, 0, sizeof(*mesh));
}

void vk_renderer_draw_line_mesh(VkRenderer* renderer, const VkRendererLineMesh* mesh) {
    VkRendererFrameState* frame = active_frame(renderer);
    if (!renderer || !mesh || !frame || mesh->vertex_count == 0u ||
        mesh->vertex_buffer.buffer == VK_NULL_HANDLE) {
        return;
    }

    VkBuffer buffers[] = {mesh->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {0};
    VkCommandBuffer cmd = frame->command_buffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      renderer->pipelines[VK_RENDERER_PIPELINE_LINES].pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    push_basic_constants(renderer, cmd, VK_RENDERER_PIPELINE_LINES);
    apply_active_scissor(renderer, cmd);
    vkCmdDraw(cmd, mesh->vertex_count, 1, 0, 0);
    renderer->draw_state.draw_call_count++;
}

void vk_renderer_draw_line_mesh_affine(VkRenderer* renderer,
                                       const VkRendererLineMesh* mesh,
                                       float t0x,
                                       float t0y,
                                       float t0z,
                                       float t1x,
                                       float t1y,
                                       float t1z) {
    VkRendererFrameState* frame = active_frame(renderer);
    if (!renderer || !mesh || !frame || mesh->vertex_count == 0u ||
        mesh->vertex_buffer.buffer == VK_NULL_HANDLE) {
        return;
    }

    VkBuffer buffers[] = {mesh->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {0};
    VkCommandBuffer cmd = frame->command_buffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      renderer->pipelines[VK_RENDERER_PIPELINE_LINES].pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    push_basic_constants_affine(renderer, cmd, VK_RENDERER_PIPELINE_LINES, 1.0f,
                                t0x, t0y, t0z, t1x, t1y, t1z);
    apply_active_scissor(renderer, cmd);
    vkCmdDraw(cmd, mesh->vertex_count, 1, 0, 0);
    renderer->draw_state.draw_call_count++;
}

VkResult vk_renderer_create_tri_mesh(VkRenderer* renderer,
                                     const SDL_FPoint* vertices,
                                     uint32_t vertex_count,
                                     const uint32_t* indices,
                                     uint32_t index_count,
                                     float r,
                                     float g,
                                     float b,
                                     float a,
                                     VkRendererTriMesh* out_mesh) {
    if (!renderer || !out_mesh || !vertices || !indices ||
        vertex_count < 3u || index_count < 3u || (index_count % 3u) != 0u) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    memset(out_mesh, 0, sizeof(*out_mesh));

    VkDeviceSize vertex_bytes = (VkDeviceSize)vertex_count * (VkDeviceSize)(sizeof(float) * 6u);
    VkResult result = vk_renderer_memory_create_buffer(
        &renderer->context,
        vertex_bytes,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &out_mesh->vertex_buffer);
    if (result != VK_SUCCESS) {
        return result;
    }
    if (!out_mesh->vertex_buffer.mapped) {
        vk_renderer_memory_destroy_buffer(&renderer->context, &out_mesh->vertex_buffer);
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    VkDeviceSize index_bytes = (VkDeviceSize)index_count * (VkDeviceSize)sizeof(uint32_t);
    result = vk_renderer_memory_create_buffer(
        &renderer->context,
        index_bytes,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &out_mesh->index_buffer);
    if (result != VK_SUCCESS) {
        vk_renderer_memory_destroy_buffer(&renderer->context, &out_mesh->vertex_buffer);
        return result;
    }
    if (!out_mesh->index_buffer.mapped) {
        vk_renderer_memory_destroy_buffer(&renderer->context, &out_mesh->index_buffer);
        vk_renderer_memory_destroy_buffer(&renderer->context, &out_mesh->vertex_buffer);
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    float* out_v = (float*)out_mesh->vertex_buffer.mapped;
    for (uint32_t i = 0; i < vertex_count; ++i) {
        const SDL_FPoint p = vertices[i];
        *out_v++ = p.x; *out_v++ = p.y;
        *out_v++ = r;   *out_v++ = g;   *out_v++ = b; *out_v++ = a;
    }
    memcpy(out_mesh->index_buffer.mapped, indices, (size_t)index_bytes);

    out_mesh->index_count = index_count;
    return VK_SUCCESS;
}

void vk_renderer_destroy_tri_mesh(VkRenderer* renderer, VkRendererTriMesh* mesh) {
    if (!renderer || !mesh) {
        return;
    }
    if (mesh->vertex_buffer.buffer != VK_NULL_HANDLE) {
        vk_renderer_memory_destroy_buffer(&renderer->context, &mesh->vertex_buffer);
    }
    if (mesh->index_buffer.buffer != VK_NULL_HANDLE) {
        vk_renderer_memory_destroy_buffer(&renderer->context, &mesh->index_buffer);
    }
    memset(mesh, 0, sizeof(*mesh));
}

void vk_renderer_draw_tri_mesh(VkRenderer* renderer, const VkRendererTriMesh* mesh) {
    vk_renderer_draw_tri_mesh_affine(renderer, mesh, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
}

void vk_renderer_draw_tri_mesh_affine(VkRenderer* renderer,
                                      const VkRendererTriMesh* mesh,
                                      float t0x,
                                      float t0y,
                                      float t0z,
                                      float t1x,
                                      float t1y,
                                      float t1z) {
    VkRendererFrameState* frame = active_frame(renderer);
    if (!renderer || !mesh || !frame || mesh->index_count == 0u ||
        mesh->vertex_buffer.buffer == VK_NULL_HANDLE ||
        mesh->index_buffer.buffer == VK_NULL_HANDLE) {
        return;
    }

    VkBuffer buffers[] = {mesh->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {0};
    VkCommandBuffer cmd = frame->command_buffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      renderer->pipelines[VK_RENDERER_PIPELINE_SOLID].pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(cmd, mesh->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    push_basic_constants_affine(renderer, cmd, VK_RENDERER_PIPELINE_SOLID, 1.0f,
                                t0x, t0y, t0z, t1x, t1y, t1z);
    apply_active_scissor(renderer, cmd);
    vkCmdDrawIndexed(cmd, mesh->index_count, 1, 0, 0, 0);
    renderer->draw_state.draw_call_count++;
}

static void emit_filled_quad(VkRenderer* renderer,
                             VkRendererFrameState* frame,
                             const float quad[6][6],
                             VkRendererPipelineKind pipeline,
                             uint32_t vertex_count) {
    static int logged_quad = 0;
    if (!renderer->pipelines[pipeline].pipeline || !renderer->pipelines[pipeline].layout) {
        static int logged_missing = 0;
        if (!logged_missing) {
            VK_RENDERER_DEBUG_LOG("[vulkan] emit_filled_quad skipped: pipeline not ready.\n");
            logged_missing = 1;
        }
        return;
    }
    VkDeviceSize bytes = sizeof(float) * 6 * vertex_count;
    if (ensure_frame_vertex_buffer(renderer, frame, bytes) != VK_SUCCESS) return;
    if (!frame->vertex_buffer.mapped) {
        static int logged_unmapped = 0;
        if (!logged_unmapped) {
            VK_RENDERER_DEBUG_LOG("[vulkan] emit_filled_quad skipped: vertex buffer not mapped.\n");
            logged_unmapped = 1;
        }
        return;
    }

    uint8_t* dst = (uint8_t*)frame->vertex_buffer.mapped + frame->vertex_offset;
    memcpy(dst, quad, (size_t)bytes);

    VkDeviceSize offset = frame->vertex_offset;
    frame->vertex_offset += bytes;

    flush_vertex_range(renderer, frame, offset, bytes);

    VkBuffer buffers[] = {frame->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {offset};
    VkCommandBuffer cmd = frame->command_buffer;

    if (!logged_quad) {
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[vulkan] emit_filled_quad pipeline=%p vertex_buffer=%p first vertex=(%.2f, %.2f) color=%.2f\n",
            (void*)renderer->pipelines[pipeline].pipeline,
            (void*)frame->vertex_buffer.buffer,
            quad[0][0], quad[0][1], quad[0][2]);
        logged_quad = 1;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      renderer->pipelines[pipeline].pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    push_basic_constants(renderer, cmd, pipeline);
    apply_active_scissor(renderer, cmd);
    vkCmdDraw(cmd, vertex_count, 1, 0, 0);
    renderer->draw_state.draw_call_count++;
    if (renderer->debug_capture.frame_counter == renderer->debug_capture.frame_trigger) {
#if VK_RENDERER_FRAME_DEBUG_ENABLED
        const float* floats = (const float*)quad;
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[vulkan] quad sample v0=(%.2f, %.2f, %.2f, %.2f) v2=(%.2f, %.2f, %.2f, %.2f) (total=%llu)\n",
            floats[0], floats[1], floats[2], floats[3],
            floats[12], floats[13], floats[14], floats[15],
            (unsigned long long)(s_total_vertices_logged += vertex_count));
#endif
    }
}

void vk_renderer_draw_rect(VkRenderer* renderer, const SDL_Rect* rect) {
    if (!rect) return;
    float x = (float)rect->x;
    float y = (float)rect->y;
    float w = (float)rect->w;
    float h = (float)rect->h;

    vk_renderer_draw_line(renderer, x, y, x + w, y);
    vk_renderer_draw_line(renderer, x + w, y, x + w, y + h);
    vk_renderer_draw_line(renderer, x + w, y + h, x, y + h);
    vk_renderer_draw_line(renderer, x, y + h, x, y);
}

void vk_renderer_fill_rect(VkRenderer* renderer, const SDL_Rect* rect) {
    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame || !rect) return;

    static int logged_fill = 0;
    if (!logged_fill) {
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[vulkan] vk_renderer_fill_rect first call x=%d y=%d w=%d h=%d color=%.2f,%.2f,%.2f,%.2f\n",
            rect->x, rect->y, rect->w, rect->h,
            renderer->draw_state.current_color[0],
            renderer->draw_state.current_color[1],
            renderer->draw_state.current_color[2],
            renderer->draw_state.current_color[3]);
        logged_fill = 1;
    }

    float x = (float)rect->x;
    float y = (float)rect->y;
    float w = (float)rect->w;
    float h = (float)rect->h;

    const float color[4] = {
        renderer->draw_state.current_color[0],
        renderer->draw_state.current_color[1],
        renderer->draw_state.current_color[2],
        renderer->draw_state.current_color[3],
    };

    float quad[6][6] = {
        {x, y, color[0], color[1], color[2], color[3]},
        {x + w, y, color[0], color[1], color[2], color[3]},
        {x + w, y + h, color[0], color[1], color[2], color[3]},
        {x, y, color[0], color[1], color[2], color[3]},
        {x + w, y + h, color[0], color[1], color[2], color[3]},
        {x, y + h, color[0], color[1], color[2], color[3]},
    };

    emit_filled_quad(renderer, frame, quad, VK_RENDERER_PIPELINE_SOLID, 6);
}

void vk_renderer_draw_texture(VkRenderer* renderer,
                              const VkRendererTexture* texture,
                              const SDL_Rect* src,
                              const SDL_Rect* dst) {
    VkRendererFrameState* frame = active_frame(renderer);
    // Skip invalid textures or draw regions to avoid NaNs in UVs.
    if (!frame || !texture || texture->width == 0 || texture->height == 0) return;
    if (!renderer->pipelines[VK_RENDERER_PIPELINE_TEXTURED].pipeline ||
        !renderer->pipelines[VK_RENDERER_PIPELINE_TEXTURED].layout) {
        static int logged_missing = 0;
        if (!logged_missing) {
            VK_RENDERER_DEBUG_LOG("[vulkan] vk_renderer_draw_texture skipped: pipeline not ready.\n");
            logged_missing = 1;
        }
        return;
    }
    if (!texture->descriptor_set || !texture->image.view || !texture->sampler) {
        static int logged_invalid = 0;
        if (!logged_invalid) {
            VK_RENDERER_DEBUG_LOG("[vulkan] vk_renderer_draw_texture skipped: texture missing GPU handles.\n");
            logged_invalid = 1;
        }
        return;
    }

    static int logged_texture = 0;
    if (!logged_texture) {
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[vulkan] vk_renderer_draw_texture first call dst=(%d,%d %dx%d) texExtent=%ux%u\n",
            dst ? dst->x : 0,
            dst ? dst->y : 0,
            dst ? dst->w : (int)texture->width,
            dst ? dst->h : (int)texture->height,
            texture->width,
            texture->height);
        logged_texture = 1;
    }

    SDL_Rect local_dst;
    if (!dst) {
        local_dst.x = 0;
        local_dst.y = 0;
        local_dst.w = (int)texture->width;
        local_dst.h = (int)texture->height;
        dst = &local_dst;
    }
    if (dst->w <= 0 || dst->h <= 0) {
        return;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    float sw = (float)texture->width;
    float sh = (float)texture->height;

    if (src) {
        sx = (float)src->x;
        sy = (float)src->y;
        sw = (float)src->w;
        sh = (float)src->h;
    }
    if (sw <= 0.0f || sh <= 0.0f) {
        return;
    }

    float u0 = sx / (float)texture->width;
    float v0 = sy / (float)texture->height;
    float u1 = (sx + sw) / (float)texture->width;
    float v1 = (sy + sh) / (float)texture->height;

    float x = (float)dst->x;
    float y = (float)dst->y;
    float w = (float)dst->w;
    float h = (float)dst->h;

    float textured_vertices[6][8] = {
        {x, y, u0, v0, 1.0f, 1.0f, 1.0f, 1.0f},
        {x + w, y, u1, v0, 1.0f, 1.0f, 1.0f, 1.0f},
        {x + w, y + h, u1, v1, 1.0f, 1.0f, 1.0f, 1.0f},
        {x, y, u0, v0, 1.0f, 1.0f, 1.0f, 1.0f},
        {x + w, y + h, u1, v1, 1.0f, 1.0f, 1.0f, 1.0f},
        {x, y + h, u0, v1, 1.0f, 1.0f, 1.0f, 1.0f},
    };

    VkDeviceSize bytes = sizeof(textured_vertices);
    if (ensure_frame_vertex_buffer(renderer, frame, bytes) != VK_SUCCESS) return;

    if (!frame->vertex_buffer.mapped) {
        static int logged_unmapped = 0;
        if (!logged_unmapped) {
            VK_RENDERER_DEBUG_LOG("[vulkan] vk_renderer_draw_texture skipped: vertex buffer not mapped.\n");
            logged_unmapped = 1;
        }
        return;
    }
    uint8_t* dst_ptr = (uint8_t*)frame->vertex_buffer.mapped + frame->vertex_offset;
    memcpy(dst_ptr, textured_vertices, bytes);

    VkDeviceSize offset = frame->vertex_offset;
    frame->vertex_offset += bytes;

    flush_vertex_range(renderer, frame, offset, bytes);

    VkBuffer buffers[] = {frame->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {offset};
    VkCommandBuffer cmd = frame->command_buffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      renderer->pipelines[VK_RENDERER_PIPELINE_TEXTURED].pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    push_basic_constants(renderer, cmd, VK_RENDERER_PIPELINE_TEXTURED);
    apply_active_scissor(renderer, cmd);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            renderer->pipelines[VK_RENDERER_PIPELINE_TEXTURED].layout, 0, 1,
                            &texture->descriptor_set, 0, NULL);

    vkCmdDraw(cmd, 6, 1, 0, 0);
    renderer->draw_state.draw_call_count++;
    if (renderer->debug_capture.frame_counter == renderer->debug_capture.frame_trigger) {
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[vulkan] texture draw dst=(%d,%d %dx%d) color=%.2f %.2f %.2f %.2f\n",
            dst ? dst->x : 0,
            dst ? dst->y : 0,
            dst ? dst->w : (int)texture->width,
            dst ? dst->h : (int)texture->height,
            renderer->draw_state.current_color[0],
            renderer->draw_state.current_color[1],
            renderer->draw_state.current_color[2],
            renderer->draw_state.current_color[3]);
    }
}

VkResult vk_renderer_upload_sdl_surface_with_filter(VkRenderer* renderer,
                                                    SDL_Surface* surface,
                                                    VkRendererTexture* out_texture,
                                                    VkFilter filter) {
    if (!renderer || !surface || !out_texture) return VK_ERROR_INITIALIZATION_FAILED;
    // Ignore zero-sized surfaces instead of uploading invalid textures.
    if (surface->w <= 0 || surface->h <= 0) return VK_ERROR_INITIALIZATION_FAILED;

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_Surface* used_surface = converted ? converted : surface;

    VkResult result = vk_renderer_texture_create_from_rgba(
        renderer, used_surface->pixels, (uint32_t)used_surface->w, (uint32_t)used_surface->h,
        filter, out_texture);

    if (converted) SDL_FreeSurface(converted);
    return result;
}

VkResult vk_renderer_upload_sdl_surface(VkRenderer* renderer,
                                        SDL_Surface* surface,
                                        VkRendererTexture* out_texture) {
    return vk_renderer_upload_sdl_surface_with_filter(
        renderer, surface, out_texture, VK_FILTER_LINEAR);
}

void vk_renderer_queue_texture_destroy(VkRenderer* renderer,
                                       VkRendererTexture* texture) {
    if (!renderer || !texture) return;
    if (!texture->descriptor_set && !texture->sampler && !texture->image.image) {
        return;
    }

    uint32_t frame_index = renderer->current_frame_index;
    if (frame_index == UINT32_MAX) {
        vk_renderer_texture_destroy(renderer, texture);
        return;
    }

    VkRendererFrameState* frame = &renderer->frames[frame_index];
    if (!frame) {
        vk_renderer_texture_destroy(renderer, texture);
        return;
    }

    if (frame->transient_texture_count >= frame->transient_texture_capacity) {
        uint32_t new_capacity = frame->transient_texture_capacity ? frame->transient_texture_capacity * 2 : 8;
        VkRendererTexture* resized = (VkRendererTexture*)realloc(frame->transient_textures,
                                                                 sizeof(VkRendererTexture) * new_capacity);
        if (!resized) {
            vk_renderer_texture_destroy(renderer, texture);
            return;
        }
        frame->transient_textures = resized;
        frame->transient_texture_capacity = new_capacity;
    }

    frame->transient_textures[frame->transient_texture_count++] = *texture;
    memset(texture, 0, sizeof(*texture));
}

void vk_renderer_wait_idle(VkRenderer* renderer) {
    if (!renderer || !renderer->context.device) return;
    vk_renderer_device_wait_idle(renderer->context.device);
}

void vk_renderer_shutdown_surface(VkRenderer* renderer) {
    if (!renderer) return;
    VkRendererDevice* device = renderer->context.device;
    if (device) {
        vk_renderer_device_wait_idle(device);
    }

    if (renderer->frames) {
        for (uint32_t i = 0; i < renderer->frame_count; ++i) {
            VkRendererFrameState* frame = &renderer->frames[i];
            flush_transient_textures(renderer, frame);
            free(frame->transient_textures);
            frame->transient_textures = NULL;
            frame->transient_texture_capacity = 0;
            frame->transient_texture_count = 0;
            vk_renderer_memory_destroy_buffer(&renderer->context, &frame->vertex_buffer);
        }
    }

    destroy_framebuffers(renderer);
    vk_renderer_debug_capture_destroy(renderer);

    if (device && device->device != VK_NULL_HANDLE) {
        VkDevice vk_device = device->device;
        if (renderer->descriptor_pool) {
            vkDestroyDescriptorPool(vk_device, renderer->descriptor_pool, NULL);
            renderer->descriptor_pool = VK_NULL_HANDLE;
        }

        if (renderer->sampler_set_layout) {
            vkDestroyDescriptorSetLayout(vk_device, renderer->sampler_set_layout, NULL);
            renderer->sampler_set_layout = VK_NULL_HANDLE;
        }

        vk_renderer_pipeline_destroy_all(&renderer->context, renderer->pipelines);

        if (renderer->render_pass) {
            vkDestroyRenderPass(vk_device, renderer->render_pass, NULL);
            renderer->render_pass = VK_NULL_HANDLE;
        }
    }

    vk_renderer_commands_destroy(renderer, &renderer->command_pool);

    VkBool32 owned = renderer->context.owns_device;
    renderer->context.owns_device = VK_FALSE;
    vk_renderer_context_destroy(&renderer->context);
    renderer->context.owns_device = owned;
}

void vk_renderer_shutdown(VkRenderer* renderer) {
    if (!renderer) return;
    vk_renderer_wait_idle(renderer);

    if (renderer->frames) {
        for (uint32_t i = 0; i < renderer->frame_count; ++i) {
            VkRendererFrameState* frame = &renderer->frames[i];
            flush_transient_textures(renderer, frame);
            free(frame->transient_textures);
            frame->transient_textures = NULL;
            frame->transient_texture_capacity = 0;
            frame->transient_texture_count = 0;
            vk_renderer_memory_destroy_buffer(&renderer->context, &frame->vertex_buffer);
        }
    }

    destroy_framebuffers(renderer);
    vk_renderer_debug_capture_destroy(renderer);

    if (renderer->context.device && renderer->context.device->device != VK_NULL_HANDLE) {
        VkDevice device = renderer->context.device->device;
        if (renderer->descriptor_pool) {
            vkDestroyDescriptorPool(device, renderer->descriptor_pool, NULL);
            renderer->descriptor_pool = VK_NULL_HANDLE;
        }

        if (renderer->sampler_set_layout) {
            vkDestroyDescriptorSetLayout(device, renderer->sampler_set_layout, NULL);
            renderer->sampler_set_layout = VK_NULL_HANDLE;
        }

        vk_renderer_pipeline_destroy_all(&renderer->context, renderer->pipelines);

        if (renderer->render_pass) {
            vkDestroyRenderPass(device, renderer->render_pass, NULL);
            renderer->render_pass = VK_NULL_HANDLE;
        }
    }

    vk_renderer_commands_destroy(renderer, &renderer->command_pool);

    vk_renderer_context_destroy(&renderer->context);
}
