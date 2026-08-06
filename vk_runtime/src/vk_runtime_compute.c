#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "vk_runtime.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#define VK_RUNTIME_SPIRV_MAGIC 0x07230203u
#define VK_RUNTIME_MAX_COMPUTE_BINDINGS 16u

typedef struct VkRuntimeComputeBuffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *mapped;
    VkDeviceSize size;
} VkRuntimeComputeBuffer;

typedef struct VkRuntimeComputeState {
    VkShaderModule shader_module;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkFence fence;
    VkRuntimeComputeBuffer buffers[VK_RUNTIME_MAX_COMPUTE_BINDINGS];
    uint32_t buffer_count;
} VkRuntimeComputeState;

static uint64_t monotonic_ns(void) {
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

static uint64_t elapsed_since(uint64_t start) {
    uint64_t end = monotonic_ns();
    return start != 0u && end >= start ? end - start : 0u;
}

static VkRuntimeStatus compute_fail(VkRuntimeComputeResult *result,
                                    VkRuntimeStatus status,
                                    VkResult vulkan_result) {
    result->status = status;
    result->vulkan_result = vulkan_result;
    return status;
}

static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_bits,
                                 VkMemoryPropertyFlags required) {
    VkPhysicalDeviceMemoryProperties properties;
    memset(&properties, 0, sizeof(properties));
    vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
    for (uint32_t i = 0u; i < properties.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) != 0u &&
            (properties.memoryTypes[i].propertyFlags & required) ==
                required) {
            return i;
        }
    }
    return UINT32_MAX;
}

static void destroy_compute_state(VkRuntime *runtime,
                                  VkRuntimeComputeState *state) {
    if (state->fence != VK_NULL_HANDLE) {
        vkDestroyFence(runtime->device, state->fence, NULL);
    }
    if (state->command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(runtime->device, state->command_pool, NULL);
    }
    if (state->descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(runtime->device,
                                state->descriptor_pool,
                                NULL);
    }
    if (state->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(runtime->device, state->pipeline, NULL);
    }
    if (state->pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(runtime->device,
                                state->pipeline_layout,
                                NULL);
    }
    if (state->descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(runtime->device,
                                     state->descriptor_set_layout,
                                     NULL);
    }
    if (state->shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(runtime->device,
                              state->shader_module,
                              NULL);
    }
    for (uint32_t i = 0u; i < state->buffer_count; ++i) {
        if (state->buffers[i].mapped) {
            vkUnmapMemory(runtime->device, state->buffers[i].memory);
        }
        if (state->buffers[i].buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(runtime->device,
                            state->buffers[i].buffer,
                            NULL);
        }
        if (state->buffers[i].memory != VK_NULL_HANDLE) {
            vkFreeMemory(runtime->device, state->buffers[i].memory, NULL);
        }
    }
    memset(state, 0, sizeof(*state));
}

static VkRuntimeStatus validate_request(
    const VkRuntime *runtime,
    const VkRuntimeComputeRequest *request,
    VkRuntimeComputeResult *result) {
    if (!runtime || !request || !result ||
        runtime->device == VK_NULL_HANDLE ||
        runtime->physical_device == VK_NULL_HANDLE ||
        runtime->compute_queue == VK_NULL_HANDLE) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
                            VK_ERROR_INITIALIZATION_FAILED);
    }
    if (!request->spirv_words ||
        request->spirv_size < sizeof(uint32_t) * 5u ||
        request->spirv_size % sizeof(uint32_t) != 0u ||
        request->spirv_words[0] != VK_RUNTIME_SPIRV_MAGIC ||
        !request->entry_point || request->entry_point[0] == '\0') {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_SHADER_CODE_INVALID,
                            VK_ERROR_UNKNOWN);
    }
    if (!request->bindings || request->binding_count == 0u ||
        request->binding_count > VK_RUNTIME_MAX_COMPUTE_BINDINGS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_DESCRIPTOR_BINDING_INVALID,
                            VK_ERROR_INITIALIZATION_FAILED);
    }
    for (uint32_t i = 0u; i < request->binding_count; ++i) {
        if (request->bindings[i].size == 0u ||
            !request->bindings[i].upload_data ||
            (request->bindings[i].readback &&
             !request->bindings[i].readback_data)) {
            return compute_fail(
                result,
                VK_RUNTIME_STATUS_DESCRIPTOR_BINDING_INVALID,
                VK_ERROR_INITIALIZATION_FAILED);
        }
    }
    if (request->group_count_x == 0u ||
        request->group_count_y == 0u ||
        request->group_count_z == 0u) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
                            VK_ERROR_INITIALIZATION_FAILED);
    }
    return VK_RUNTIME_STATUS_OK;
}

static VkRuntimeStatus create_buffer(VkRuntime *runtime,
                                     const VkRuntimeComputeBinding *binding,
                                     VkRuntimeComputeBuffer *buffer,
                                     VkRuntimeComputeResult *result) {
    buffer->size = (VkDeviceSize)binding->size;

    VkBufferCreateInfo buffer_info;
    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer->size;
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult vk_result =
        vkCreateBuffer(runtime->device, &buffer_info, NULL, &buffer->buffer);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_BUFFER_CREATE_FAILED,
                            vk_result);
    }

    VkMemoryRequirements requirements;
    memset(&requirements, 0, sizeof(requirements));
    vkGetBufferMemoryRequirements(runtime->device,
                                  buffer->buffer,
                                  &requirements);
    uint32_t memory_type = find_memory_type(
        runtime->physical_device,
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_type == UINT32_MAX) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_MEMORY_TYPE_UNAVAILABLE,
                            VK_ERROR_FEATURE_NOT_PRESENT);
    }

    VkMemoryAllocateInfo allocation;
    memset(&allocation, 0, sizeof(allocation));
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_type;
    vk_result = vkAllocateMemory(runtime->device,
                                 &allocation,
                                 NULL,
                                 &buffer->memory);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_MEMORY_ALLOCATE_FAILED,
                            vk_result);
    }
    vk_result = vkBindBufferMemory(runtime->device,
                                   buffer->buffer,
                                   buffer->memory,
                                   0u);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_MEMORY_ALLOCATE_FAILED,
                            vk_result);
    }
    vk_result = vkMapMemory(runtime->device,
                            buffer->memory,
                            0u,
                            buffer->size,
                            0u,
                            &buffer->mapped);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_MEMORY_MAP_FAILED,
                            vk_result);
    }
    memcpy(buffer->mapped, binding->upload_data, binding->size);
    return VK_RUNTIME_STATUS_OK;
}

static VkRuntimeStatus create_shader_and_layout(
    VkRuntime *runtime,
    const VkRuntimeComputeRequest *request,
    VkRuntimeComputeState *state,
    VkRuntimeComputeResult *result) {
    VkShaderModuleCreateInfo shader_info;
    memset(&shader_info, 0, sizeof(shader_info));
    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.codeSize = request->spirv_size;
    shader_info.pCode = request->spirv_words;
    VkResult vk_result = vkCreateShaderModule(runtime->device,
                                              &shader_info,
                                              NULL,
                                              &state->shader_module);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_SHADER_MODULE_CREATE_FAILED,
                            vk_result);
    }

    VkDescriptorSetLayoutBinding bindings[VK_RUNTIME_MAX_COMPUTE_BINDINGS];
    memset(bindings, 0, sizeof(bindings));
    for (uint32_t i = 0u; i < request->binding_count; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1u;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo descriptor_layout;
    memset(&descriptor_layout, 0, sizeof(descriptor_layout));
    descriptor_layout.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout.bindingCount = request->binding_count;
    descriptor_layout.pBindings = bindings;
    vk_result = vkCreateDescriptorSetLayout(runtime->device,
                                             &descriptor_layout,
                                             NULL,
                                             &state->descriptor_set_layout);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_DESCRIPTOR_CREATE_FAILED,
                            vk_result);
    }

    VkPipelineLayoutCreateInfo pipeline_layout;
    memset(&pipeline_layout, 0, sizeof(pipeline_layout));
    pipeline_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout.setLayoutCount = 1u;
    pipeline_layout.pSetLayouts = &state->descriptor_set_layout;
    vk_result = vkCreatePipelineLayout(runtime->device,
                                        &pipeline_layout,
                                        NULL,
                                        &state->pipeline_layout);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_COMPUTE_PIPELINE_CREATE_FAILED,
                            vk_result);
    }

    VkPipelineShaderStageCreateInfo stage;
    memset(&stage, 0, sizeof(stage));
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = state->shader_module;
    stage.pName = request->entry_point;

    VkComputePipelineCreateInfo pipeline;
    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline.stage = stage;
    pipeline.layout = state->pipeline_layout;
    vk_result = vkCreateComputePipelines(runtime->device,
                                          VK_NULL_HANDLE,
                                          1u,
                                          &pipeline,
                                          NULL,
                                          &state->pipeline);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_COMPUTE_PIPELINE_CREATE_FAILED,
                            vk_result);
    }
    return VK_RUNTIME_STATUS_OK;
}

static VkRuntimeStatus create_descriptors(
    VkRuntime *runtime,
    const VkRuntimeComputeRequest *request,
    VkRuntimeComputeState *state,
    VkRuntimeComputeResult *result) {
    VkDescriptorPoolSize pool_size;
    memset(&pool_size, 0, sizeof(pool_size));
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = request->binding_count;

    VkDescriptorPoolCreateInfo pool;
    memset(&pool, 0, sizeof(pool));
    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.maxSets = 1u;
    pool.poolSizeCount = 1u;
    pool.pPoolSizes = &pool_size;
    VkResult vk_result = vkCreateDescriptorPool(runtime->device,
                                                 &pool,
                                                 NULL,
                                                 &state->descriptor_pool);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_DESCRIPTOR_CREATE_FAILED,
                            vk_result);
    }

    VkDescriptorSetAllocateInfo allocation;
    memset(&allocation, 0, sizeof(allocation));
    allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocation.descriptorPool = state->descriptor_pool;
    allocation.descriptorSetCount = 1u;
    allocation.pSetLayouts = &state->descriptor_set_layout;
    vk_result = vkAllocateDescriptorSets(runtime->device,
                                          &allocation,
                                          &state->descriptor_set);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_DESCRIPTOR_CREATE_FAILED,
                            vk_result);
    }

    VkDescriptorBufferInfo infos[VK_RUNTIME_MAX_COMPUTE_BINDINGS];
    VkWriteDescriptorSet writes[VK_RUNTIME_MAX_COMPUTE_BINDINGS];
    memset(infos, 0, sizeof(infos));
    memset(writes, 0, sizeof(writes));
    for (uint32_t i = 0u; i < request->binding_count; ++i) {
        infos[i].buffer = state->buffers[i].buffer;
        infos[i].offset = 0u;
        infos[i].range = state->buffers[i].size;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = state->descriptor_set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1u;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(runtime->device,
                           request->binding_count,
                           writes,
                           0u,
                           NULL);
    return VK_RUNTIME_STATUS_OK;
}

static VkRuntimeStatus create_and_record_commands(
    VkRuntime *runtime,
    const VkRuntimeComputeRequest *request,
    VkRuntimeComputeState *state,
    VkRuntimeComputeResult *result) {
    const VkRuntimeDeviceCapability *capability =
        &runtime->report.devices[runtime->report.selected_device_index];

    VkCommandPoolCreateInfo pool;
    memset(&pool, 0, sizeof(pool));
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool.queueFamilyIndex = capability->compute_queue_family;
    VkResult vk_result = vkCreateCommandPool(runtime->device,
                                              &pool,
                                              NULL,
                                              &state->command_pool);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
                            vk_result);
    }

    VkCommandBufferAllocateInfo allocation;
    memset(&allocation, 0, sizeof(allocation));
    allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation.commandPool = state->command_pool;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 1u;
    vk_result = vkAllocateCommandBuffers(runtime->device,
                                          &allocation,
                                          &state->command_buffer);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
                            vk_result);
    }

    VkCommandBufferBeginInfo begin;
    memset(&begin, 0, sizeof(begin));
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_result = vkBeginCommandBuffer(state->command_buffer, &begin);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
                            vk_result);
    }
    vkCmdBindPipeline(state->command_buffer,
                      VK_PIPELINE_BIND_POINT_COMPUTE,
                      state->pipeline);
    vkCmdBindDescriptorSets(state->command_buffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            state->pipeline_layout,
                            0u,
                            1u,
                            &state->descriptor_set,
                            0u,
                            NULL);
    vkCmdDispatch(state->command_buffer,
                  request->group_count_x,
                  request->group_count_y,
                  request->group_count_z);
    vk_result = vkEndCommandBuffer(state->command_buffer);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
                            vk_result);
    }

    VkFenceCreateInfo fence;
    memset(&fence, 0, sizeof(fence));
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vk_result = vkCreateFence(runtime->device, &fence, NULL, &state->fence);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
                            vk_result);
    }
    return VK_RUNTIME_STATUS_OK;
}

static VkRuntimeStatus submit_and_wait(
    VkRuntime *runtime,
    const VkRuntimeComputeRequest *request,
    VkRuntimeComputeState *state,
    VkRuntimeComputeResult *result) {
    VkSubmitInfo submit;
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &state->command_buffer;

    uint64_t start = monotonic_ns();
    VkResult vk_result =
        vkQueueSubmit(runtime->compute_queue, 1u, &submit, state->fence);
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_COMMAND_SUBMIT_FAILED,
                            vk_result);
    }
    vk_result = vkWaitForFences(runtime->device,
                                1u,
                                &state->fence,
                                VK_TRUE,
                                request->timeout_ns);
    result->submit_wait_ns = elapsed_since(start);
    if (vk_result == VK_TIMEOUT) {
        /*
         * S2 has no cancellation/device-loss recovery owner. Drain this finite
         * one-shot dispatch before destroying its resources, while preserving
         * the caller-visible bounded-wait timeout result.
         */
        VkResult drain_result = vkWaitForFences(runtime->device,
                                                 1u,
                                                 &state->fence,
                                                 VK_TRUE,
                                                 UINT64_MAX);
        if (drain_result != VK_SUCCESS) {
            return compute_fail(result,
                                VK_RUNTIME_STATUS_FENCE_WAIT_FAILED,
                                drain_result);
        }
        return compute_fail(result,
                            VK_RUNTIME_STATUS_FENCE_WAIT_TIMEOUT,
                            vk_result);
    }
    if (vk_result != VK_SUCCESS) {
        return compute_fail(result,
                            VK_RUNTIME_STATUS_FENCE_WAIT_FAILED,
                            vk_result);
    }
    return VK_RUNTIME_STATUS_OK;
}

VkRuntimeStatus vk_runtime_compute_dispatch(
    VkRuntime *runtime,
    const VkRuntimeComputeRequest *request,
    VkRuntimeComputeResult *result) {
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = VK_RUNTIME_STATUS_OK;
        result->vulkan_result = VK_SUCCESS;
    }
    VkRuntimeStatus status = validate_request(runtime, request, result);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }

    VkRuntimeComputeState state;
    memset(&state, 0, sizeof(state));
    state.buffer_count = request->binding_count;

    uint64_t upload_start = monotonic_ns();
    for (uint32_t i = 0u; i < request->binding_count; ++i) {
        status = create_buffer(runtime,
                               &request->bindings[i],
                               &state.buffers[i],
                               result);
        if (status != VK_RUNTIME_STATUS_OK) {
            destroy_compute_state(runtime, &state);
            return status;
        }
    }
    result->host_upload_ns = elapsed_since(upload_start);

    status = create_shader_and_layout(runtime, request, &state, result);
    if (status == VK_RUNTIME_STATUS_OK) {
        status = create_descriptors(runtime, request, &state, result);
    }
    if (status == VK_RUNTIME_STATUS_OK) {
        status =
            create_and_record_commands(runtime, request, &state, result);
    }
    if (status == VK_RUNTIME_STATUS_OK) {
        status = submit_and_wait(runtime, request, &state, result);
    }
    if (status != VK_RUNTIME_STATUS_OK) {
        destroy_compute_state(runtime, &state);
        return status;
    }

    uint64_t readback_start = monotonic_ns();
    for (uint32_t i = 0u; i < request->binding_count; ++i) {
        if (request->bindings[i].readback) {
            memcpy(request->bindings[i].readback_data,
                   state.buffers[i].mapped,
                   request->bindings[i].size);
        }
    }
    result->host_readback_ns = elapsed_since(readback_start);
    result->status = VK_RUNTIME_STATUS_OK;
    result->vulkan_result = VK_SUCCESS;
    destroy_compute_state(runtime, &state);
    return VK_RUNTIME_STATUS_OK;
}
