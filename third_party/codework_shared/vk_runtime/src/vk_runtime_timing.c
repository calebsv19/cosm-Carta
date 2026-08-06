#include "vk_runtime_resident_internal.h"

#include <float.h>
#include <stdint.h>
#include <string.h>

uint64_t vk_runtime_timing_delta_ticks(uint64_t start,
                                       uint64_t end,
                                       uint32_t valid_bits) {
    if (valid_bits >= 64u) {
        return end - start;
    }
    uint64_t mask = (UINT64_C(1) << valid_bits) - UINT64_C(1);
    return ((end & mask) - (start & mask)) & mask;
}

uint64_t vk_runtime_timing_ticks_to_ns(uint64_t ticks,
                                       double period_ns) {
    long double value =
        (long double)ticks * (long double)period_ns;
    if (value >= (long double)UINT64_MAX) {
        return UINT64_MAX;
    }
    return value > 0.0L ? (uint64_t)(value + 0.5L) : 0u;
}

VkRuntimeStatus vk_runtime_timing_session_initialize(
    VkRuntimeComputeSessionState *state,
    VkRuntimeResidentOperationResult *result) {
    if (!state || !state->runtime ||
        state->runtime->report.selected_device_index >=
            state->runtime->report.device_count) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    const VkRuntimeDeviceCapability *device =
        &state->runtime->report.devices[
            state->runtime->report.selected_device_index];
    if (state->queue_family_index >= device->queue_family_count) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    state->timestamp_valid_bits =
        device->queue_families[state->queue_family_index]
            .timestamp_valid_bits;

    VkPhysicalDeviceProperties properties;
    memset(&properties, 0, sizeof(properties));
    vkGetPhysicalDeviceProperties(state->runtime->physical_device,
                                  &properties);
    state->timestamp_period_ns =
        (double)properties.limits.timestampPeriod;
    if (state->timestamp_valid_bits == 0u ||
        !(state->timestamp_period_ns > 0.0) ||
        state->timestamp_period_ns > DBL_MAX) {
        state->timestamp_valid_bits = 0u;
        state->timestamp_period_ns = 0.0;
        return VK_RUNTIME_STATUS_OK;
    }

    VkQueryPoolCreateInfo create;
    memset(&create, 0, sizeof(create));
    create.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    create.queryType = VK_QUERY_TYPE_TIMESTAMP;
    create.queryCount = 2u;
    VkResult vk_result =
        vkCreateQueryPool(state->runtime->device,
                          &create,
                          NULL,
                          &state->timestamp_query_pool);
    if (vk_result != VK_SUCCESS) {
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_TIMESTAMP_QUERY_CREATE_FAILED,
            vk_result);
    }
    state->timestamp_supported = true;
    return VK_RUNTIME_STATUS_OK;
}

void vk_runtime_timing_session_destroy(
    VkRuntimeComputeSessionState *state) {
    if (!state || state->timestamp_query_pool == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyQueryPool(state->runtime->device,
                       state->timestamp_query_pool,
                       NULL);
    state->timestamp_query_pool = VK_NULL_HANDLE;
    state->timestamp_supported = false;
    state->timestamp_pending = false;
}

void vk_runtime_timing_record_begin(
    VkRuntimeComputeSessionState *state,
    VkRuntimeResidentOperationResult *result) {
    if (result) {
        result->gpu_timestamp_supported =
            state && state->timestamp_supported;
    }
    if (!state || !state->timestamp_supported) {
        return;
    }
    vkCmdResetQueryPool(state->command_buffer,
                        state->timestamp_query_pool,
                        0u,
                        2u);
    vkCmdWriteTimestamp(state->command_buffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        state->timestamp_query_pool,
                        0u);
}

void vk_runtime_timing_record_end(
    VkRuntimeComputeSessionState *state) {
    if (!state || !state->timestamp_supported) {
        return;
    }
    vkCmdWriteTimestamp(state->command_buffer,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        state->timestamp_query_pool,
                        1u);
    state->timestamp_pending = true;
}

VkRuntimeStatus vk_runtime_timing_resolve(
    VkRuntimeComputeSessionState *state,
    VkRuntimeResidentOperationResult *result) {
    if (!state || !state->timestamp_supported ||
        !state->timestamp_pending) {
        if (result) {
            result->gpu_timestamp_supported =
                state && state->timestamp_supported;
        }
        return VK_RUNTIME_STATUS_OK;
    }
    uint64_t ticks[2] = {0u, 0u};
    VkResult vk_result =
        vkGetQueryPoolResults(state->runtime->device,
                              state->timestamp_query_pool,
                              0u,
                              2u,
                              sizeof(ticks),
                              ticks,
                              sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT);
    state->timestamp_pending = false;
    if (vk_result != VK_SUCCESS) {
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_TIMESTAMP_RESULT_FAILED,
            vk_result);
    }
    uint64_t delta =
        vk_runtime_timing_delta_ticks(ticks[0],
                                      ticks[1],
                                      state->timestamp_valid_bits);
    if (result) {
        result->gpu_timestamp_supported = true;
        result->gpu_timestamp_valid = true;
        result->gpu_start_tick = ticks[0];
        result->gpu_end_tick = ticks[1];
        result->gpu_elapsed_ns =
            vk_runtime_timing_ticks_to_ns(
                delta,
                state->timestamp_period_ns);
    }
    state->timestamp_measurement_count += 1u;
    return VK_RUNTIME_STATUS_OK;
}
