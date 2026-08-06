#ifndef CODEWORK_VK_RUNTIME_RESIDENT_INTERNAL_H
#define CODEWORK_VK_RUNTIME_RESIDENT_INTERNAL_H

#include "vk_runtime.h"

#define VK_RUNTIME_SPIRV_MAGIC 0x07230203u

typedef struct VkRuntimeComputeSessionState VkRuntimeComputeSessionState;
typedef struct VkRuntimeResidentBufferState VkRuntimeResidentBufferState;
typedef struct VkRuntimeComputeProgramState VkRuntimeComputeProgramState;

struct VkRuntimeComputeSessionState {
    VkRuntime *runtime;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkFence fence;
    VkQueryPool timestamp_query_pool;
    uint32_t queue_family_index;
    uint32_t timestamp_valid_bits;
    double timestamp_period_ns;
    uint32_t live_buffer_count;
    uint32_t live_program_count;
    uint64_t submission_count;
    uint64_t completed_submission_count;
    uint64_t upload_count;
    uint64_t readback_count;
    uint64_t dispatch_count;
    uint64_t barrier_count;
    uint64_t timestamp_measurement_count;
    bool timestamp_supported;
    bool timestamp_pending;
    bool in_flight;
};

struct VkRuntimeResidentBufferState {
    VkRuntimeComputeSessionState *session;
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *mapped;
    VkDeviceSize size;
    VkRuntimeBufferRole role;
    uint32_t memory_type_index;
    VkMemoryPropertyFlags memory_property_flags;
    uint32_t program_reference_count;
};

struct VkRuntimeComputeProgramState {
    VkRuntimeComputeSessionState *session;
    VkShaderModule shader_module;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkRuntimeResidentBufferState
        *bindings[VK_RUNTIME_MAX_RESIDENT_BINDINGS];
    uint32_t binding_count;
};

uint64_t vk_runtime_resident_monotonic_ns(void);
uint64_t vk_runtime_resident_elapsed_since(uint64_t start);

void vk_runtime_resident_result_reset(
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_resident_fail(
    VkRuntimeResidentOperationResult *result,
    VkRuntimeStatus status,
    VkResult vulkan_result);

VkRuntimeStatus vk_runtime_resident_fail_vk(
    VkRuntimeResidentOperationResult *result,
    VkRuntimeStatus fallback,
    VkResult vulkan_result);

VkRuntimeComputeSessionState *vk_runtime_resident_session_state(
    VkRuntimeComputeSession *session);

VkRuntimeStatus vk_runtime_resident_begin_commands(
    VkRuntimeComputeSessionState *state,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_resident_end_submit_wait(
    VkRuntimeComputeSessionState *state,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_timing_session_initialize(
    VkRuntimeComputeSessionState *state,
    VkRuntimeResidentOperationResult *result);

uint64_t vk_runtime_timing_delta_ticks(
    uint64_t start,
    uint64_t end,
    uint32_t valid_bits);

uint64_t vk_runtime_timing_ticks_to_ns(
    uint64_t ticks,
    double period_ns);

void vk_runtime_timing_session_destroy(
    VkRuntimeComputeSessionState *state);

void vk_runtime_timing_record_begin(
    VkRuntimeComputeSessionState *state,
    VkRuntimeResidentOperationResult *result);

void vk_runtime_timing_record_end(
    VkRuntimeComputeSessionState *state);

VkRuntimeStatus vk_runtime_timing_resolve(
    VkRuntimeComputeSessionState *state,
    VkRuntimeResidentOperationResult *result);

#endif
