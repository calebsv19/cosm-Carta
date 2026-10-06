#include "vk_renderer_commands.h"
#include "vk_renderer.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


VkResult vk_renderer_commands_recreate_present_semaphores(
    VkRenderer* renderer,
    VkRendererCommandPool* pool,
    uint32_t swapchain_image_count) {
    VkSemaphore* replacement;
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    uint32_t i;
    if (!renderer || !pool || !renderer->context.device || swapchain_image_count == 0u) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    replacement = (VkSemaphore*)calloc(swapchain_image_count, sizeof(VkSemaphore));
    if (!replacement) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    for (i = 0u; i < swapchain_image_count; ++i) {
        if (vkCreateSemaphore(renderer->context.device->device,
                              &semaphore_info,
                              NULL,
                              &replacement[i]) != VK_SUCCESS) {
            while (i > 0u) {
                --i;
                vkDestroySemaphore(renderer->context.device->device,
                                   replacement[i],
                                   NULL);
            }
            free(replacement);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    if (pool->render_finished) {
        for (i = 0u; i < pool->render_finished_count; ++i) {
            if (pool->render_finished[i]) {
                vkDestroySemaphore(renderer->context.device->device,
                                   pool->render_finished[i],
                                   NULL);
            }
        }
        free(pool->render_finished);
    }
    pool->render_finished = replacement;
    pool->render_finished_count = swapchain_image_count;
    return VK_SUCCESS;
}

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

    if (!out_pool->buffers || !out_pool->fences || !out_pool->image_available ||
        renderer->context.swapchain.image_count == 0u) {
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
        renderer->frames[i].command_buffer = out_pool->buffers[i];
        renderer->frames[i].in_flight_fence = out_pool->fences[i];
        renderer->frames[i].image_available = out_pool->image_available[i];
    }

    result = vk_renderer_commands_recreate_present_semaphores(
        renderer, out_pool, renderer->context.swapchain.image_count);
    if (result != VK_SUCCESS) return result;

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
        for (uint32_t i = 0; i < pool->render_finished_count; ++i) {
            if (pool->render_finished[i])
                vkDestroySemaphore(device, pool->render_finished[i], NULL);
        }
        free(pool->render_finished);
        pool->render_finished = NULL;
    }
    pool->render_finished_count = 0u;

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
