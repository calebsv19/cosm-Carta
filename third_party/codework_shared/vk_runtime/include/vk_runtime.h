#ifndef CODEWORK_VK_RUNTIME_H
#define CODEWORK_VK_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VK_RUNTIME_CAPABILITY_SCHEMA "codework_gpu_capability_report_v1"
#define VK_RUNTIME_CAPABILITY_SCHEMA_VERSION 1u
#define VK_RUNTIME_INVALID_QUEUE_FAMILY UINT32_MAX

#define VK_RUNTIME_PLATFORM_NAME_CAPACITY 32u
#define VK_RUNTIME_ARCHITECTURE_NAME_CAPACITY 32u
#define VK_RUNTIME_COMPILER_NAME_CAPACITY 192u
#define VK_RUNTIME_VERSION_TEXT_CAPACITY 32u
#define VK_RUNTIME_UUID_TEXT_CAPACITY 33u
#define VK_RUNTIME_MAX_RESIDENT_BINDINGS 16u

typedef enum VkRuntimeStatus {
    VK_RUNTIME_STATUS_OK = 0,
    VK_RUNTIME_STATUS_INVALID_ARGUMENT,
    VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY,
    VK_RUNTIME_STATUS_LOADER_UNAVAILABLE,
    VK_RUNTIME_STATUS_API_VERSION_UNSUPPORTED,
    VK_RUNTIME_STATUS_INSTANCE_EXTENSION_MISSING,
    VK_RUNTIME_STATUS_VALIDATION_LAYER_MISSING,
    VK_RUNTIME_STATUS_INSTANCE_CREATE_FAILED,
    VK_RUNTIME_STATUS_DEBUG_MESSENGER_CREATE_FAILED,
    VK_RUNTIME_STATUS_NO_PHYSICAL_DEVICE,
    VK_RUNTIME_STATUS_NO_SUITABLE_DEVICE,
    VK_RUNTIME_STATUS_DEVICE_EXTENSION_MISSING,
    VK_RUNTIME_STATUS_LOGICAL_DEVICE_CREATE_FAILED,
    VK_RUNTIME_STATUS_DEVICE_WAIT_FAILED,
    VK_RUNTIME_STATUS_SHADER_CODE_INVALID,
    VK_RUNTIME_STATUS_SHADER_MODULE_CREATE_FAILED,
    VK_RUNTIME_STATUS_DESCRIPTOR_BINDING_INVALID,
    VK_RUNTIME_STATUS_BUFFER_CREATE_FAILED,
    VK_RUNTIME_STATUS_MEMORY_TYPE_UNAVAILABLE,
    VK_RUNTIME_STATUS_MEMORY_ALLOCATE_FAILED,
    VK_RUNTIME_STATUS_MEMORY_MAP_FAILED,
    VK_RUNTIME_STATUS_DESCRIPTOR_CREATE_FAILED,
    VK_RUNTIME_STATUS_COMPUTE_PIPELINE_CREATE_FAILED,
    VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED,
    VK_RUNTIME_STATUS_COMMAND_SUBMIT_FAILED,
    VK_RUNTIME_STATUS_FENCE_WAIT_TIMEOUT,
    VK_RUNTIME_STATUS_FENCE_WAIT_FAILED,
    VK_RUNTIME_STATUS_RESOURCE_IN_USE,
    VK_RUNTIME_STATUS_BUFFER_RANGE_INVALID,
    VK_RUNTIME_STATUS_DEVICE_LOST,
    VK_RUNTIME_STATUS_TIMESTAMP_QUERY_CREATE_FAILED,
    VK_RUNTIME_STATUS_TIMESTAMP_RESULT_FAILED,
    VK_RUNTIME_STATUS_SERIALIZATION_FAILED,
    VK_RUNTIME_STATUS_INTERNAL_LIMIT_EXCEEDED
} VkRuntimeStatus;

typedef enum VkRuntimeDeviceRejectionBits {
    VK_RUNTIME_DEVICE_REJECT_NONE = 0u,
    VK_RUNTIME_DEVICE_REJECT_GRAPHICS_QUEUE_MISSING = 1u << 0,
    VK_RUNTIME_DEVICE_REJECT_COMPUTE_QUEUE_MISSING = 1u << 1,
    VK_RUNTIME_DEVICE_REJECT_TRANSFER_QUEUE_MISSING = 1u << 2,
    VK_RUNTIME_DEVICE_REJECT_PRESENT_QUEUE_MISSING = 1u << 3,
    VK_RUNTIME_DEVICE_REJECT_DEVICE_EXTENSION_MISSING = 1u << 4
} VkRuntimeDeviceRejectionBits;

typedef struct VkRuntimeConfig {
    const char *application_name;
    uint32_t application_version;
    uint32_t requested_api_version;
    bool enable_validation;
    bool require_validation;
    bool require_graphics_queue;
    bool require_compute_queue;
    bool require_transfer_queue;
    bool require_present_queue;
    bool prefer_discrete_gpu;
    bool create_logical_device;
    const char *const *instance_extensions;
    uint32_t instance_extension_count;
    const char *const *device_extensions;
    uint32_t device_extension_count;
} VkRuntimeConfig;

typedef struct VkRuntimeQueueFamilyCapability {
    uint32_t index;
    uint32_t queue_count;
    uint32_t queue_flags;
    uint32_t timestamp_valid_bits;
    uint32_t min_image_transfer_granularity[3];
    bool graphics;
    bool compute;
    bool transfer;
    bool sparse_binding;
    bool protected_queue;
    bool present;
} VkRuntimeQueueFamilyCapability;

typedef struct VkRuntimeMemoryHeapCapability {
    uint64_t size_bytes;
    uint32_t flags;
    bool device_local;
    bool multi_instance;
} VkRuntimeMemoryHeapCapability;

typedef struct VkRuntimeDeviceCapability {
    char device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    char driver_name[VK_MAX_DRIVER_NAME_SIZE];
    char driver_info[VK_MAX_DRIVER_INFO_SIZE];
    char device_uuid[VK_RUNTIME_UUID_TEXT_CAPACITY];

    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t device_type;
    uint32_t api_version;
    uint32_t driver_version;

    uint32_t subgroup_size;
    uint32_t subgroup_supported_stages;
    uint32_t subgroup_supported_operations;

    bool sampler_anisotropy;
    bool shader_float64;
    bool shader_int64;
    bool portability_subset;

    uint32_t graphics_queue_family;
    uint32_t compute_queue_family;
    uint32_t transfer_queue_family;
    uint32_t present_queue_family;
    uint32_t rejection_bits;
    bool suitable;
    bool selected;

    uint32_t queue_family_count;
    VkRuntimeQueueFamilyCapability *queue_families;

    uint32_t memory_heap_count;
    VkRuntimeMemoryHeapCapability memory_heaps[VK_MAX_MEMORY_HEAPS];

    uint32_t extension_count;
    char (*extensions)[VK_MAX_EXTENSION_NAME_SIZE];
} VkRuntimeDeviceCapability;

typedef struct VkRuntimeCapabilityReport {
    char platform[VK_RUNTIME_PLATFORM_NAME_CAPACITY];
    char architecture[VK_RUNTIME_ARCHITECTURE_NAME_CAPACITY];
    char compiler[VK_RUNTIME_COMPILER_NAME_CAPACITY];

    uint32_t schema_version;
    uint32_t vulkan_header_version;
    uint32_t requested_api_version;
    uint32_t loader_api_version;
    uint32_t negotiated_api_version;

    bool portability_enumeration_available;
    bool portability_enumeration_enabled;
    bool debug_utils_available;
    bool validation_requested;
    bool validation_available;
    bool validation_enabled;
    bool validation_load_failed;
    uint32_t validation_warning_count;
    uint32_t validation_error_count;

    VkRuntimeStatus status;
    VkResult vulkan_result;

    uint32_t device_count;
    uint32_t selected_device_index;
    VkRuntimeDeviceCapability *devices;
} VkRuntimeCapabilityReport;

typedef struct VkRuntime {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;
    VkQueue present_queue;
    VkRuntimeCapabilityReport report;
} VkRuntime;

const char *vk_runtime_version_string(void);

typedef struct VkRuntimeComputeBinding {
    const void *upload_data;
    void *readback_data;
    size_t size;
    bool readback;
} VkRuntimeComputeBinding;

typedef struct VkRuntimeComputeRequest {
    const uint32_t *spirv_words;
    size_t spirv_size;
    const char *entry_point;
    const VkRuntimeComputeBinding *bindings;
    uint32_t binding_count;
    uint32_t group_count_x;
    uint32_t group_count_y;
    uint32_t group_count_z;
    uint64_t timeout_ns;
} VkRuntimeComputeRequest;

typedef struct VkRuntimeComputeResult {
    VkRuntimeStatus status;
    VkResult vulkan_result;
    uint64_t host_upload_ns;
    uint64_t submit_wait_ns;
    uint64_t host_readback_ns;
} VkRuntimeComputeResult;

typedef enum VkRuntimeBufferRole {
    VK_RUNTIME_BUFFER_ROLE_HOST_STAGING = 0,
    VK_RUNTIME_BUFFER_ROLE_DEVICE_LOCAL = 1
} VkRuntimeBufferRole;

typedef struct VkRuntimeComputeSession {
    void *internal;
} VkRuntimeComputeSession;

typedef struct VkRuntimeResidentBuffer {
    void *internal;
} VkRuntimeResidentBuffer;

typedef struct VkRuntimeComputeProgram {
    void *internal;
} VkRuntimeComputeProgram;

typedef struct VkRuntimeResidentBinding {
    VkRuntimeResidentBuffer *buffer;
    size_t offset;
    size_t range;
} VkRuntimeResidentBinding;

typedef struct VkRuntimeComputeProgramConfig {
    const uint32_t *spirv_words;
    size_t spirv_size;
    const char *entry_point;
    const VkRuntimeResidentBinding *bindings;
    uint32_t binding_count;
} VkRuntimeComputeProgramConfig;

typedef struct VkRuntimeResidentDispatch {
    VkRuntimeComputeProgram *program;
    uint32_t group_count_x;
    uint32_t group_count_y;
    uint32_t group_count_z;
} VkRuntimeResidentDispatch;

typedef struct VkRuntimeResidentOperationResult {
    VkRuntimeStatus status;
    VkResult vulkan_result;
    uint64_t host_copy_ns;
    uint64_t submit_wait_ns;
    uint64_t gpu_start_tick;
    uint64_t gpu_end_tick;
    uint64_t gpu_elapsed_ns;
    uint32_t dispatch_count;
    uint32_t barrier_count;
    bool gpu_timestamp_supported;
    bool gpu_timestamp_valid;
} VkRuntimeResidentOperationResult;

typedef struct VkRuntimeComputeSessionInfo {
    uint32_t queue_family_index;
    uint32_t live_buffer_count;
    uint32_t live_program_count;
    uint64_t submission_count;
    uint64_t completed_submission_count;
    uint64_t upload_count;
    uint64_t readback_count;
    uint64_t dispatch_count;
    uint64_t barrier_count;
    uint64_t timestamp_measurement_count;
    uint32_t timestamp_valid_bits;
    double timestamp_period_ns;
    bool timestamp_supported;
    bool in_flight;
} VkRuntimeComputeSessionInfo;

typedef struct VkRuntimeResidentBufferInfo {
    VkRuntimeBufferRole role;
    size_t size;
    uint32_t memory_type_index;
    VkMemoryPropertyFlags memory_property_flags;
    uint32_t program_reference_count;
} VkRuntimeResidentBufferInfo;

void vk_runtime_config_defaults(VkRuntimeConfig *config);

VkRuntimeStatus vk_runtime_initialize(VkRuntime *runtime,
                                      const VkRuntimeConfig *config);
VkRuntimeStatus vk_runtime_initialize_instance(VkRuntime *runtime,
                                               const VkRuntimeConfig *config);
VkRuntimeStatus vk_runtime_initialize_device(VkRuntime *runtime,
                                             const VkRuntimeConfig *config,
                                             VkSurfaceKHR presentation_surface);
VkRuntimeStatus vk_runtime_close(VkRuntime *runtime);
void vk_runtime_shutdown(VkRuntime *runtime);

const VkRuntimeCapabilityReport *
vk_runtime_get_capability_report(const VkRuntime *runtime);

void vk_runtime_capability_report_destroy(VkRuntimeCapabilityReport *report);

const char *vk_runtime_status_name(VkRuntimeStatus status);
const char *vk_runtime_device_type_name(uint32_t device_type);

VkRuntimeStatus vk_runtime_capability_report_to_json(
    const VkRuntimeCapabilityReport *report,
    bool pretty,
    char *output,
    size_t output_capacity,
    size_t *output_size);

VkRuntimeStatus vk_runtime_compute_dispatch(
    VkRuntime *runtime,
    const VkRuntimeComputeRequest *request,
    VkRuntimeComputeResult *result);

VkRuntimeStatus vk_runtime_compute_session_initialize(
    VkRuntime *runtime,
    VkRuntimeComputeSession *session,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_compute_session_get_info(
    const VkRuntimeComputeSession *session,
    VkRuntimeComputeSessionInfo *info);

VkRuntimeStatus vk_runtime_compute_session_wait(
    VkRuntimeComputeSession *session,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_compute_session_close(
    VkRuntimeComputeSession *session,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_resident_buffer_create(
    VkRuntimeComputeSession *session,
    VkRuntimeBufferRole role,
    size_t size,
    VkRuntimeResidentBuffer *buffer,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_resident_buffer_get_info(
    const VkRuntimeResidentBuffer *buffer,
    VkRuntimeResidentBufferInfo *info);

VkRuntimeStatus vk_runtime_resident_buffer_destroy(
    VkRuntimeResidentBuffer *buffer,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_resident_buffer_upload(
    VkRuntimeComputeSession *session,
    VkRuntimeResidentBuffer *staging,
    VkRuntimeResidentBuffer *destination,
    const void *data,
    size_t size,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_resident_buffer_readback(
    VkRuntimeComputeSession *session,
    VkRuntimeResidentBuffer *source,
    VkRuntimeResidentBuffer *staging,
    void *data,
    size_t size,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_compute_program_create(
    VkRuntimeComputeSession *session,
    const VkRuntimeComputeProgramConfig *config,
    VkRuntimeComputeProgram *program,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_compute_program_destroy(
    VkRuntimeComputeProgram *program,
    VkRuntimeResidentOperationResult *result);

VkRuntimeStatus vk_runtime_compute_session_dispatch(
    VkRuntimeComputeSession *session,
    const VkRuntimeResidentDispatch *dispatches,
    uint32_t dispatch_count,
    uint64_t timeout_ns,
    VkRuntimeResidentOperationResult *result);

#ifdef __cplusplus
}
#endif

#endif
