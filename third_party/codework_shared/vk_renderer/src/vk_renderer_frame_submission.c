#include "vk_renderer_commands.h"
#include "vk_renderer.h"
#include <stdint.h>
#include <stdio.h>

static int s_logged_acquire_out_of_date;
static int s_logged_acquire_failure;
static int s_logged_submit_failure;
static int s_logged_present_failure;

VkResult vk_renderer_commands_begin_frame(VkRenderer* renderer,
                                          uint32_t* frame_index,
                                          VkCommandBuffer* out_cmd) {
    if (!renderer || !out_cmd || !frame_index) return VK_ERROR_INITIALIZATION_FAILED;
    if (!renderer->context.device || !renderer->frames || !renderer->frame_count) return VK_ERROR_INITIALIZATION_FAILED;
    VkDevice device = renderer->context.device->device;

    uint32_t current_frame = renderer->frame_index % renderer->frame_count;
    VkRendererFrameState* frame = &renderer->frames[current_frame];

    VkResult wait_result = vkWaitForFences(device, 1, &frame->in_flight_fence, VK_TRUE,
                                           UINT64_MAX);
    if (wait_result != VK_SUCCESS) {
        return wait_result;
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
        /* Acquisition succeeded: consume its semaphore by submitting this image. */
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
    if (image_index >= renderer->command_pool.render_finished_count) {
        return VK_ERROR_OUT_OF_DATE_KHR;
    }
    frame->render_finished = renderer->command_pool.render_finished[image_index];

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

    /* Reset only once there is a finished command buffer to submit. A skipped
     * acquire/recording frame must leave the fence signaled for the next frame. */
    result = vkResetFences(device->device, 1, &frame->in_flight_fence);
    if (result != VK_SUCCESS) return result;
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
