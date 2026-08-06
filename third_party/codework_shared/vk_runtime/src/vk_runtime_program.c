#include "vk_runtime_resident_internal.h"

#include <stdlib.h>
#include <string.h>

static void destroy_program_state(VkRuntimeComputeProgramState *state) {
    if (!state) {
        return;
    }
    VkDevice device = state->session->runtime->device;
    if (state->descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, state->descriptor_pool, NULL);
    }
    if (state->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, state->pipeline, NULL);
    }
    if (state->pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, state->pipeline_layout, NULL);
    }
    if (state->descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device,
                                     state->descriptor_set_layout,
                                     NULL);
    }
    if (state->shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, state->shader_module, NULL);
    }
    free(state);
}

static VkRuntimeStatus validate_program_config(
    VkRuntimeComputeSessionState *session,
    const VkRuntimeComputeProgramConfig *config,
    VkRuntimeResidentOperationResult *result) {
    if (!session || !config || !config->spirv_words ||
        config->spirv_size < sizeof(uint32_t) * 5u ||
        config->spirv_size % sizeof(uint32_t) != 0u ||
        config->spirv_words[0] != VK_RUNTIME_SPIRV_MAGIC ||
        !config->entry_point || config->entry_point[0] == '\0') {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_SHADER_CODE_INVALID,
            VK_ERROR_UNKNOWN);
    }
    if (!config->bindings || config->binding_count == 0u ||
        config->binding_count > VK_RUNTIME_MAX_RESIDENT_BINDINGS) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_DESCRIPTOR_BINDING_INVALID,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    for (uint32_t i = 0u; i < config->binding_count; ++i) {
        const VkRuntimeResidentBinding *binding = &config->bindings[i];
        VkRuntimeResidentBufferState *buffer =
            binding->buffer
                ? (VkRuntimeResidentBufferState *)
                      binding->buffer->internal
                : NULL;
        if (!buffer || buffer->session != session ||
            buffer->role != VK_RUNTIME_BUFFER_ROLE_DEVICE_LOCAL ||
            binding->range == 0u ||
            (VkDeviceSize)binding->offset > buffer->size ||
            (VkDeviceSize)binding->range >
                buffer->size - (VkDeviceSize)binding->offset) {
            return vk_runtime_resident_fail(
                result,
                VK_RUNTIME_STATUS_DESCRIPTOR_BINDING_INVALID,
                VK_ERROR_INITIALIZATION_FAILED);
        }
    }
    return VK_RUNTIME_STATUS_OK;
}

VkRuntimeStatus vk_runtime_compute_program_create(
    VkRuntimeComputeSession *session,
    const VkRuntimeComputeProgramConfig *config,
    VkRuntimeComputeProgram *program,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_resident_result_reset(result);
    VkRuntimeComputeSessionState *session_state =
        vk_runtime_resident_session_state(session);
    if (!program || program->internal ||
        (session_state && session_state->in_flight)) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    VkRuntimeStatus status =
        validate_program_config(session_state, config, result);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }
    VkRuntimeComputeProgramState *state =
        (VkRuntimeComputeProgramState *)calloc(1u, sizeof(*state));
    if (!state) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY,
            VK_ERROR_OUT_OF_HOST_MEMORY);
    }
    state->session = session_state;
    state->binding_count = config->binding_count;
    VkDevice device = session_state->runtime->device;

    VkShaderModuleCreateInfo shader;
    memset(&shader, 0, sizeof(shader));
    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.codeSize = config->spirv_size;
    shader.pCode = config->spirv_words;
    VkResult vk_result =
        vkCreateShaderModule(device,
                             &shader,
                             NULL,
                             &state->shader_module);
    VkRuntimeStatus failure_status =
        VK_RUNTIME_STATUS_SHADER_MODULE_CREATE_FAILED;

    VkDescriptorSetLayoutBinding
        layout_bindings[VK_RUNTIME_MAX_RESIDENT_BINDINGS];
    memset(layout_bindings, 0, sizeof(layout_bindings));
    for (uint32_t i = 0u; i < config->binding_count; ++i) {
        layout_bindings[i].binding = i;
        layout_bindings[i].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layout_bindings[i].descriptorCount = 1u;
        layout_bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    if (vk_result == VK_SUCCESS) {
        failure_status =
            VK_RUNTIME_STATUS_DESCRIPTOR_CREATE_FAILED;
        VkDescriptorSetLayoutCreateInfo layout;
        memset(&layout, 0, sizeof(layout));
        layout.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.bindingCount = config->binding_count;
        layout.pBindings = layout_bindings;
        vk_result =
            vkCreateDescriptorSetLayout(
                device,
                &layout,
                NULL,
                &state->descriptor_set_layout);
    }
    if (vk_result == VK_SUCCESS) {
        failure_status =
            VK_RUNTIME_STATUS_COMPUTE_PIPELINE_CREATE_FAILED;
        VkPipelineLayoutCreateInfo layout;
        memset(&layout, 0, sizeof(layout));
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1u;
        layout.pSetLayouts = &state->descriptor_set_layout;
        vk_result =
            vkCreatePipelineLayout(device,
                                   &layout,
                                   NULL,
                                   &state->pipeline_layout);
    }
    if (vk_result == VK_SUCCESS) {
        VkPipelineShaderStageCreateInfo stage;
        memset(&stage, 0, sizeof(stage));
        stage.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = state->shader_module;
        stage.pName = config->entry_point;
        VkComputePipelineCreateInfo pipeline;
        memset(&pipeline, 0, sizeof(pipeline));
        pipeline.sType =
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline.stage = stage;
        pipeline.layout = state->pipeline_layout;
        vk_result =
            vkCreateComputePipelines(device,
                                     VK_NULL_HANDLE,
                                     1u,
                                     &pipeline,
                                     NULL,
                                     &state->pipeline);
    }
    if (vk_result != VK_SUCCESS) {
        destroy_program_state(state);
        return vk_runtime_resident_fail_vk(
            result,
            failure_status,
            vk_result);
    }

    VkDescriptorPoolSize pool_size;
    memset(&pool_size, 0, sizeof(pool_size));
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = config->binding_count;
    VkDescriptorPoolCreateInfo pool;
    memset(&pool, 0, sizeof(pool));
    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.maxSets = 1u;
    pool.poolSizeCount = 1u;
    pool.pPoolSizes = &pool_size;
    vk_result =
        vkCreateDescriptorPool(device,
                               &pool,
                               NULL,
                               &state->descriptor_pool);
    if (vk_result == VK_SUCCESS) {
        VkDescriptorSetAllocateInfo allocation;
        memset(&allocation, 0, sizeof(allocation));
        allocation.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocation.descriptorPool = state->descriptor_pool;
        allocation.descriptorSetCount = 1u;
        allocation.pSetLayouts = &state->descriptor_set_layout;
        vk_result =
            vkAllocateDescriptorSets(device,
                                     &allocation,
                                     &state->descriptor_set);
    }
    if (vk_result != VK_SUCCESS) {
        destroy_program_state(state);
        return vk_runtime_resident_fail_vk(
            result,
            VK_RUNTIME_STATUS_DESCRIPTOR_CREATE_FAILED,
            vk_result);
    }

    VkDescriptorBufferInfo
        infos[VK_RUNTIME_MAX_RESIDENT_BINDINGS];
    VkWriteDescriptorSet writes[VK_RUNTIME_MAX_RESIDENT_BINDINGS];
    memset(infos, 0, sizeof(infos));
    memset(writes, 0, sizeof(writes));
    for (uint32_t i = 0u; i < config->binding_count; ++i) {
        VkRuntimeResidentBufferState *buffer =
            (VkRuntimeResidentBufferState *)
                config->bindings[i].buffer->internal;
        state->bindings[i] = buffer;
        infos[i].buffer = buffer->buffer;
        infos[i].offset = (VkDeviceSize)config->bindings[i].offset;
        infos[i].range = (VkDeviceSize)config->bindings[i].range;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = state->descriptor_set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1u;
        writes[i].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(device,
                           config->binding_count,
                           writes,
                           0u,
                           NULL);
    for (uint32_t i = 0u; i < config->binding_count; ++i) {
        state->bindings[i]->program_reference_count += 1u;
    }
    session_state->live_program_count += 1u;
    program->internal = state;
    return VK_RUNTIME_STATUS_OK;
}

VkRuntimeStatus vk_runtime_compute_program_destroy(
    VkRuntimeComputeProgram *program,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_resident_result_reset(result);
    if (!program || !program->internal) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    VkRuntimeComputeProgramState *state =
        (VkRuntimeComputeProgramState *)program->internal;
    if (state->session->in_flight) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_RESOURCE_IN_USE,
            VK_NOT_READY);
    }
    for (uint32_t i = 0u; i < state->binding_count; ++i) {
        state->bindings[i]->program_reference_count -= 1u;
    }
    state->session->live_program_count -= 1u;
    destroy_program_state(state);
    program->internal = NULL;
    return VK_RUNTIME_STATUS_OK;
}

VkRuntimeStatus vk_runtime_compute_session_dispatch(
    VkRuntimeComputeSession *session,
    const VkRuntimeResidentDispatch *dispatches,
    uint32_t dispatch_count,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result) {
    vk_runtime_resident_result_reset(result);
    VkRuntimeComputeSessionState *session_state =
        vk_runtime_resident_session_state(session);
    if (!session_state || !dispatches || dispatch_count == 0u) {
        return vk_runtime_resident_fail(
            result,
            VK_RUNTIME_STATUS_INVALID_ARGUMENT,
            VK_ERROR_INITIALIZATION_FAILED);
    }
    for (uint32_t i = 0u; i < dispatch_count; ++i) {
        VkRuntimeComputeProgramState *program =
            dispatches[i].program
                ? (VkRuntimeComputeProgramState *)
                      dispatches[i].program->internal
                : NULL;
        if (!program || program->session != session_state ||
            dispatches[i].group_count_x == 0u ||
            dispatches[i].group_count_y == 0u ||
            dispatches[i].group_count_z == 0u) {
            return vk_runtime_resident_fail(
                result,
                VK_RUNTIME_STATUS_INVALID_ARGUMENT,
                VK_ERROR_INITIALIZATION_FAILED);
        }
    }
    VkRuntimeStatus status =
        vk_runtime_resident_begin_commands(session_state, result);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }
    for (uint32_t i = 0u; i < dispatch_count; ++i) {
        VkRuntimeComputeProgramState *program =
            (VkRuntimeComputeProgramState *)
                dispatches[i].program->internal;
        vkCmdBindPipeline(session_state->command_buffer,
                          VK_PIPELINE_BIND_POINT_COMPUTE,
                          program->pipeline);
        vkCmdBindDescriptorSets(session_state->command_buffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                program->pipeline_layout,
                                0u,
                                1u,
                                &program->descriptor_set,
                                0u,
                                NULL);
        vkCmdDispatch(session_state->command_buffer,
                      dispatches[i].group_count_x,
                      dispatches[i].group_count_y,
                      dispatches[i].group_count_z);
        if (i + 1u < dispatch_count) {
            VkMemoryBarrier barrier;
            memset(&barrier, 0, sizeof(barrier));
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask =
                VK_ACCESS_SHADER_READ_BIT |
                VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(
                session_state->command_buffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0u,
                1u,
                &barrier,
                0u,
                NULL,
                0u,
                NULL);
        }
    }
    if (result) {
        result->dispatch_count = dispatch_count;
        result->barrier_count = dispatch_count - 1u;
    }
    status = vk_runtime_resident_end_submit_wait(
        session_state, timeout_ns, result);
    if (status == VK_RUNTIME_STATUS_OK ||
        status == VK_RUNTIME_STATUS_FENCE_WAIT_TIMEOUT) {
        session_state->dispatch_count += dispatch_count;
        session_state->barrier_count += dispatch_count - 1u;
    }
    return status;
}
