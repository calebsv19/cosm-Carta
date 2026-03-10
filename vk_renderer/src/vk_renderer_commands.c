#include "vk_renderer_commands.h"
#include "vk_renderer.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_logged_acquire_out_of_date = 0;
static int s_logged_acquire_failure = 0;
static int s_logged_submit_failure = 0;
static int s_logged_present_failure = 0;

VkResult vk_renderer_commands_init(VkRenderer* renderer,
                                   VkRendererCommandPool* out_pool,
                                   uint32_t frames_in_flight) {
    if (!renderer || !out_pool) return VK_ERROR_INITIALIZATION_FAILED;
    if (!renderer->context.device) return VK_ERROR_INITIALIZATION_FAILED;

    memset(out_pool, 0, sizeof(*out_pool));
    VkRendererDevice* device = renderer->context.device;

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = device->graphics_queue_family,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };

    VkResult result = vkCreateCommandPool(device->device, &pool_info, NULL,
                                          &out_pool->pool);
    if (result != VK_SUCCESS) return result;

    out_pool->count = frames_in_flight;

    out_pool->buffers = (VkCommandBuffer*)calloc(frames_in_flight, sizeof(VkCommandBuffer));
    out_pool->fences = (VkFence*)calloc(frames_in_flight, sizeof(VkFence));
    out_pool->image_available =
        (VkSemaphore*)calloc(frames_in_flight, sizeof(VkSemaphore));
    out_pool->render_finished =
        (VkSemaphore*)calloc(frames_in_flight, sizeof(VkSemaphore));

    if (!out_pool->buffers || !out_pool->fences || !out_pool->image_available ||
        !out_pool->render_finished) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = out_pool->pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = frames_in_flight,
    };

    result = vkAllocateCommandBuffers(device->device, &alloc_info,
                                      out_pool->buffers);
    if (result != VK_SUCCESS) return result;

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    renderer->frames =
        (VkRendererFrameState*)calloc(frames_in_flight, sizeof(VkRendererFrameState));
    if (!renderer->frames) return VK_ERROR_OUT_OF_HOST_MEMORY;

    for (uint32_t i = 0; i < frames_in_flight; ++i) {
        if (vkCreateFence(device->device, &fence_info, NULL,
                          &out_pool->fences[i]) != VK_SUCCESS) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        if (vkCreateSemaphore(device->device, &semaphore_info, NULL,
                              &out_pool->image_available[i]) != VK_SUCCESS) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        if (vkCreateSemaphore(device->device, &semaphore_info, NULL,
                              &out_pool->render_finished[i]) != VK_SUCCESS) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        renderer->frames[i].command_buffer = out_pool->buffers[i];
        renderer->frames[i].in_flight_fence = out_pool->fences[i];
        renderer->frames[i].image_available = out_pool->image_available[i];
        renderer->frames[i].render_finished = out_pool->render_finished[i];
    }

    renderer->frame_count = frames_in_flight;
    renderer->frame_index = 0;

    return VK_SUCCESS;
}

void vk_renderer_commands_destroy(VkRenderer* renderer,
                                  VkRendererCommandPool* pool) {
    if (!renderer || !pool || !renderer->context.device) return;

    VkDevice device = renderer->context.device->device;

    if (pool->image_available) {
        for (uint32_t i = 0; i < pool->count; ++i) {
            if (pool->image_available[i])
                vkDestroySemaphore(device, pool->image_available[i], NULL);
        }
        free(pool->image_available);
        pool->image_available = NULL;
    }

    if (pool->render_finished) {
        for (uint32_t i = 0; i < pool->count; ++i) {
            if (pool->render_finished[i])
                vkDestroySemaphore(device, pool->render_finished[i], NULL);
        }
        free(pool->render_finished);
        pool->render_finished = NULL;
    }

    if (pool->fences) {
        for (uint32_t i = 0; i < pool->count; ++i) {
            if (pool->fences[i]) vkDestroyFence(device, pool->fences[i], NULL);
        }
        free(pool->fences);
        pool->fences = NULL;
    }

    if (pool->buffers) {
        free(pool->buffers);
        pool->buffers = NULL;
    }

    if (pool->pool) {
        vkDestroyCommandPool(device, pool->pool, NULL);
        pool->pool = VK_NULL_HANDLE;
    }

    if (renderer->frames) {
        free(renderer->frames);
        renderer->frames = NULL;
    }
    renderer->frame_count = 0;
    renderer->frame_index = 0;
}

VkResult vk_renderer_commands_begin_frame(VkRenderer* renderer,
                                          uint32_t* frame_index,
                                          VkCommandBuffer* out_cmd) {
    if (!renderer || !out_cmd || !frame_index) return VK_ERROR_INITIALIZATION_FAILED;
    if (!renderer->context.device) return VK_ERROR_INITIALIZATION_FAILED;
    VkDevice device = renderer->context.device->device;

    uint32_t current_frame = renderer->frame_index % renderer->frame_count;
    VkRendererFrameState* frame = &renderer->frames[current_frame];

    VkResult wait_result = vkWaitForFences(device, 1, &frame->in_flight_fence, VK_TRUE,
                                           UINT64_MAX);
    if (wait_result != VK_SUCCESS) {
        return wait_result;
    }
    VkResult reset_result = vkResetFences(device, 1, &frame->in_flight_fence);
    if (reset_result != VK_SUCCESS) {
        return reset_result;
    }

    uint32_t image_index = 0;
    VkResult result = vkAcquireNextImageKHR(device,
                                            renderer->context.swapchain.handle,
                                            UINT64_MAX, frame->image_available,
                                            VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!s_logged_acquire_out_of_date) {
            fprintf(stderr, "[vulkan] vkAcquireNextImageKHR returned OUT_OF_DATE.\n");
            s_logged_acquire_out_of_date = 1;
        }
        return result;
    } else if (result == VK_SUBOPTIMAL_KHR) {
        if (!s_logged_acquire_failure) {
            fprintf(stderr, "[vulkan] vkAcquireNextImageKHR returned SUBOPTIMAL.\n");
            s_logged_acquire_failure = 1;
        }
        return result;
    } else if (result != VK_SUCCESS) {
        if (!s_logged_acquire_failure) {
            fprintf(stderr, "[vulkan] vkAcquireNextImageKHR failed: %d\n", result);
            s_logged_acquire_failure = 1;
        }
        return result;
    }
    s_logged_acquire_out_of_date = 0;
    s_logged_acquire_failure = 0;

    renderer->swapchain_image_index = image_index;

    VkResult reset_cmd_result = vkResetCommandBuffer(frame->command_buffer, 0);
    if (reset_cmd_result != VK_SUCCESS) {
        return reset_cmd_result;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VkResult begin_result = vkBeginCommandBuffer(frame->command_buffer, &begin_info);
    if (begin_result != VK_SUCCESS) {
        return begin_result;
    }

    *frame_index = current_frame;
    *out_cmd = frame->command_buffer;
    return VK_SUCCESS;
}

VkResult vk_renderer_commands_end_frame(VkRenderer* renderer,
                                        uint32_t frame_index,
                                        VkCommandBuffer cmd) {
    if (!renderer) return VK_ERROR_INITIALIZATION_FAILED;
    if (!renderer->context.device) return VK_ERROR_INITIALIZATION_FAILED;
    VkRendererDevice* device = renderer->context.device;

    VkRendererFrameState* frame = &renderer->frames[frame_index];

    VkResult result = vkEndCommandBuffer(cmd);
    if (result != VK_SUCCESS) return result;

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame->image_available,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frame->render_finished,
    };

    result = vkQueueSubmit(device->graphics_queue, 1, &submit_info,
                           frame->in_flight_fence);
    if (result != VK_SUCCESS) {
        if (!s_logged_submit_failure) {
            fprintf(stderr, "[vulkan] vkQueueSubmit failed: %d\n", result);
            s_logged_submit_failure = 1;
        }
        return result;
    }
    s_logged_submit_failure = 0;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame->render_finished,
        .swapchainCount = 1,
        .pSwapchains = &renderer->context.swapchain.handle,
        .pImageIndices = &renderer->swapchain_image_index,
    };

    result = vkQueuePresentKHR(device->present_queue, &present_info);

    renderer->frame_index = (renderer->frame_index + 1) % renderer->frame_count;
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!s_logged_present_failure) {
            fprintf(stderr, "[vulkan] vkQueuePresentKHR returned OUT_OF_DATE.\n");
            s_logged_present_failure = 1;
        }
        return result;
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        if (!s_logged_present_failure) {
            fprintf(stderr, "[vulkan] vkQueuePresentKHR returned SUBOPTIMAL.\n");
            s_logged_present_failure = 1;
        }
        return result;
    }
    if (result != VK_SUCCESS && !s_logged_present_failure) {
        fprintf(stderr, "[vulkan] vkQueuePresentKHR failed: %d\n", result);
        s_logged_present_failure = 1;
    } else if (result == VK_SUCCESS) {
        s_logged_present_failure = 0;
    }
    return result;
}
