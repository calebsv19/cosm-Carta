#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "vk_runtime_resident_internal.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#endif

uint64_t vk_runtime_resident_monotonic_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (!QueryPerformanceFrequency(&frequency) ||
        !QueryPerformanceCounter(&counter) ||
        frequency.QuadPart <= 0) {
        return 0u;
    }
    return (uint64_t)((counter.QuadPart * 1000000000ull) /
                      frequency.QuadPart);
#else
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        return 0u;
    }
    return (uint64_t)value.tv_sec * 1000000000ull +
           (uint64_t)value.tv_nsec;
#endif
}

uint64_t vk_runtime_resident_elapsed_since(uint64_t start) {
    uint64_t end = vk_runtime_resident_monotonic_ns();
    return start != 0u && end >= start ? end - start : 0u;
}

void vk_runtime_resident_result_reset(
    VkRuntimeResidentOperationResult *result) {
    if (!result) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->status = VK_RUNTIME_STATUS_OK;
    result->vulkan_result = VK_SUCCESS;
}

VkRuntimeStatus vk_runtime_resident_fail(
    VkRuntimeResidentOperationResult *result,
    VkRuntimeStatus status,
    VkResult vulkan_result) {
    if (result) {
        result->status = status;
        result->vulkan_result = vulkan_result;
    }
    return status;
}

VkRuntimeStatus vk_runtime_resident_fail_vk(
    VkRuntimeResidentOperationResult *result,
    VkRuntimeStatus fallback,
    VkResult vulkan_result) {
    return vk_runtime_resident_fail(
        result,
        vulkan_result == VK_ERROR_DEVICE_LOST
            ? VK_RUNTIME_STATUS_DEVICE_LOST
            : fallback,
        vulkan_result);
}

VkRuntimeComputeSessionState *vk_runtime_resident_session_state(
    VkRuntimeComputeSession *session) {
    return session
               ? (VkRuntimeComputeSessionState *)session->internal
               : NULL;
}

static uint32_t find_memory_type(
    VkPhysicalDevice physical_device,
    uint32_t type_bits,
    VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred,
    VkMemoryPropertyFlags *selected_flags) {
    VkPhysicalDeviceMemoryProperties properties;
    memset(&properties, 0, sizeof(properties));
    vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
    uint32_t best_index = UINT32_MAX;
    uint32_t best_score = 0u;
    for (uint32_t i = 0u; i < properties.memoryTypeCount; ++i) {
        VkMemoryPropertyFlags flags =
            properties.memoryTypes[i].propertyFlags;
        if ((type_bits & (1u << i)) == 0u ||
            (flags & required) != required) {
            continue;
        }
        uint32_t score = 1u;
        VkMemoryPropertyFlags matches = flags & preferred;
        while (matches != 0u) {
            score += matches & 1u;
            matches >>= 1u;
        }
        if (best_index == UINT32_MAX || score > best_score) {
            best_index = i;
            best_score = score;
        }
    }
    if (best_index != UINT32_MAX && selected_flags) {
        *selected_flags =
            properties.memoryTypes[best_index].propertyFlags;
    }
    return best_index;
}

static void destroy_buffer_state(VkRuntimeResidentBufferState *state) {
    if (!state) {
        return;
    }
    VkDevice device = state->session->runtime->device;
    if (state->mapped) {
        vkUnmapMemory(device, state->memory);
    }
    if (state->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, state->buffer, NULL);
    }
    if (state->memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, state->memory, NULL);
    }
    free(state);
}

VkRuntimeStatus vk_runtime_resident_begin_commands(
    VkRuntimeComputeSessionState *state,
    VkRuntimeResidentOperationResult *result) {
    if (!state || state->in_flight) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_RESOURCE_IN_USE,
            VK_NOT_READY);
    }
    VkResult vk_result =
        vkResetCommandPool(state->runtime->device,
                           state->command_pool,
                           0u);
    if (vk_result != VK_SUCCESS) {
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
            vk_result);
    }
    VkCommandBufferBeginInfo begin;
    memset(&begin, 0, sizeof(begin));
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_result = vkBeginCommandBuffer(state->command_buffer, &begin);
    if (vk_result != VK_SUCCESS) {
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
            vk_result);
    }
    vk_runtime_timing_record_begin(state, result);
    return VK_RUNTIME_STATUS_OK;
}

static VkRuntimeStatus wait_internal(
    VkRuntimeComputeSessionState *state,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result) {
    if (!state->in_flight) {
        return VK_RUNTIME_STATUS_OK;
    }
    uint64_t start = vk_runtime_resident_monotonic_ns();
    VkResult vk_result =
        vkWaitForFences(state->runtime->device,
                        1u,
                        &state->fence,
                        VK_TRUE,
                        timeout_ns);
    if (result) {
        result->submit_wait_ns +=
            vk_runtime_resident_elapsed_since(start);
    }
    if (vk_result == VK_TIMEOUT) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_FENCE_WAIT_TIMEOUT,
            vk_result);
    }
    if (vk_result != VK_SUCCESS) {
        state->timestamp_pending = false;
        if (vk_result == VK_ERROR_DEVICE_LOST) {
            state->in_flight = false;
        }
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_FENCE_WAIT_FAILED,
            vk_result);
    }
    state->in_flight = false;
    state->completed_submission_count += 1u;
    return vk_runtime_timing_resolve(state, result);
}

VkRuntimeStatus vk_runtime_resident_end_submit_wait(
    VkRuntimeComputeSessionState *state,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_timing_record_end(state);
    VkResult vk_result = vkEndCommandBuffer(state->command_buffer);
    if (vk_result != VK_SUCCESS) {
        state->timestamp_pending = false;
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
            vk_result);
    }
    vk_result =
        vkResetFences(state->runtime->device, 1u, &state->fence);
    if (vk_result != VK_SUCCESS) {
        state->timestamp_pending = false;
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
            vk_result);
    }
    VkSubmitInfo submit;
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &state->command_buffer;
    uint64_t start = vk_runtime_resident_monotonic_ns();
    vk_result = vkQueueSubmit(state->runtime->compute_queue,
                              1u,
                              &submit,
                              state->fence);
    if (vk_result != VK_SUCCESS) {
        state->timestamp_pending = false;
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_COMMAND_SUBMIT_FAILED,
            vk_result);
    }
    state->submission_count += 1u;
    state->in_flight = true;
    if (timeout_ns == 0u) {
        if (result) {
            result->submit_wait_ns +=
                vk_runtime_resident_elapsed_since(start);
        }
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_FENCE_WAIT_TIMEOUT,
            VK_TIMEOUT);
    }
    return wait_internal(state, timeout_ns, result);
}

VkRuntimeStatus vk_runtime_compute_session_initialize(
    VkRuntime *runtime,
    VkRuntimeComputeSession *session,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_resident_result_reset(result);
    if (!runtime || !session || session->internal ||
        runtime->device == VK_NULL_HANDLE ||
        runtime->physical_device == VK_NULL_HANDLE ||
        runtime->compute_queue == VK_NULL_HANDLE ||
        runtime->report.selected_device_index >=
            runtime->report.device_count) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    VkRuntimeComputeSessionState *state =
        (VkRuntimeComputeSessionState *)calloc(1u, sizeof(*state));
    if (!state) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY,
            VK_ERROR_OUT_OF_HOST_MEMORY);
    }
    state->runtime = runtime;
    state->queue_family_index =
        runtime->report.devices[runtime->report.selected_device_index]
            .compute_queue_family;

    VkCommandPoolCreateInfo pool;
    memset(&pool, 0, sizeof(pool));
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = state->queue_family_index;
    VkResult vk_result =
        vkCreateCommandPool(runtime->device,
                            &pool,
                            NULL,
                            &state->command_pool);
    if (vk_result == VK_SUCCESS) {
        VkCommandBufferAllocateInfo allocation;
        memset(&allocation, 0, sizeof(allocation));
        allocation.sType =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocation.commandPool = state->command_pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1u;
        vk_result =
            vkAllocateCommandBuffers(runtime->device,
                                     &allocation,
                                     &state->command_buffer);
    }
    if (vk_result == VK_SUCCESS) {
        VkFenceCreateInfo fence;
        memset(&fence, 0, sizeof(fence));
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vk_result =
            vkCreateFence(runtime->device, &fence, NULL, &state->fence);
    }
    if (vk_result != VK_SUCCESS) {
        if (state->fence != VK_NULL_HANDLE) {
            vkDestroyFence(runtime->device, state->fence, NULL);
        }
        if (state->command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(runtime->device,
                                 state->command_pool,
                                 NULL);
        }
        free(state);
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
            vk_result);
    }
    VkRuntimeStatus timing_status =
        vk_runtime_timing_session_initialize(state, result);
    if (timing_status != VK_RUNTIME_STATUS_OK) {
        vk_runtime_timing_session_destroy(state);
        vkDestroyFence(runtime->device, state->fence, NULL);
        vkDestroyCommandPool(runtime->device,
                             state->command_pool,
                             NULL);
        free(state);
        return timing_status;
    }
    session->internal = state;
    return VK_RUNTIME_STATUS_OK;
}

VkRuntimeStatus vk_runtime_compute_session_get_info(
    const VkRuntimeComputeSession *session,
    VkRuntimeComputeSessionInfo *info) {
    if (!session || !session->internal || !info) {
        return VK_RUNTIME_STATUS_INVALID_ARGUMENT;
    }
    const VkRuntimeComputeSessionState *state =
        (const VkRuntimeComputeSessionState *)session->internal;
    memset(info, 0, sizeof(*info));
    info->queue_family_index = state->queue_family_index;
    info->live_buffer_count = state->live_buffer_count;
    info->live_program_count = state->live_program_count;
    info->submission_count = state->submission_count;
    info->completed_submission_count =
        state->completed_submission_count;
    info->upload_count = state->upload_count;
    info->readback_count = state->readback_count;
    info->dispatch_count = state->dispatch_count;
    info->barrier_count = state->barrier_count;
    info->timestamp_measurement_count =
        state->timestamp_measurement_count;
    info->timestamp_valid_bits = state->timestamp_valid_bits;
    info->timestamp_period_ns = state->timestamp_period_ns;
    info->timestamp_supported = state->timestamp_supported;
    info->in_flight = state->in_flight;
    return VK_RUNTIME_STATUS_OK;
}

VkRuntimeStatus vk_runtime_compute_session_wait(
    VkRuntimeComputeSession *session,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_resident_result_reset(result);
    VkRuntimeComputeSessionState *state =
        vk_runtime_resident_session_state(session);
    if (!state) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    return wait_internal(state, timeout_ns, result);
}

VkRuntimeStatus vk_runtime_compute_session_close(
    VkRuntimeComputeSession *session,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_resident_result_reset(result);
    VkRuntimeComputeSessionState *state =
        vk_runtime_resident_session_state(session);
    if (!state) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    if (state->in_flight || state->live_buffer_count != 0u ||
        state->live_program_count != 0u) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_RESOURCE_IN_USE,
            VK_NOT_READY);
    }
    VkDevice device = state->runtime->device;
    vk_runtime_timing_session_destroy(state);
    vkDestroyFence(device, state->fence, NULL);
    vkDestroyCommandPool(device, state->command_pool, NULL);
    free(state);
    session->internal = NULL;
    return VK_RUNTIME_STATUS_OK;
}

VkRuntimeStatus vk_runtime_resident_buffer_create(
    VkRuntimeComputeSession *session,
    VkRuntimeBufferRole role,
    size_t size,
    VkRuntimeResidentBuffer *buffer,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_resident_result_reset(result);
    VkRuntimeComputeSessionState *session_state =
        vk_runtime_resident_session_state(session);
    if (!session_state || !buffer || buffer->internal || size == 0u ||
        (role != VK_RUNTIME_BUFFER_ROLE_HOST_STAGING &&
         role != VK_RUNTIME_BUFFER_ROLE_DEVICE_LOCAL) ||
        session_state->in_flight) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    VkRuntimeResidentBufferState *state =
        (VkRuntimeResidentBufferState *)calloc(1u, sizeof(*state));
    if (!state) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY,
            VK_ERROR_OUT_OF_HOST_MEMORY);
    }
    state->session = session_state;
    state->size = (VkDeviceSize)size;
    state->role = role;

    VkBufferCreateInfo create;
    memset(&create, 0, sizeof(create));
    create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create.size = state->size;
    create.usage = role == VK_RUNTIME_BUFFER_ROLE_HOST_STAGING
                       ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT
                       : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult vk_result =
        vkCreateBuffer(session_state->runtime->device,
                       &create,
                       NULL,
                       &state->buffer);
    if (vk_result != VK_SUCCESS) {
        free(state);
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_BUFFER_CREATE_FAILED,
            vk_result);
    }

    VkMemoryRequirements requirements;
    memset(&requirements, 0, sizeof(requirements));
    vkGetBufferMemoryRequirements(session_state->runtime->device,
                                  state->buffer,
                                  &requirements);
    VkMemoryPropertyFlags required =
        role == VK_RUNTIME_BUFFER_ROLE_HOST_STAGING
            ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryPropertyFlags preferred =
        role == VK_RUNTIME_BUFFER_ROLE_HOST_STAGING
            ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT
            : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    state->memory_type_index = find_memory_type(
        session_state->runtime->physical_device,
        requirements.memoryTypeBits,
        required,
        preferred,
        &state->memory_property_flags);
    if (state->memory_type_index == UINT32_MAX) {
        destroy_buffer_state(state);
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_MEMORY_TYPE_UNAVAILABLE,
            VK_ERROR_FEATURE_NOT_PRESENT);
    }

    VkMemoryAllocateInfo allocation;
    memset(&allocation, 0, sizeof(allocation));
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = state->memory_type_index;
    vk_result =
        vkAllocateMemory(session_state->runtime->device,
                         &allocation,
                         NULL,
                         &state->memory);
    if (vk_result != VK_SUCCESS) {
        destroy_buffer_state(state);
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_MEMORY_ALLOCATE_FAILED,
            vk_result);
    }
    vk_result =
        vkBindBufferMemory(session_state->runtime->device,
                           state->buffer,
                           state->memory,
                           0u);
    if (vk_result != VK_SUCCESS) {
        destroy_buffer_state(state);
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_MEMORY_ALLOCATE_FAILED,
            vk_result);
    }
    if (role == VK_RUNTIME_BUFFER_ROLE_HOST_STAGING) {
        vk_result =
            vkMapMemory(session_state->runtime->device,
                        state->memory,
                        0u,
                        state->size,
                        0u,
                        &state->mapped);
        if (vk_result != VK_SUCCESS) {
            destroy_buffer_state(state);
            return vk_runtime_resident_fail_vk(
                result,
                VK_RUNTIME_STATUS_MEMORY_MAP_FAILED,
                vk_result);
        }
    }
    buffer->internal = state;
    session_state->live_buffer_count += 1u;
    return VK_RUNTIME_STATUS_OK;
}

VkRuntimeStatus vk_runtime_resident_buffer_get_info(
    const VkRuntimeResidentBuffer *buffer,
    VkRuntimeResidentBufferInfo *info) {
    if (!buffer || !buffer->internal || !info) {
        return VK_RUNTIME_STATUS_INVALID_ARGUMENT;
    }
    const VkRuntimeResidentBufferState *state =
        (const VkRuntimeResidentBufferState *)buffer->internal;
    memset(info, 0, sizeof(*info));
    info->role = state->role;
    info->size = (size_t)state->size;
    info->memory_type_index = state->memory_type_index;
    info->memory_property_flags = state->memory_property_flags;
    info->program_reference_count = state->program_reference_count;
    return VK_RUNTIME_STATUS_OK;
}

VkRuntimeStatus vk_runtime_resident_buffer_destroy(
    VkRuntimeResidentBuffer *buffer,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_resident_result_reset(result);
    if (!buffer || !buffer->internal) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    VkRuntimeResidentBufferState *state =
        (VkRuntimeResidentBufferState *)buffer->internal;
    if (state->session->in_flight ||
        state->program_reference_count != 0u) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_RESOURCE_IN_USE,
            VK_NOT_READY);
    }
    state->session->live_buffer_count -= 1u;
    destroy_buffer_state(state);
    buffer->internal = NULL;
    return VK_RUNTIME_STATUS_OK;
}

static VkRuntimeStatus validate_transfer(
    VkRuntimeComputeSession *session,
    VkRuntimeResidentBuffer *staging,
    VkRuntimeResidentBuffer *device_buffer,
    size_t size,
    VkRuntimeResidentOperationResult *result,
    VkRuntimeComputeSessionState **session_state_out,
    VkRuntimeResidentBufferState **staging_state_out,
    VkRuntimeResidentBufferState **device_state_out) {
    VkRuntimeComputeSessionState *session_state =
        vk_runtime_resident_session_state(session);
    VkRuntimeResidentBufferState *staging_state =
        staging ? (VkRuntimeResidentBufferState *)staging->internal
                : NULL;
    VkRuntimeResidentBufferState *device_state =
        device_buffer
            ? (VkRuntimeResidentBufferState *)device_buffer->internal
            : NULL;
    if (!session_state || !staging_state || !device_state ||
        staging_state->session != session_state ||
        device_state->session != session_state ||
        staging_state->role != VK_RUNTIME_BUFFER_ROLE_HOST_STAGING ||
        device_state->role != VK_RUNTIME_BUFFER_ROLE_DEVICE_LOCAL) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    if (size == 0u || (VkDeviceSize)size > staging_state->size ||
        (VkDeviceSize)size > device_state->size) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_BUFFER_RANGE_INVALID,
            VK_ERROR_OUT_OF_DEVICE_MEMORY);
    }
    *session_state_out = session_state;
    *staging_state_out = staging_state;
    *device_state_out = device_state;
    return VK_RUNTIME_STATUS_OK;
}

VkRuntimeStatus vk_runtime_resident_buffer_upload(
    VkRuntimeComputeSession *session,
    VkRuntimeResidentBuffer *staging,
    VkRuntimeResidentBuffer *destination,
    const void *data,
    size_t size,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_resident_result_reset(result);
    if (!data) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    VkRuntimeComputeSessionState *session_state = NULL;
    VkRuntimeResidentBufferState *staging_state = NULL;
    VkRuntimeResidentBufferState *device_state = NULL;
    VkRuntimeStatus status =
        validate_transfer(session,
                          staging,
                          destination,
                          size,
                          result,
                          &session_state,
                          &staging_state,
                          &device_state);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }
    uint64_t copy_start = vk_runtime_resident_monotonic_ns();
    memcpy(staging_state->mapped, data, size);
    if (result) {
        result->host_copy_ns =
            vk_runtime_resident_elapsed_since(copy_start);
    }
    status = vk_runtime_resident_begin_commands(session_state, result);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }
    VkBufferCopy copy;
    memset(&copy, 0, sizeof(copy));
    copy.size = (VkDeviceSize)size;
    vkCmdCopyBuffer(session_state->command_buffer,
                    staging_state->buffer,
                    device_state->buffer,
                    1u,
                    &copy);
    VkBufferMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = device_state->buffer;
    barrier.offset = 0u;
    barrier.size = (VkDeviceSize)size;
    vkCmdPipelineBarrier(session_state->command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0u,
                         0u,
                         NULL,
                         1u,
                         &barrier,
                         0u,
                         NULL);
    status = vk_runtime_resident_end_submit_wait(
        session_state, timeout_ns, result);
    if (status == VK_RUNTIME_STATUS_OK ||
        status == VK_RUNTIME_STATUS_FENCE_WAIT_TIMEOUT) {
        session_state->upload_count += 1u;
    }
    return status;
}

VkRuntimeStatus vk_runtime_resident_buffer_readback(
    VkRuntimeComputeSession *session,
    VkRuntimeResidentBuffer *source,
    VkRuntimeResidentBuffer *staging,
    void *data,
    size_t size,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_resident_result_reset(result);
    if (!data) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    VkRuntimeComputeSessionState *session_state = NULL;
    VkRuntimeResidentBufferState *staging_state = NULL;
    VkRuntimeResidentBufferState *device_state = NULL;
    VkRuntimeStatus status =
        validate_transfer(session,
                          staging,
                          source,
                          size,
                          result,
                          &session_state,
                          &staging_state,
                          &device_state);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }
    status = vk_runtime_resident_begin_commands(session_state, result);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }
    VkBufferMemoryBarrier to_transfer;
    memset(&to_transfer, 0, sizeof(to_transfer));
    to_transfer.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    to_transfer.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.buffer = device_state->buffer;
    to_transfer.offset = 0u;
    to_transfer.size = (VkDeviceSize)size;
    vkCmdPipelineBarrier(session_state->command_buffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0u,
                         0u,
                         NULL,
                         1u,
                         &to_transfer,
                         0u,
                         NULL);
    VkBufferCopy copy;
    memset(&copy, 0, sizeof(copy));
    copy.size = (VkDeviceSize)size;
    vkCmdCopyBuffer(session_state->command_buffer,
                    device_state->buffer,
                    staging_state->buffer,
                    1u,
                    &copy);
    VkBufferMemoryBarrier to_host;
    memset(&to_host, 0, sizeof(to_host));
    to_host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    to_host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    to_host.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_host.buffer = staging_state->buffer;
    to_host.offset = 0u;
    to_host.size = (VkDeviceSize)size;
    vkCmdPipelineBarrier(session_state->command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT,
                         0u,
                         0u,
                         NULL,
                         1u,
                         &to_host,
                         0u,
                         NULL);
    status = vk_runtime_resident_end_submit_wait(
        session_state, timeout_ns, result);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }
    uint64_t copy_start = vk_runtime_resident_monotonic_ns();
    memcpy(data, staging_state->mapped, size);
    if (result) {
        result->host_copy_ns =
            vk_runtime_resident_elapsed_since(copy_start);
    }
    session_state->readback_count += 1u;
    return VK_RUNTIME_STATUS_OK;
}
