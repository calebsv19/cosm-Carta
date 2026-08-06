#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "vk_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOCAL_SIZE_X 64u
#define WORKLOAD_COUNT 7u
#define WARMUP_COUNT 2u
#define SAMPLE_COUNT 7u
#define WAIT_TIMEOUT_NS UINT64_C(5000000000)
#define MAX_VALUE_COUNT 1048576u
#define CPU_BATCH_MIN_VALUES 16777216u

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

typedef struct TimingMedians {
    uint64_t cpu_reference_ns;
    uint64_t upload_host_copy_ns;
    uint64_t upload_submit_wait_ns;
    uint64_t upload_gpu_ns;
    uint64_t execution_submit_wait_ns;
    uint64_t execution_gpu_ns;
    uint64_t download_submit_wait_ns;
    uint64_t download_gpu_ns;
    uint64_t readback_host_copy_ns;
    uint64_t transfer_gpu_ns;
    uint64_t total_wall_ns;
} TimingMedians;

typedef struct WorkloadResult {
    uint32_t value_count;
    uint32_t group_count_x;
    uint64_t checksum;
    bool parity;
    TimingMedians median;
} WorkloadResult;

typedef struct TimingSample {
    uint64_t upload_host_copy_ns;
    uint64_t upload_submit_wait_ns;
    uint64_t upload_gpu_ns;
    uint64_t execution_submit_wait_ns;
    uint64_t execution_gpu_ns;
    uint64_t download_submit_wait_ns;
    uint64_t download_gpu_ns;
    uint64_t readback_host_copy_ns;
    uint64_t total_wall_ns;
} TimingSample;

static const uint32_t k_workload_sizes[WORKLOAD_COUNT] = {
    256u,
    1024u,
    4096u,
    16384u,
    65536u,
    262144u,
    1048576u
};

static volatile uint64_t g_cpu_checksum_sink;

static uint64_t monotonic_ns(void) {
#if defined(_WIN32)
    return 0u;
#else
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        return 0u;
    }
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
           (uint64_t)value.tv_nsec;
#endif
}

static uint64_t elapsed_since(uint64_t start) {
    uint64_t end = monotonic_ns();
    return start != 0u && end >= start ? end - start : 0u;
}

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
    if (length <= 0 ||
        (size_t)length % sizeof(uint32_t) != 0u ||
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

static void fill_input(uint32_t value_count, uint32_t *input) {
    for (uint32_t i = 0u; i < value_count; ++i) {
        input[i] =
            (i * 2654435761u) ^ (value_count * 0x10001u);
    }
}

static uint64_t cpu_reference(uint32_t value_count,
                              const uint32_t *input,
                              uint32_t *expected) {
    uint64_t checksum = 0u;
    for (uint32_t i = 0u; i < value_count; ++i) {
        expected[i] = transform(transform(input[i]));
        checksum += expected[i];
    }
    return checksum;
}

static uint64_t measure_cpu_reference(
    uint32_t value_count,
    const uint32_t *input,
    uint32_t *expected,
    uint64_t *checksum_out) {
    uint32_t repetitions =
        (CPU_BATCH_MIN_VALUES + value_count - 1u) / value_count;
    uint64_t checksum = 0u;
    uint64_t start = monotonic_ns();
    for (uint32_t i = 0u; i < repetitions; ++i) {
        checksum = cpu_reference(value_count, input, expected);
        g_cpu_checksum_sink ^= checksum + i;
    }
    uint64_t elapsed = elapsed_since(start);
    *checksum_out = checksum;
    return elapsed / repetitions;
}

static bool exact_parity(uint32_t value_count,
                         const uint32_t *expected,
                         const uint32_t *output) {
    return memcmp(expected,
                  output,
                  (size_t)value_count * sizeof(uint32_t)) == 0;
}

static int compare_u64(const void *left, const void *right) {
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

static uint64_t median(uint64_t values[SAMPLE_COUNT]) {
    qsort(values, SAMPLE_COUNT, sizeof(values[0]), compare_u64);
    return values[SAMPLE_COUNT / 2u];
}

static bool run_gpu_sample(
    VkRuntimeComputeSession *session,
    VkRuntimeResidentBuffer *staging,
    VkRuntimeResidentBuffer *device_a,
    const VkRuntimeResidentDispatch dispatches[2],
    uint32_t value_count,
    const uint32_t *input,
    uint32_t *output,
    TimingSample *sample) {
    size_t byte_count = (size_t)value_count * sizeof(uint32_t);
    VkRuntimeResidentOperationResult result;
    uint64_t total_start = monotonic_ns();
    if (vk_runtime_resident_buffer_upload(
            session,
            staging,
            device_a,
            input,
            byte_count,
            WAIT_TIMEOUT_NS,
            &result) != VK_RUNTIME_STATUS_OK ||
        !result.gpu_timestamp_supported ||
        !result.gpu_timestamp_valid) {
        return false;
    }
    sample->upload_host_copy_ns = result.host_copy_ns;
    sample->upload_submit_wait_ns = result.submit_wait_ns;
    sample->upload_gpu_ns = result.gpu_elapsed_ns;

    VkRuntimeResidentDispatch sized_dispatches[2] = {
        dispatches[0],
        dispatches[1]
    };
    uint32_t group_count =
        (value_count + LOCAL_SIZE_X - 1u) / LOCAL_SIZE_X;
    sized_dispatches[0].group_count_x = group_count;
    sized_dispatches[1].group_count_x = group_count;
    if (vk_runtime_compute_session_dispatch(
            session,
            sized_dispatches,
            2u,
            WAIT_TIMEOUT_NS,
            &result) != VK_RUNTIME_STATUS_OK ||
        result.dispatch_count != 2u ||
        result.barrier_count != 1u ||
        !result.gpu_timestamp_valid) {
        return false;
    }
    sample->execution_submit_wait_ns = result.submit_wait_ns;
    sample->execution_gpu_ns = result.gpu_elapsed_ns;

    if (vk_runtime_resident_buffer_readback(
            session,
            device_a,
            staging,
            output,
            byte_count,
            WAIT_TIMEOUT_NS,
            &result) != VK_RUNTIME_STATUS_OK ||
        !result.gpu_timestamp_valid) {
        return false;
    }
    sample->download_submit_wait_ns = result.submit_wait_ns;
    sample->download_gpu_ns = result.gpu_elapsed_ns;
    sample->readback_host_copy_ns = result.host_copy_ns;
    sample->total_wall_ns = elapsed_since(total_start);
    return true;
}

static void collect_medians(
    const TimingSample samples[SAMPLE_COUNT],
    uint64_t cpu_samples[SAMPLE_COUNT],
    TimingMedians *output) {
    uint64_t values[SAMPLE_COUNT];
#define TAKE_MEDIAN(member)                                             \
    do {                                                                \
        for (uint32_t i = 0u; i < SAMPLE_COUNT; ++i) {                  \
            values[i] = samples[i].member;                              \
        }                                                               \
        output->member = median(values);                                \
    } while (0)
    memset(output, 0, sizeof(*output));
    output->cpu_reference_ns = median(cpu_samples);
    TAKE_MEDIAN(upload_host_copy_ns);
    TAKE_MEDIAN(upload_submit_wait_ns);
    TAKE_MEDIAN(upload_gpu_ns);
    TAKE_MEDIAN(execution_submit_wait_ns);
    TAKE_MEDIAN(execution_gpu_ns);
    TAKE_MEDIAN(download_submit_wait_ns);
    TAKE_MEDIAN(download_gpu_ns);
    TAKE_MEDIAN(readback_host_copy_ns);
    TAKE_MEDIAN(total_wall_ns);
#undef TAKE_MEDIAN
    output->transfer_gpu_ns =
        output->upload_gpu_ns + output->download_gpu_ns;
}

static bool measure_workload(
    VkRuntimeComputeSession *session,
    VkRuntimeResidentBuffer *staging,
    VkRuntimeResidentBuffer *device_a,
    const VkRuntimeResidentDispatch dispatches[2],
    uint32_t value_count,
    uint32_t *input,
    uint32_t *expected,
    uint32_t *output,
    WorkloadResult *workload) {
    fill_input(value_count, input);
    TimingSample warmup;
    memset(&warmup, 0, sizeof(warmup));
    for (uint32_t i = 0u; i < WARMUP_COUNT; ++i) {
        if (!run_gpu_sample(session,
                            staging,
                            device_a,
                            dispatches,
                            value_count,
                            input,
                            output,
                            &warmup)) {
            return false;
        }
    }

    uint64_t cpu_samples[SAMPLE_COUNT];
    uint64_t checksum = 0u;
    for (uint32_t i = 0u; i < SAMPLE_COUNT; ++i) {
        cpu_samples[i] =
            measure_cpu_reference(value_count,
                                  input,
                                  expected,
                                  &checksum);
    }

    TimingSample samples[SAMPLE_COUNT];
    memset(samples, 0, sizeof(samples));
    bool parity = true;
    for (uint32_t i = 0u; i < SAMPLE_COUNT; ++i) {
        memset(output,
               0,
               (size_t)value_count * sizeof(uint32_t));
        if (!run_gpu_sample(session,
                            staging,
                            device_a,
                            dispatches,
                            value_count,
                            input,
                            output,
                            &samples[i])) {
            return false;
        }
        parity =
            parity && exact_parity(value_count, expected, output);
    }
    workload->value_count = value_count;
    workload->group_count_x =
        (value_count + LOCAL_SIZE_X - 1u) / LOCAL_SIZE_X;
    workload->checksum = checksum;
    workload->parity = parity;
    collect_medians(samples, cpu_samples, &workload->median);
    return parity;
}

static bool destroy_resources_best_effort(
    VkRuntimeComputeSession *session,
    VkRuntimeComputeProgram *forward,
    VkRuntimeComputeProgram *reverse,
    VkRuntimeResidentBuffer *staging,
    VkRuntimeResidentBuffer *device_a,
    VkRuntimeResidentBuffer *device_b) {
    VkRuntimeResidentOperationResult result;
    bool ok = true;
    if (session->internal) {
        VkRuntimeComputeSessionInfo info;
        if (vk_runtime_compute_session_get_info(session, &info) ==
                VK_RUNTIME_STATUS_OK &&
            info.in_flight) {
            ok = vk_runtime_compute_session_wait(
                     session, WAIT_TIMEOUT_NS, &result) ==
                     VK_RUNTIME_STATUS_OK &&
                 ok;
        }
    }
    if (reverse->internal) {
        ok = vk_runtime_compute_program_destroy(reverse, &result) ==
                 VK_RUNTIME_STATUS_OK &&
             ok;
    }
    if (forward->internal) {
        ok = vk_runtime_compute_program_destroy(forward, &result) ==
                 VK_RUNTIME_STATUS_OK &&
             ok;
    }
    if (device_b->internal) {
        ok = vk_runtime_resident_buffer_destroy(device_b, &result) ==
                 VK_RUNTIME_STATUS_OK &&
             ok;
    }
    if (device_a->internal) {
        ok = vk_runtime_resident_buffer_destroy(device_a, &result) ==
                 VK_RUNTIME_STATUS_OK &&
             ok;
    }
    if (staging->internal) {
        ok = vk_runtime_resident_buffer_destroy(staging, &result) ==
                 VK_RUNTIME_STATUS_OK &&
             ok;
    }
    if (session->internal) {
        ok = vk_runtime_compute_session_close(session, &result) ==
                 VK_RUNTIME_STATUS_OK &&
             ok;
    }
    return ok;
}

static bool write_report(
    const Options *options,
    const VkRuntime *runtime,
    const VkRuntimeComputeSessionInfo *session_info,
    const WorkloadResult workloads[WORKLOAD_COUNT]) {
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
    uint32_t execution_crossover = 0u;
    uint32_t end_to_end_crossover = 0u;
    for (uint32_t i = 0u; i < WORKLOAD_COUNT; ++i) {
        if (execution_crossover == 0u &&
            workloads[i].median.execution_gpu_ns <
                workloads[i].median.cpu_reference_ns) {
            execution_crossover = workloads[i].value_count;
        }
        if (end_to_end_crossover == 0u &&
            workloads[i].median.total_wall_ns <
                workloads[i].median.cpu_reference_ns) {
            end_to_end_crossover = workloads[i].value_count;
        }
    }
    fprintf(
        file,
        "{\n"
        "  \"schema\":\"codework_gpu_timing_report_v1\",\n"
        "  \"schema_version\":1,\n"
        "  \"module_version\":\"%s\",\n"
        "  \"runtime_status\":\"%s\",\n"
        "  \"validation\":{\"enabled\":%s,\"warnings\":%u,\"errors\":%u},\n"
        "  \"platform\":{\"os\":\"%s\",\"architecture\":\"%s\","
        "\"device_name\":\"%s\",\"device_uuid\":\"%s\","
        "\"vendor_id\":%u,\"device_id\":%u,\"compute_queue_family\":%u},\n"
        "  \"shader_identity\":{\"u32_sha256\":\"%s\"},\n"
        "  \"timestamp\":{\"supported\":%s,\"valid_bits\":%u,"
        "\"period_ns\":%.9g,\"measurement_count\":%llu},\n"
        "  \"methodology\":{\"warmups\":%u,\"samples\":%u,"
        "\"aggregation\":\"median\",\"cpu_batch_min_values\":%u,"
        "\"dispatches_per_sample\":2,"
        "\"barriers_per_sample\":1,\"final_readbacks_per_sample\":1,"
        "\"gpu_timestamps_bracket_queue_commands\":true,"
        "\"submit_wait_includes_fence_wait\":true,"
        "\"cpu_operation\":\"twice(output=input*3u+7u)\"},\n"
        "  \"workloads\":[\n",
        vk_runtime_version_string(),
        vk_runtime_status_name(capability->status),
        capability->validation_enabled ? "true" : "false",
        capability->validation_warning_count,
        capability->validation_error_count,
        capability->platform,
        capability->architecture,
        device ? device->device_name : "",
        device ? device->device_uuid : "",
        device ? device->vendor_id : 0u,
        device ? device->device_id : 0u,
        session_info->queue_family_index,
        options->spirv_sha256,
        session_info->timestamp_supported ? "true" : "false",
        session_info->timestamp_valid_bits,
        session_info->timestamp_period_ns,
        (unsigned long long)session_info->timestamp_measurement_count,
        WARMUP_COUNT,
        SAMPLE_COUNT,
        CPU_BATCH_MIN_VALUES);
    for (uint32_t i = 0u; i < WORKLOAD_COUNT; ++i) {
        const WorkloadResult *w = &workloads[i];
        const TimingMedians *m = &w->median;
        fprintf(
            file,
            "    %s{\"value_count\":%u,\"byte_count\":%llu,"
            "\"group_count_x\":%u,\"parity\":%s,\"checksum\":%llu,"
            "\"median_ns\":{\"cpu_reference\":%llu,"
            "\"upload_host_copy\":%llu,\"upload_submit_wait\":%llu,"
            "\"upload_gpu\":%llu,\"execution_submit_wait\":%llu,"
            "\"execution_gpu\":%llu,\"download_submit_wait\":%llu,"
            "\"download_gpu\":%llu,\"readback_host_copy\":%llu,"
            "\"transfer_gpu\":%llu,\"total_wall\":%llu}}\n",
            i == 0u ? "" : ",",
            w->value_count,
            (unsigned long long)w->value_count * sizeof(uint32_t),
            w->group_count_x,
            w->parity ? "true" : "false",
            (unsigned long long)w->checksum,
            (unsigned long long)m->cpu_reference_ns,
            (unsigned long long)m->upload_host_copy_ns,
            (unsigned long long)m->upload_submit_wait_ns,
            (unsigned long long)m->upload_gpu_ns,
            (unsigned long long)m->execution_submit_wait_ns,
            (unsigned long long)m->execution_gpu_ns,
            (unsigned long long)m->download_submit_wait_ns,
            (unsigned long long)m->download_gpu_ns,
            (unsigned long long)m->readback_host_copy_ns,
            (unsigned long long)m->transfer_gpu_ns,
            (unsigned long long)m->total_wall_ns);
    }
    fprintf(
        file,
        "  ],\n"
        "  \"crossover\":{\"execution_only\":{\"observed\":%s,"
        "\"first_value_count\":",
        execution_crossover != 0u ? "true" : "false");
    if (execution_crossover != 0u) {
        fprintf(file, "%u", execution_crossover);
    } else {
        fputs("null", file);
    }
    fprintf(
        file,
        "},\"end_to_end\":{\"observed\":%s,\"first_value_count\":",
        end_to_end_crossover != 0u ? "true" : "false");
    if (end_to_end_crossover != 0u) {
        fprintf(file, "%u", end_to_end_crossover);
    } else {
        fputs("null", file);
    }
    fputs("}}\n}\n", file);
    return ferror(file) == 0 && fclose(file) == 0;
}

int main(int argc, char **argv) {
    Options options;
    if (!parse_options(argc, argv, &options)) {
        fprintf(stderr, "invalid timing-test options\n");
        return 2;
    }
    SpirvFile spirv;
    if (!read_spirv(options.spirv_path, &spirv)) {
        fprintf(stderr, "could not read timing-test SPIR-V\n");
        return 3;
    }
    uint32_t *input =
        (uint32_t *)malloc((size_t)MAX_VALUE_COUNT * sizeof(uint32_t));
    uint32_t *expected =
        (uint32_t *)malloc((size_t)MAX_VALUE_COUNT * sizeof(uint32_t));
    uint32_t *output =
        (uint32_t *)malloc((size_t)MAX_VALUE_COUNT * sizeof(uint32_t));
    if (!input || !expected || !output) {
        free(input);
        free(expected);
        free(output);
        free(spirv.words);
        return 4;
    }

    VkRuntimeConfig config;
    vk_runtime_config_defaults(&config);
    config.application_name = "CodeWork vk_runtime S4 timing proof";
    config.enable_validation = options.require_validation;
    config.require_validation = options.require_validation;
    VkRuntime runtime;
    VkRuntimeStatus runtime_status =
        vk_runtime_initialize(&runtime, &config);

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
    VkRuntimeResidentOperationResult operation;
    bool ok =
        runtime_status == VK_RUNTIME_STATUS_OK &&
        vk_runtime_compute_session_initialize(
            &runtime, &session, &operation) == VK_RUNTIME_STATUS_OK;

    size_t max_bytes =
        (size_t)MAX_VALUE_COUNT * sizeof(uint32_t);
    if (ok) {
        ok =
            vk_runtime_resident_buffer_create(
                &session,
                VK_RUNTIME_BUFFER_ROLE_HOST_STAGING,
                max_bytes,
                &staging,
                &operation) == VK_RUNTIME_STATUS_OK &&
            vk_runtime_resident_buffer_create(
                &session,
                VK_RUNTIME_BUFFER_ROLE_DEVICE_LOCAL,
                max_bytes,
                &device_a,
                &operation) == VK_RUNTIME_STATUS_OK &&
            vk_runtime_resident_buffer_create(
                &session,
                VK_RUNTIME_BUFFER_ROLE_DEVICE_LOCAL,
                max_bytes,
                &device_b,
                &operation) == VK_RUNTIME_STATUS_OK;
    }
    VkRuntimeResidentBinding forward_bindings[2] = {
        {&device_a, 0u, max_bytes},
        {&device_b, 0u, max_bytes}
    };
    VkRuntimeResidentBinding reverse_bindings[2] = {
        {&device_b, 0u, max_bytes},
        {&device_a, 0u, max_bytes}
    };
    VkRuntimeComputeProgramConfig program_config;
    memset(&program_config, 0, sizeof(program_config));
    program_config.spirv_words = spirv.words;
    program_config.spirv_size = spirv.size;
    program_config.entry_point = "main";
    program_config.binding_count = 2u;
    if (ok) {
        program_config.bindings = forward_bindings;
        ok = vk_runtime_compute_program_create(
                 &session,
                 &program_config,
                 &forward,
                 &operation) == VK_RUNTIME_STATUS_OK;
    }
    if (ok) {
        program_config.bindings = reverse_bindings;
        ok = vk_runtime_compute_program_create(
                 &session,
                 &program_config,
                 &reverse,
                 &operation) == VK_RUNTIME_STATUS_OK;
    }
    VkRuntimeResidentDispatch dispatches[2];
    memset(dispatches, 0, sizeof(dispatches));
    dispatches[0].program = &forward;
    dispatches[0].group_count_y = 1u;
    dispatches[0].group_count_z = 1u;
    dispatches[1].program = &reverse;
    dispatches[1].group_count_y = 1u;
    dispatches[1].group_count_z = 1u;

    VkRuntimeComputeSessionInfo initial_info;
    memset(&initial_info, 0, sizeof(initial_info));
    if (ok) {
        ok = vk_runtime_compute_session_get_info(
                 &session, &initial_info) == VK_RUNTIME_STATUS_OK &&
             initial_info.timestamp_supported &&
             initial_info.timestamp_valid_bits > 0u &&
             initial_info.timestamp_period_ns > 0.0;
    }

    WorkloadResult workloads[WORKLOAD_COUNT];
    memset(workloads, 0, sizeof(workloads));
    for (uint32_t i = 0u; i < WORKLOAD_COUNT && ok; ++i) {
        ok = measure_workload(&session,
                              &staging,
                              &device_a,
                              dispatches,
                              k_workload_sizes[i],
                              input,
                              expected,
                              output,
                              &workloads[i]);
    }

    VkRuntimeComputeSessionInfo final_info;
    memset(&final_info, 0, sizeof(final_info));
    if (ok) {
        ok = vk_runtime_compute_session_get_info(
                 &session, &final_info) == VK_RUNTIME_STATUS_OK &&
             final_info.submission_count == 189u &&
             final_info.completed_submission_count == 189u &&
             final_info.upload_count == 63u &&
             final_info.readback_count == 63u &&
             final_info.dispatch_count == 126u &&
             final_info.barrier_count == 63u &&
             final_info.timestamp_measurement_count == 189u &&
             !final_info.in_flight;
    }
    bool report_ok =
        ok && write_report(&options, &runtime, &final_info, workloads);
    bool resources_ok =
        destroy_resources_best_effort(&session,
                                      &forward,
                                      &reverse,
                                      &staging,
                                      &device_a,
                                      &device_b);
    VkRuntimeStatus close_status = vk_runtime_close(&runtime);
    bool validation_clean =
        runtime.report.validation_warning_count == 0u &&
        runtime.report.validation_error_count == 0u;
    vk_runtime_shutdown(&runtime);
    free(input);
    free(expected);
    free(output);
    free(spirv.words);

    if (!ok || !report_ok || !resources_ok ||
        close_status != VK_RUNTIME_STATUS_OK || !validation_clean) {
        fprintf(stderr,
                "S4 timing proof failed: runtime=%s measurement=%s "
                "report=%s resources=%s close=%s validation=%s\n",
                vk_runtime_status_name(runtime_status),
                ok ? "ok" : "failed",
                report_ok ? "ok" : "failed",
                resources_ok ? "ok" : "failed",
                vk_runtime_status_name(close_status),
                validation_clean ? "clean" : "dirty");
        return 1;
    }
    printf("vk_runtime S4 timing proof: ok\n");
    return 0;
}
