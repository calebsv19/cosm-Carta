#include "vk_runtime.h"
#include "../src/vk_runtime_resident_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VK_RUNTIME_TEST_FIXTURE_PATH
#define VK_RUNTIME_TEST_FIXTURE_PATH "tests/fixtures/capability_report_v1.json"
#endif

static int failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr,                                                    \
                    "FAIL %s:%d: %s\n",                                        \
                    __FILE__,                                                  \
                    __LINE__,                                                  \
                    #condition);                                               \
            failures += 1;                                                     \
        }                                                                      \
    } while (0)

static char *read_fixture(void) {
    FILE *file = fopen(VK_RUNTIME_TEST_FIXTURE_PATH, "rb");
    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long measured = ftell(file);
    if (measured < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    size_t size = (size_t)measured;
    char *text = (char *)calloc(size + 1u, 1u);
    if (!text) {
        fclose(file);
        return NULL;
    }
    if (fread(text, 1u, size, file) != size) {
        free(text);
        fclose(file);
        return NULL;
    }
    fclose(file);
    while (size > 0u &&
           (text[size - 1u] == '\n' || text[size - 1u] == '\r')) {
        text[--size] = '\0';
    }
    return text;
}

static VkRuntimeCapabilityReport make_synthetic_report(void) {
    VkRuntimeCapabilityReport report;
    memset(&report, 0, sizeof(report));
    (void)snprintf(report.platform, sizeof(report.platform), "test-platform");
    (void)snprintf(
        report.architecture, sizeof(report.architecture), "test-arch");
    (void)snprintf(report.compiler, sizeof(report.compiler), "test-compiler");
    report.schema_version = VK_RUNTIME_CAPABILITY_SCHEMA_VERSION;
    report.vulkan_header_version = 4202497u;
    report.requested_api_version = VK_API_VERSION_1_2;
    report.loader_api_version = VK_API_VERSION_1_3;
    report.negotiated_api_version = VK_API_VERSION_1_2;
    report.portability_enumeration_available = true;
    report.portability_enumeration_enabled = true;
    report.debug_utils_available = true;
    report.validation_requested = true;
    report.validation_available = true;
    report.validation_enabled = true;
    report.validation_load_failed = false;
    report.validation_warning_count = 1u;
    report.validation_error_count = 0u;
    report.status = VK_RUNTIME_STATUS_OK;
    report.vulkan_result = VK_SUCCESS;
    report.device_count = 1u;
    report.selected_device_index = 0u;
    report.devices = calloc(1u, sizeof(*report.devices));

    VkRuntimeDeviceCapability *device = &report.devices[0];
    (void)snprintf(
        device->device_name, sizeof(device->device_name), "Synthetic \"GPU\"");
    (void)snprintf(device->driver_name,
                   sizeof(device->driver_name),
                   "Synthetic Driver");
    (void)snprintf(
        device->driver_info, sizeof(device->driver_info), "1.2\\test");
    (void)snprintf(device->device_uuid,
                   sizeof(device->device_uuid),
                   "000102030405060708090a0b0c0d0e0f");
    device->vendor_id = 4660u;
    device->device_id = 22136u;
    device->device_type = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    device->api_version = VK_API_VERSION_1_2;
    device->driver_version = 42u;
    device->subgroup_size = 32u;
    device->subgroup_supported_stages = 32u;
    device->subgroup_supported_operations = 31u;
    device->sampler_anisotropy = true;
    device->shader_float64 = false;
    device->shader_int64 = true;
    device->portability_subset = true;
    device->graphics_queue_family = 0u;
    device->compute_queue_family = 0u;
    device->transfer_queue_family = 0u;
    device->suitable = true;
    device->selected = true;

    device->queue_family_count = 1u;
    device->queue_families =
        calloc(1u, sizeof(*device->queue_families));
    device->queue_families[0].index = 0u;
    device->queue_families[0].queue_count = 1u;
    device->queue_families[0].queue_flags = 7u;
    device->queue_families[0].timestamp_valid_bits = 64u;
    device->queue_families[0].min_image_transfer_granularity[0] = 1u;
    device->queue_families[0].min_image_transfer_granularity[1] = 2u;
    device->queue_families[0].min_image_transfer_granularity[2] = 3u;
    device->queue_families[0].graphics = true;
    device->queue_families[0].compute = true;
    device->queue_families[0].transfer = true;

    device->memory_heap_count = 1u;
    device->memory_heaps[0].size_bytes = UINT64_C(8589934592);
    device->memory_heaps[0].flags = 1u;
    device->memory_heaps[0].device_local = true;

    device->extension_count = 2u;
    device->extensions = calloc(2u, sizeof(*device->extensions));
    (void)snprintf(device->extensions[0],
                   sizeof(device->extensions[0]),
                   "VK_EXT_alpha");
    (void)snprintf(device->extensions[1],
                   sizeof(device->extensions[1]),
                   "VK_KHR_portability_subset");
    return report;
}

int main(void) {
    VkRuntimeConfig config;
    vk_runtime_config_defaults(&config);
    CHECK(config.requested_api_version == VK_API_VERSION_1_2);
    CHECK(config.require_compute_queue);
    CHECK(!config.require_graphics_queue);
    CHECK(config.create_logical_device);
    CHECK(strcmp(vk_runtime_status_name(VK_RUNTIME_STATUS_OK), "ok") == 0);
    CHECK(strcmp(vk_runtime_status_name(
                     VK_RUNTIME_STATUS_VALIDATION_LAYER_MISSING),
                 "validation_layer_missing") == 0);
    CHECK(strcmp(vk_runtime_status_name(
                     VK_RUNTIME_STATUS_DESCRIPTOR_BINDING_INVALID),
                 "descriptor_binding_invalid") == 0);
    CHECK(strcmp(vk_runtime_status_name(
                     VK_RUNTIME_STATUS_FENCE_WAIT_TIMEOUT),
                 "fence_wait_timeout") == 0);
    CHECK(strcmp(vk_runtime_status_name(
                     VK_RUNTIME_STATUS_RESOURCE_IN_USE),
                 "resource_in_use") == 0);
    CHECK(strcmp(vk_runtime_status_name(
                     VK_RUNTIME_STATUS_BUFFER_RANGE_INVALID),
                 "buffer_range_invalid") == 0);
    CHECK(strcmp(vk_runtime_status_name(
                     VK_RUNTIME_STATUS_DEVICE_LOST),
                 "device_lost") == 0);
    CHECK(strcmp(vk_runtime_status_name(
                     VK_RUNTIME_STATUS_TIMESTAMP_QUERY_CREATE_FAILED),
                 "timestamp_query_create_failed") == 0);
    CHECK(strcmp(vk_runtime_status_name(
                     VK_RUNTIME_STATUS_TIMESTAMP_RESULT_FAILED),
                 "timestamp_result_failed") == 0);
    CHECK(vk_runtime_timing_delta_ticks(10u, 25u, 64u) == 15u);
    CHECK(vk_runtime_timing_delta_ticks(250u, 5u, 8u) == 11u);
    CHECK(vk_runtime_timing_ticks_to_ns(7u, 1.5) == 11u);
    CHECK(vk_runtime_timing_ticks_to_ns(UINT64_MAX, 2.0) ==
          UINT64_MAX);
    CHECK(strcmp(vk_runtime_device_type_name(
                     VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU),
                 "integrated_gpu") == 0);
    CHECK(vk_runtime_initialize(NULL, &config) ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    VkRuntimeComputeResult compute_result;
    CHECK(vk_runtime_compute_dispatch(NULL, NULL, &compute_result) ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    CHECK(compute_result.status == VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    VkRuntimeComputeSession resident_session;
    VkRuntimeResidentBuffer resident_buffer;
    VkRuntimeComputeProgram resident_program;
    VkRuntimeResidentOperationResult resident_result;
    VkRuntimeComputeSessionInfo resident_info;
    memset(&resident_session, 0, sizeof(resident_session));
    memset(&resident_buffer, 0, sizeof(resident_buffer));
    memset(&resident_program, 0, sizeof(resident_program));
    CHECK(vk_runtime_compute_session_initialize(
              NULL, &resident_session, &resident_result) ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    CHECK(resident_result.status == VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    CHECK(vk_runtime_compute_session_get_info(
              &resident_session, &resident_info) ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    CHECK(vk_runtime_compute_session_wait(
              &resident_session, 1u, &resident_result) ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    CHECK(vk_runtime_compute_session_close(
              &resident_session, &resident_result) ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    CHECK(vk_runtime_resident_buffer_create(
              &resident_session,
              VK_RUNTIME_BUFFER_ROLE_DEVICE_LOCAL,
              sizeof(uint32_t),
              &resident_buffer,
              &resident_result) ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    CHECK(vk_runtime_resident_buffer_get_info(
              &resident_buffer, NULL) ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    CHECK(vk_runtime_compute_program_destroy(
              &resident_program, &resident_result) ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    VkRuntime invalid_runtime;
    config.application_name = NULL;
    CHECK(vk_runtime_initialize(&invalid_runtime, &config) ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    CHECK(invalid_runtime.report.status ==
          VK_RUNTIME_STATUS_INVALID_ARGUMENT);
    vk_runtime_config_defaults(&config);

    VkRuntimeCapabilityReport report = make_synthetic_report();
    CHECK(report.devices != NULL);
    CHECK(report.devices[0].queue_families != NULL);
    CHECK(report.devices[0].extensions != NULL);

    size_t required_size = 0u;
    CHECK(vk_runtime_capability_report_to_json(
              &report, false, NULL, 0u, &required_size) ==
          VK_RUNTIME_STATUS_OK);
    CHECK(required_size > 1u);

    char *json = (char *)malloc(required_size);
    CHECK(json != NULL);
    CHECK(vk_runtime_capability_report_to_json(
              &report, false, json, required_size, &required_size) ==
          VK_RUNTIME_STATUS_OK);

    char too_small[8];
    size_t too_small_required = 0u;
    CHECK(vk_runtime_capability_report_to_json(
              &report,
              false,
              too_small,
              sizeof(too_small),
              &too_small_required) ==
          VK_RUNTIME_STATUS_SERIALIZATION_FAILED);
    CHECK(too_small_required >= sizeof(too_small));
    CHECK(too_small_required == required_size);

    report.devices[0].rejection_bits =
        VK_RUNTIME_DEVICE_REJECT_COMPUTE_QUEUE_MISSING |
        VK_RUNTIME_DEVICE_REJECT_TRANSFER_QUEUE_MISSING;
    size_t rejection_size = 0u;
    CHECK(vk_runtime_capability_report_to_json(
              &report, false, NULL, 0u, &rejection_size) ==
          VK_RUNTIME_STATUS_OK);
    char *rejection_json = (char *)malloc(rejection_size);
    CHECK(rejection_json != NULL);
    CHECK(vk_runtime_capability_report_to_json(
              &report,
              false,
              rejection_json,
              rejection_size,
              &rejection_size) == VK_RUNTIME_STATUS_OK);
    CHECK(strstr(rejection_json, "compute_queue_missing") != NULL);
    CHECK(strstr(rejection_json, "transfer_queue_missing") != NULL);
    free(rejection_json);
    report.devices[0].rejection_bits = VK_RUNTIME_DEVICE_REJECT_NONE;

    char *fixture = read_fixture();
    CHECK(fixture != NULL);
    if (fixture && json && strcmp(fixture, json) != 0) {
        fprintf(stderr, "expected:\n%s\nactual:\n%s\n", fixture, json);
        failures += 1;
    }

    free(fixture);
    free(json);
    vk_runtime_capability_report_destroy(&report);
    CHECK(report.devices == NULL);
    CHECK(report.device_count == 0u);

    if (failures != 0) {
        fprintf(stderr, "vk_runtime contract tests: %d failure(s)\n", failures);
        return 1;
    }
    printf("vk_runtime contract tests: ok\n");
    return 0;
}
