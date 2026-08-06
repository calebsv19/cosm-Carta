#include "vk_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VALUE_COUNT 1025u
#define LOCAL_SIZE_X 64u
#define LIFECYCLE_CYCLES 4u
#define CHAINS_PER_CYCLE 3u
#define WAIT_TIMEOUT_NS 5000000000ull

typedef struct Options {
    const char *spirv_path;
    const char *spirv_sha256;
    const char *output_path;
    bool require_validation;
} Options;

typedef struct SpirvFile {
    uint32_t *words;
    size_t size;
} SpirvFile;

typedef struct ProofState {
    uint64_t submission_count;
    uint64_t completed_submission_count;
    uint64_t upload_count;
    uint64_t readback_count;
    uint64_t dispatch_count;
    uint64_t barrier_count;
    uint64_t timestamp_measurement_count;
    uint64_t upload_host_copy_ns;
    uint64_t upload_submit_wait_ns;
    uint64_t dispatch_submit_wait_ns;
    uint64_t readback_submit_wait_ns;
    uint64_t readback_host_copy_ns;
    uint32_t queue_family_index;
    VkMemoryPropertyFlags staging_flags;
    VkMemoryPropertyFlags device_a_flags;
    VkMemoryPropertyFlags device_b_flags;
    VkRuntimeStatus null_session_status;
    VkRuntimeStatus double_session_status;
    VkRuntimeStatus range_status;
    VkRuntimeStatus binding_status;
    VkRuntimeStatus referenced_buffer_status;
    VkRuntimeStatus live_close_status;
    VkRuntimeStatus timeout_status;
    VkRuntimeStatus timeout_close_status;
    VkRuntimeStatus timeout_recovery_status;
    bool timeout_in_flight_observed;
    bool timeout_timestamp_valid;
    bool final_resource_counts_zero;
    bool parity;
    uint32_t final_input[VALUE_COUNT];
    uint32_t final_expected[VALUE_COUNT];
    uint32_t final_output[VALUE_COUNT];
} ProofState;

static bool parse_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--u32") == 0 && i + 1 < argc) {
            options->spirv_path = argv[++i];
        } else if (strcmp(argv[i], "--u32-sha256") == 0 &&
                   i + 1 < argc) {
            options->spirv_sha256 = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 &&
                   i + 1 < argc) {
            options->output_path = argv[++i];
        } else if (strcmp(argv[i], "--require-validation") == 0) {
            options->require_validation = true;
        } else {
            return false;
        }
    }
    return options->spirv_path && options->spirv_sha256 &&
           options->output_path;
}

static bool read_spirv(const char *path, SpirvFile *spirv) {
    memset(spirv, 0, sizeof(*spirv));
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long length = ftell(file);
    if (length <= 0 || (size_t)length % sizeof(uint32_t) != 0u ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    spirv->words = (uint32_t *)malloc((size_t)length);
    if (!spirv->words) {
        fclose(file);
        return false;
    }
    spirv->size = (size_t)length;
    bool ok =
        fread(spirv->words, 1u, spirv->size, file) == spirv->size;
    fclose(file);
    if (!ok) {
        free(spirv->words);
        memset(spirv, 0, sizeof(*spirv));
    }
    return ok;
}

static uint32_t transform(uint32_t value) {
    return value * 3u + 7u;
}

static void fill_fixture(uint32_t cycle,
                         uint32_t iteration,
                         uint32_t *input,
                         uint32_t *expected,
                         uint32_t *output) {
    for (uint32_t i = 0u; i < VALUE_COUNT; ++i) {
        input[i] =
            (i * 2654435761u) ^
            (0x9e3779b9u + cycle * 0x10001u + iteration * 0x101u);
        expected[i] = transform(transform(input[i]));
        output[i] = 0u;
    }
}

static bool exact_parity(const uint32_t *expected,
                         const uint32_t *output) {
    for (uint32_t i = 0u; i < VALUE_COUNT; ++i) {
        if (expected[i] != output[i]) {
            return false;
        }
    }
    return true;
}

static void collect_operation_timing(
    ProofState *proof,
    const VkRuntimeResidentOperationResult *result,
    uint32_t lane) {
    if (lane == 0u) {
        proof->upload_host_copy_ns += result->host_copy_ns;
        proof->upload_submit_wait_ns += result->submit_wait_ns;
    } else if (lane == 1u) {
        proof->dispatch_submit_wait_ns += result->submit_wait_ns;
    } else {
        proof->readback_submit_wait_ns += result->submit_wait_ns;
        proof->readback_host_copy_ns += result->host_copy_ns;
    }
}

static bool destroy_cycle_resources(
    VkRuntimeComputeSession *session,
    VkRuntimeComputeProgram *forward,
    VkRuntimeComputeProgram *reverse,
    VkRuntimeResidentBuffer *staging,
    VkRuntimeResidentBuffer *device_a,
    VkRuntimeResidentBuffer *device_b,
    ProofState *proof) {
    VkRuntimeResidentOperationResult result;
    if (vk_runtime_compute_program_destroy(reverse, &result) !=
            VK_RUNTIME_STATUS_OK ||
        vk_runtime_compute_program_destroy(forward, &result) !=
            VK_RUNTIME_STATUS_OK ||
        vk_runtime_resident_buffer_destroy(device_b, &result) !=
            VK_RUNTIME_STATUS_OK ||
        vk_runtime_resident_buffer_destroy(device_a, &result) !=
            VK_RUNTIME_STATUS_OK ||
        vk_runtime_resident_buffer_destroy(staging, &result) !=
            VK_RUNTIME_STATUS_OK) {
        return false;
    }
    VkRuntimeComputeSessionInfo info;
    if (vk_runtime_compute_session_get_info(session, &info) !=
        VK_RUNTIME_STATUS_OK) {
        return false;
    }
    proof->submission_count += info.submission_count;
    proof->completed_submission_count +=
        info.completed_submission_count;
    proof->upload_count += info.upload_count;
    proof->readback_count += info.readback_count;
    proof->dispatch_count += info.dispatch_count;
    proof->barrier_count += info.barrier_count;
    proof->timestamp_measurement_count +=
        info.timestamp_measurement_count;
    proof->final_resource_counts_zero =
        proof->final_resource_counts_zero &&
        info.live_buffer_count == 0u &&
        info.live_program_count == 0u && !info.in_flight;
    return vk_runtime_compute_session_close(session, &result) ==
           VK_RUNTIME_STATUS_OK;
}

static bool run_cycle(VkRuntime *runtime,
                      const SpirvFile *spirv,
                      uint32_t cycle,
                      ProofState *proof) {
    VkRuntimeComputeSession session;
    VkRuntimeResidentBuffer staging;
    VkRuntimeResidentBuffer device_a;
    VkRuntimeResidentBuffer device_b;
    VkRuntimeComputeProgram forward;
    VkRuntimeComputeProgram reverse;
    memset(&session, 0, sizeof(session));
    memset(&staging, 0, sizeof(staging));
    memset(&device_a, 0, sizeof(device_a));
    memset(&device_b, 0, sizeof(device_b));
    memset(&forward, 0, sizeof(forward));
    memset(&reverse, 0, sizeof(reverse));
    VkRuntimeResidentOperationResult result;
    size_t byte_size = sizeof(proof->final_input);

    if (vk_runtime_compute_session_initialize(runtime,
                                              &session,
                                              &result) !=
            VK_RUNTIME_STATUS_OK) {
        return false;
    }
    proof->double_session_status =
        vk_runtime_compute_session_initialize(runtime,
                                              &session,
                                              &result);
    if (proof->double_session_status !=
            VK_RUNTIME_STATUS_INVALID_ARGUMENT ||
        vk_runtime_resident_buffer_create(
            &session,
            VK_RUNTIME_BUFFER_ROLE_HOST_STAGING,
            byte_size,
            &staging,
            &result) != VK_RUNTIME_STATUS_OK ||
        vk_runtime_resident_buffer_create(
            &session,
            VK_RUNTIME_BUFFER_ROLE_DEVICE_LOCAL,
            byte_size,
            &device_a,
            &result) != VK_RUNTIME_STATUS_OK ||
        vk_runtime_resident_buffer_create(
            &session,
            VK_RUNTIME_BUFFER_ROLE_DEVICE_LOCAL,
            byte_size,
            &device_b,
            &result) != VK_RUNTIME_STATUS_OK) {
        return false;
    }

    VkRuntimeComputeSessionInfo session_info;
    VkRuntimeResidentBufferInfo staging_info;
    VkRuntimeResidentBufferInfo device_a_info;
    VkRuntimeResidentBufferInfo device_b_info;
    if (vk_runtime_compute_session_get_info(&session, &session_info) !=
            VK_RUNTIME_STATUS_OK ||
        vk_runtime_resident_buffer_get_info(&staging,
                                            &staging_info) !=
            VK_RUNTIME_STATUS_OK ||
        vk_runtime_resident_buffer_get_info(&device_a,
                                            &device_a_info) !=
            VK_RUNTIME_STATUS_OK ||
        vk_runtime_resident_buffer_get_info(&device_b,
                                            &device_b_info) !=
            VK_RUNTIME_STATUS_OK) {
        return false;
    }
    proof->queue_family_index = session_info.queue_family_index;
    proof->staging_flags = staging_info.memory_property_flags;
    proof->device_a_flags = device_a_info.memory_property_flags;
    proof->device_b_flags = device_b_info.memory_property_flags;

    proof->range_status =
        vk_runtime_resident_buffer_upload(
            &session,
            &staging,
            &device_a,
            proof->final_input,
            byte_size + sizeof(uint32_t),
            WAIT_TIMEOUT_NS,
            &result);

    VkRuntimeComputeProgramConfig invalid_program;
    memset(&invalid_program, 0, sizeof(invalid_program));
    invalid_program.spirv_words = spirv->words;
    invalid_program.spirv_size = spirv->size;
    invalid_program.entry_point = "main";
    proof->binding_status =
        vk_runtime_compute_program_create(&session,
                                          &invalid_program,
                                          &forward,
                                          &result);

    VkRuntimeResidentBinding forward_bindings[2];
    VkRuntimeResidentBinding reverse_bindings[2];
    memset(forward_bindings, 0, sizeof(forward_bindings));
    memset(reverse_bindings, 0, sizeof(reverse_bindings));
    forward_bindings[0].buffer = &device_a;
    forward_bindings[0].range = byte_size;
    forward_bindings[1].buffer = &device_b;
    forward_bindings[1].range = byte_size;
    reverse_bindings[0].buffer = &device_b;
    reverse_bindings[0].range = byte_size;
    reverse_bindings[1].buffer = &device_a;
    reverse_bindings[1].range = byte_size;

    VkRuntimeComputeProgramConfig program_config;
    memset(&program_config, 0, sizeof(program_config));
    program_config.spirv_words = spirv->words;
    program_config.spirv_size = spirv->size;
    program_config.entry_point = "main";
    program_config.bindings = forward_bindings;
    program_config.binding_count = 2u;
    if (vk_runtime_compute_program_create(&session,
                                          &program_config,
                                          &forward,
                                          &result) !=
        VK_RUNTIME_STATUS_OK) {
        return false;
    }
    program_config.bindings = reverse_bindings;
    if (vk_runtime_compute_program_create(&session,
                                          &program_config,
                                          &reverse,
                                          &result) !=
        VK_RUNTIME_STATUS_OK) {
        return false;
    }

    proof->live_close_status =
        vk_runtime_compute_session_close(&session, &result);
    proof->referenced_buffer_status =
        vk_runtime_resident_buffer_destroy(&device_a, &result);

    VkRuntimeResidentDispatch dispatches[2];
    memset(dispatches, 0, sizeof(dispatches));
    dispatches[0].program = &forward;
    dispatches[1].program = &reverse;
    for (uint32_t i = 0u; i < 2u; ++i) {
        dispatches[i].group_count_x =
            (VALUE_COUNT + LOCAL_SIZE_X - 1u) / LOCAL_SIZE_X;
        dispatches[i].group_count_y = 1u;
        dispatches[i].group_count_z = 1u;
    }

    uint32_t input[VALUE_COUNT];
    uint32_t expected[VALUE_COUNT];
    uint32_t output[VALUE_COUNT];
    for (uint32_t iteration = 0u;
         iteration < CHAINS_PER_CYCLE;
         ++iteration) {
        fill_fixture(cycle, iteration, input, expected, output);
        if (vk_runtime_resident_buffer_upload(
                &session,
                &staging,
                &device_a,
                input,
                byte_size,
                WAIT_TIMEOUT_NS,
                &result) != VK_RUNTIME_STATUS_OK) {
            return false;
        }
        collect_operation_timing(proof, &result, 0u);
        if (vk_runtime_compute_session_dispatch(
                &session,
                dispatches,
                2u,
                WAIT_TIMEOUT_NS,
                &result) != VK_RUNTIME_STATUS_OK ||
            result.dispatch_count != 2u ||
            result.barrier_count != 1u) {
            return false;
        }
        collect_operation_timing(proof, &result, 1u);
        if (vk_runtime_resident_buffer_readback(
                &session,
                &device_a,
                &staging,
                output,
                byte_size,
                WAIT_TIMEOUT_NS,
                &result) != VK_RUNTIME_STATUS_OK) {
            return false;
        }
        collect_operation_timing(proof, &result, 2u);
        if (!exact_parity(expected, output)) {
            return false;
        }
        if (cycle + 1u == LIFECYCLE_CYCLES &&
            iteration + 1u == CHAINS_PER_CYCLE) {
            memcpy(proof->final_input, input, byte_size);
            memcpy(proof->final_expected, expected, byte_size);
            memcpy(proof->final_output, output, byte_size);
        }
    }

    if (cycle + 1u == LIFECYCLE_CYCLES) {
        fill_fixture(cycle, CHAINS_PER_CYCLE, input, expected, output);
        if (vk_runtime_resident_buffer_upload(
                &session,
                &staging,
                &device_a,
                input,
                byte_size,
                WAIT_TIMEOUT_NS,
                &result) != VK_RUNTIME_STATUS_OK) {
            return false;
        }
        collect_operation_timing(proof, &result, 0u);
        proof->timeout_status =
            vk_runtime_compute_session_dispatch(&session,
                                                dispatches,
                                                2u,
                                                0u,
                                                &result);
        collect_operation_timing(proof, &result, 1u);
        if (vk_runtime_compute_session_get_info(
                &session, &session_info) != VK_RUNTIME_STATUS_OK) {
            return false;
        }
        proof->timeout_in_flight_observed = session_info.in_flight;
        proof->timeout_close_status =
            vk_runtime_compute_session_close(&session, &result);
        proof->timeout_recovery_status =
            vk_runtime_compute_session_wait(&session,
                                            WAIT_TIMEOUT_NS,
                                            &result);
        proof->timeout_timestamp_valid =
            result.gpu_timestamp_supported &&
            result.gpu_timestamp_valid;
        collect_operation_timing(proof, &result, 1u);
        if (proof->timeout_recovery_status != VK_RUNTIME_STATUS_OK ||
            vk_runtime_resident_buffer_readback(
                &session,
                &device_a,
                &staging,
                output,
                byte_size,
                WAIT_TIMEOUT_NS,
                &result) != VK_RUNTIME_STATUS_OK ||
            !exact_parity(expected, output)) {
            return false;
        }
        collect_operation_timing(proof, &result, 2u);
        memcpy(proof->final_input, input, byte_size);
        memcpy(proof->final_expected, expected, byte_size);
        memcpy(proof->final_output, output, byte_size);
    }

    return destroy_cycle_resources(&session,
                                   &forward,
                                   &reverse,
                                   &staging,
                                   &device_a,
                                   &device_b,
                                   proof);
}

static void write_u32_array(FILE *file,
                            const char *name,
                            const uint32_t *values) {
    fprintf(file, "\"%s\":[", name);
    for (uint32_t i = 0u; i < VALUE_COUNT; ++i) {
        fprintf(file, "%s%u", i == 0u ? "" : ",", values[i]);
    }
    fputc(']', file);
}

static bool write_report(const Options *options,
                         const VkRuntime *runtime,
                         const ProofState *proof) {
    FILE *file = fopen(options->output_path, "wb");
    if (!file) {
        return false;
    }
    const VkRuntimeCapabilityReport *capability =
        vk_runtime_get_capability_report(runtime);
    const VkRuntimeDeviceCapability *device =
        capability->selected_device_index < capability->device_count
            ? &capability->devices[capability->selected_device_index]
            : NULL;
    fprintf(
        file,
        "{\n"
        "  \"schema\":\"codework_gpu_residency_report_v1\",\n"
        "  \"schema_version\":1,\n"
        "  \"module_version\":\"%s\",\n"
        "  \"runtime_status\":\"%s\",\n"
        "  \"validation\":{\"enabled\":%s,\"warnings\":%u,\"errors\":%u},\n"
        "  \"device_identity\":{\"uuid\":\"%s\",\"vendor_id\":%u,"
        "\"device_id\":%u,\"compute_queue_family\":%u},\n"
        "  \"shader_identity\":{\"u32_sha256\":\"%s\"},\n"
        "  \"memory\":{\"staging_flags\":%u,\"device_a_flags\":%u,"
        "\"device_b_flags\":%u,\"staging_host_visible_coherent\":%s,"
        "\"device_a_local\":%s,\"device_b_local\":%s},\n"
        "  \"contract\":{\"lifecycle_cycles\":%u,\"chains\":%llu,"
        "\"dispatches_per_chain\":2,\"barriers_per_chain\":1,"
        "\"readbacks_per_chain\":1,\"final_only_readback\":true,"
        "\"staging_reused\":true,\"device_buffers_reused\":true,"
        "\"descriptor_state_reused\":true,\"command_state_reused\":true},\n"
        "  \"negative_fixtures\":{\"null_session\":\"%s\","
        "\"double_session\":\"%s\","
        "\"buffer_range\":\"%s\",\"program_binding\":\"%s\","
        "\"referenced_buffer\":\"%s\",\"close_with_resources\":\"%s\"},\n"
        "  \"timeout_fixture\":{\"submit_status\":\"%s\","
        "\"in_flight_observed\":%s,\"close_status\":\"%s\","
        "\"recovery_status\":\"%s\"},\n"
        "  \"resource_accounting\":{\"submissions\":%llu,"
        "\"completed_submissions\":%llu,\"uploads\":%llu,"
        "\"readbacks\":%llu,\"dispatches\":%llu,\"barriers\":%llu,"
        "\"final_live_counts_zero\":%s},\n"
        "  \"u32\":{\"parity\":%s,\"value_count\":%u,"
        "\"operation\":\"twice(output=input*3u+7u)\",",
        vk_runtime_version_string(),
        vk_runtime_status_name(capability->status),
        capability->validation_enabled ? "true" : "false",
        capability->validation_warning_count,
        capability->validation_error_count,
        device ? device->device_uuid : "",
        device ? device->vendor_id : 0u,
        device ? device->device_id : 0u,
        proof->queue_family_index,
        options->spirv_sha256,
        (unsigned)proof->staging_flags,
        (unsigned)proof->device_a_flags,
        (unsigned)proof->device_b_flags,
        (proof->staging_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                (proof->staging_flags &
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            ? "true"
            : "false",
        (proof->device_a_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            ? "true"
            : "false",
        (proof->device_b_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            ? "true"
            : "false",
        LIFECYCLE_CYCLES,
        (unsigned long long)proof->barrier_count,
        vk_runtime_status_name(proof->null_session_status),
        vk_runtime_status_name(proof->double_session_status),
        vk_runtime_status_name(proof->range_status),
        vk_runtime_status_name(proof->binding_status),
        vk_runtime_status_name(proof->referenced_buffer_status),
        vk_runtime_status_name(proof->live_close_status),
        vk_runtime_status_name(proof->timeout_status),
        proof->timeout_in_flight_observed ? "true" : "false",
        vk_runtime_status_name(proof->timeout_close_status),
        vk_runtime_status_name(proof->timeout_recovery_status),
        (unsigned long long)proof->submission_count,
        (unsigned long long)proof->completed_submission_count,
        (unsigned long long)proof->upload_count,
        (unsigned long long)proof->readback_count,
        (unsigned long long)proof->dispatch_count,
        (unsigned long long)proof->barrier_count,
        proof->final_resource_counts_zero ? "true" : "false",
        proof->parity ? "true" : "false",
        VALUE_COUNT);
    write_u32_array(file, "input", proof->final_input);
    fputc(',', file);
    write_u32_array(file, "expected", proof->final_expected);
    fputc(',', file);
    write_u32_array(file, "output", proof->final_output);
    fprintf(
        file,
        "},\n"
        "  \"timing_ns\":{\"upload_host_copy\":%llu,"
        "\"upload_submit_wait\":%llu,\"dispatch_submit_wait\":%llu,"
        "\"readback_submit_wait\":%llu,\"readback_host_copy\":%llu}\n"
        "}\n",
        (unsigned long long)proof->upload_host_copy_ns,
        (unsigned long long)proof->upload_submit_wait_ns,
        (unsigned long long)proof->dispatch_submit_wait_ns,
        (unsigned long long)proof->readback_submit_wait_ns,
        (unsigned long long)proof->readback_host_copy_ns);
    return ferror(file) == 0 && fclose(file) == 0;
}

int main(int argc, char **argv) {
    Options options;
    if (!parse_options(argc, argv, &options)) {
        fprintf(stderr, "invalid resident-test options\n");
        return 2;
    }
    SpirvFile spirv;
    if (!read_spirv(options.spirv_path, &spirv)) {
        fprintf(stderr, "could not read resident-test SPIR-V\n");
        return 3;
    }

    VkRuntimeConfig config;
    vk_runtime_config_defaults(&config);
    config.application_name = "CodeWork vk_runtime S3 residency proof";
    config.enable_validation = options.require_validation;
    config.require_validation = options.require_validation;
    VkRuntime runtime;
    VkRuntimeStatus runtime_status =
        vk_runtime_initialize(&runtime, &config);

    ProofState proof;
    memset(&proof, 0, sizeof(proof));
    proof.final_resource_counts_zero = true;
    VkRuntimeComputeSession invalid_session;
    memset(&invalid_session, 0, sizeof(invalid_session));
    VkRuntimeResidentOperationResult result;
    proof.null_session_status =
        vk_runtime_compute_session_initialize(NULL,
                                              &invalid_session,
                                              &result);

    bool cycles_ok = runtime_status == VK_RUNTIME_STATUS_OK;
    for (uint32_t cycle = 0u;
         cycle < LIFECYCLE_CYCLES && cycles_ok;
         ++cycle) {
        cycles_ok = run_cycle(&runtime, &spirv, cycle, &proof);
    }
    proof.parity =
        cycles_ok &&
        exact_parity(proof.final_expected, proof.final_output);

    VkRuntimeStatus close_status = vk_runtime_close(&runtime);
    bool validation_clean =
        runtime.report.validation_warning_count == 0u &&
        runtime.report.validation_error_count == 0u;
    bool fixture_statuses =
        proof.null_session_status == VK_RUNTIME_STATUS_INVALID_ARGUMENT &&
        proof.double_session_status ==
            VK_RUNTIME_STATUS_INVALID_ARGUMENT &&
        proof.range_status == VK_RUNTIME_STATUS_BUFFER_RANGE_INVALID &&
        proof.binding_status ==
            VK_RUNTIME_STATUS_DESCRIPTOR_BINDING_INVALID &&
        proof.referenced_buffer_status ==
            VK_RUNTIME_STATUS_RESOURCE_IN_USE &&
        proof.live_close_status == VK_RUNTIME_STATUS_RESOURCE_IN_USE &&
        proof.timeout_status == VK_RUNTIME_STATUS_FENCE_WAIT_TIMEOUT &&
        proof.timeout_in_flight_observed &&
        proof.timeout_close_status ==
            VK_RUNTIME_STATUS_RESOURCE_IN_USE &&
        proof.timeout_recovery_status == VK_RUNTIME_STATUS_OK &&
        proof.timeout_timestamp_valid;
    bool memory_roles =
        (proof.staging_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u &&
        (proof.staging_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0u &&
        (proof.device_a_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u &&
        (proof.device_b_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u;
    bool accounting =
        proof.submission_count == 39u &&
        proof.completed_submission_count == 39u &&
        proof.upload_count == 13u &&
        proof.readback_count == 13u &&
        proof.dispatch_count == 26u &&
        proof.barrier_count == 13u &&
        proof.timestamp_measurement_count == 39u &&
        proof.final_resource_counts_zero;
    bool report_ok = write_report(&options, &runtime, &proof);
    vk_runtime_shutdown(&runtime);
    free(spirv.words);

    if (runtime_status != VK_RUNTIME_STATUS_OK ||
        close_status != VK_RUNTIME_STATUS_OK || !cycles_ok ||
        !proof.parity || !fixture_statuses || !memory_roles ||
        !accounting || !validation_clean || !report_ok) {
        fprintf(stderr,
                "S3 residency proof failed: runtime=%s close=%s "
                "cycles=%s parity=%s fixtures=%s memory=%s "
                "accounting=%s validation=%s report=%s\n",
                vk_runtime_status_name(runtime_status),
                vk_runtime_status_name(close_status),
                cycles_ok ? "ok" : "failed",
                proof.parity ? "ok" : "failed",
                fixture_statuses ? "ok" : "failed",
                memory_roles ? "ok" : "failed",
                accounting ? "ok" : "failed",
                validation_clean ? "clean" : "dirty",
                report_ok ? "ok" : "failed");
        return 1;
    }
    printf("vk_runtime S3 residency proof: ok\n");
    return 0;
}
