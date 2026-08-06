#include "vk_runtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VALUE_COUNT 257u
#define LOCAL_SIZE_X 64u
#define FLOAT_ABSOLUTE_TOLERANCE 1.0e-6f
#define FLOAT_RELATIVE_TOLERANCE 1.0e-6f

typedef struct Options {
    const char *u32_path;
    const char *f32_path;
    const char *u32_sha256;
    const char *f32_sha256;
    const char *output_path;
    bool require_validation;
} Options;

typedef struct SpirvFile {
    uint32_t *words;
    size_t size;
} SpirvFile;

static bool parse_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--u32") == 0 && i + 1 < argc) {
            options->u32_path = argv[++i];
        } else if (strcmp(argv[i], "--f32") == 0 && i + 1 < argc) {
            options->f32_path = argv[++i];
        } else if (strcmp(argv[i], "--u32-sha256") == 0 &&
                   i + 1 < argc) {
            options->u32_sha256 = argv[++i];
        } else if (strcmp(argv[i], "--f32-sha256") == 0 &&
                   i + 1 < argc) {
            options->f32_sha256 = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            options->output_path = argv[++i];
        } else if (strcmp(argv[i], "--require-validation") == 0) {
            options->require_validation = true;
        } else {
            return false;
        }
    }
    return options->u32_path && options->f32_path &&
           options->u32_sha256 && options->f32_sha256 &&
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

static VkRuntimeStatus run_dispatch(
    VkRuntime *runtime,
    const SpirvFile *spirv,
    const void *input,
    void *output,
    size_t size,
    VkRuntimeComputeResult *result) {
    unsigned char *initial_output =
        (unsigned char *)malloc(size);
    if (!initial_output) {
        return VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY;
    }
    memset(initial_output, 0xa5, size);

    VkRuntimeComputeBinding bindings[2];
    memset(bindings, 0, sizeof(bindings));
    bindings[0].upload_data = input;
    bindings[0].size = size;
    bindings[1].upload_data = initial_output;
    bindings[1].readback_data = output;
    bindings[1].size = size;
    bindings[1].readback = true;

    VkRuntimeComputeRequest request;
    memset(&request, 0, sizeof(request));
    request.spirv_words = spirv->words;
    request.spirv_size = spirv->size;
    request.entry_point = "main";
    request.bindings = bindings;
    request.binding_count = 2u;
    request.group_count_x =
        (VALUE_COUNT + LOCAL_SIZE_X - 1u) / LOCAL_SIZE_X;
    request.group_count_y = 1u;
    request.group_count_z = 1u;
    request.timeout_ns = 5000000000ull;

    VkRuntimeStatus status =
        vk_runtime_compute_dispatch(runtime, &request, result);
    free(initial_output);
    return status;
}

static bool run_negative_fixtures(VkRuntime *runtime,
                                  const SpirvFile *valid_spirv,
                                  VkRuntimeStatus *binding_status,
                                  VkRuntimeStatus *shader_status) {
    uint32_t value = 0u;
    VkRuntimeComputeBinding binding;
    memset(&binding, 0, sizeof(binding));
    binding.upload_data = &value;
    binding.size = sizeof(value);

    VkRuntimeComputeRequest request;
    memset(&request, 0, sizeof(request));
    request.spirv_words = valid_spirv->words;
    request.spirv_size = valid_spirv->size;
    request.entry_point = "main";
    request.bindings = &binding;
    request.binding_count = 0u;
    request.group_count_x = 1u;
    request.group_count_y = 1u;
    request.group_count_z = 1u;
    request.timeout_ns = 1u;

    VkRuntimeComputeResult result;
    *binding_status =
        vk_runtime_compute_dispatch(runtime, &request, &result);

    uint32_t invalid_spirv[5] = {0u, 0u, 0u, 0u, 0u};
    request.spirv_words = invalid_spirv;
    request.spirv_size = sizeof(invalid_spirv);
    request.binding_count = 1u;
    *shader_status =
        vk_runtime_compute_dispatch(runtime, &request, &result);
    return *binding_status ==
               VK_RUNTIME_STATUS_DESCRIPTOR_BINDING_INVALID &&
           *shader_status == VK_RUNTIME_STATUS_SHADER_CODE_INVALID;
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

static void write_f32_array(FILE *file,
                            const char *name,
                            const float *values) {
    fprintf(file, "\"%s\":[", name);
    for (uint32_t i = 0u; i < VALUE_COUNT; ++i) {
        fprintf(file,
                "%s%.9g",
                i == 0u ? "" : ",",
                (double)values[i]);
    }
    fputc(']', file);
}

static bool write_report(
    const Options *options,
    const VkRuntime *runtime,
    const uint32_t *u32_input,
    const uint32_t *u32_expected,
    const uint32_t *u32_output,
    const float *f32_input,
    const float *f32_expected,
    const float *f32_output,
    bool u32_parity,
    bool f32_parity,
    float max_absolute_error,
    float max_relative_error,
    VkRuntimeStatus binding_status,
    VkRuntimeStatus shader_status,
    const VkRuntimeComputeResult *u32_result,
    const VkRuntimeComputeResult *f32_result) {
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
    fprintf(file,
            "{\n"
            "  \"schema\":\"codework_gpu_compute_report_v1\",\n"
            "  \"schema_version\":1,\n"
            "  \"module_version\":\"%s\",\n"
            "  \"runtime_status\":\"%s\",\n"
            "  \"validation_enabled\":%s,\n"
            "  \"validation_warnings\":%u,\n"
            "  \"validation_errors\":%u,\n"
            "  \"device_identity\":{"
            "\"uuid\":\"%s\","
            "\"vendor_id\":%u,"
            "\"device_id\":%u},\n"
            "  \"shader_identity\":{"
            "\"u32_sha256\":\"%s\","
            "\"f32_sha256\":\"%s\"},\n"
            "  \"negative_fixtures\":{"
            "\"binding\":\"%s\","
            "\"shader\":\"%s\"},\n"
            "  \"u32\":{\"parity\":%s,",
            vk_runtime_version_string(),
            vk_runtime_status_name(capability->status),
            capability->validation_enabled ? "true" : "false",
            capability->validation_warning_count,
            capability->validation_error_count,
            device ? device->device_uuid : "",
            device ? device->vendor_id : 0u,
            device ? device->device_id : 0u,
            options->u32_sha256,
            options->f32_sha256,
            vk_runtime_status_name(binding_status),
            vk_runtime_status_name(shader_status),
            u32_parity ? "true" : "false");
    write_u32_array(file, "input", u32_input);
    fputc(',', file);
    write_u32_array(file, "expected", u32_expected);
    fputc(',', file);
    write_u32_array(file, "output", u32_output);
    fprintf(file,
            "},\n"
            "  \"f32\":{\"parity\":%s,"
            "\"policy\":{"
            "\"absolute_tolerance\":1e-6,"
            "\"relative_tolerance\":1e-6,"
            "\"exceptional_values\":\"finite_only\"},"
            "\"max_absolute_error\":%.9g,"
            "\"max_relative_error\":%.9g,",
            f32_parity ? "true" : "false",
            (double)max_absolute_error,
            (double)max_relative_error);
    write_f32_array(file, "input", f32_input);
    fputc(',', file);
    write_f32_array(file, "expected", f32_expected);
    fputc(',', file);
    write_f32_array(file, "output", f32_output);
    fprintf(file,
            "},\n"
            "  \"timing_ns\":{"
            "\"u32_host_upload\":%llu,"
            "\"u32_submit_wait\":%llu,"
            "\"u32_host_readback\":%llu,"
            "\"f32_host_upload\":%llu,"
            "\"f32_submit_wait\":%llu,"
            "\"f32_host_readback\":%llu}\n"
            "}\n",
            (unsigned long long)u32_result->host_upload_ns,
            (unsigned long long)u32_result->submit_wait_ns,
            (unsigned long long)u32_result->host_readback_ns,
            (unsigned long long)f32_result->host_upload_ns,
            (unsigned long long)f32_result->submit_wait_ns,
            (unsigned long long)f32_result->host_readback_ns);
    bool ok = ferror(file) == 0 && fclose(file) == 0;
    return ok;
}

int main(int argc, char **argv) {
    Options options;
    if (!parse_options(argc, argv, &options)) {
        fprintf(stderr, "invalid compute-test options\n");
        return 2;
    }

    SpirvFile u32_spirv;
    SpirvFile f32_spirv;
    memset(&u32_spirv, 0, sizeof(u32_spirv));
    memset(&f32_spirv, 0, sizeof(f32_spirv));
    if (!read_spirv(options.u32_path, &u32_spirv) ||
        !read_spirv(options.f32_path, &f32_spirv)) {
        fprintf(stderr, "could not read SPIR-V fixtures\n");
        free(u32_spirv.words);
        free(f32_spirv.words);
        return 3;
    }

    uint32_t u32_input[VALUE_COUNT];
    uint32_t u32_expected[VALUE_COUNT];
    uint32_t u32_output[VALUE_COUNT];
    float f32_input[VALUE_COUNT];
    float f32_expected[VALUE_COUNT];
    float f32_output[VALUE_COUNT];
    for (uint32_t i = 0u; i < VALUE_COUNT; ++i) {
        u32_input[i] = (i * 2654435761u) ^ 0xa5a5a5a5u;
        u32_expected[i] = u32_input[i] * 3u + 7u;
        u32_output[i] = 0u;
        f32_input[i] = ((float)((int)i - 128)) / 17.0f;
        f32_expected[i] = f32_input[i] * 1.25f + 0.5f;
        f32_output[i] = 0.0f;
    }

    VkRuntimeConfig config;
    vk_runtime_config_defaults(&config);
    config.application_name = "CodeWork vk_runtime S2 compute proof";
    config.enable_validation = options.require_validation;
    config.require_validation = options.require_validation;

    VkRuntime runtime;
    VkRuntimeStatus runtime_status =
        vk_runtime_initialize(&runtime, &config);
    VkRuntimeComputeResult u32_result;
    VkRuntimeComputeResult f32_result;
    memset(&u32_result, 0, sizeof(u32_result));
    memset(&f32_result, 0, sizeof(f32_result));

    VkRuntimeStatus u32_status = runtime_status;
    VkRuntimeStatus f32_status = runtime_status;
    VkRuntimeStatus binding_status = runtime_status;
    VkRuntimeStatus shader_status = runtime_status;
    bool negative_ok = false;
    if (runtime_status == VK_RUNTIME_STATUS_OK) {
        u32_status = run_dispatch(&runtime,
                                  &u32_spirv,
                                  u32_input,
                                  u32_output,
                                  sizeof(u32_input),
                                  &u32_result);
        f32_status = run_dispatch(&runtime,
                                  &f32_spirv,
                                  f32_input,
                                  f32_output,
                                  sizeof(f32_input),
                                  &f32_result);
        negative_ok = run_negative_fixtures(&runtime,
                                            &u32_spirv,
                                            &binding_status,
                                            &shader_status);
    }

    bool u32_parity = u32_status == VK_RUNTIME_STATUS_OK;
    for (uint32_t i = 0u; i < VALUE_COUNT && u32_parity; ++i) {
        u32_parity = u32_output[i] == u32_expected[i];
    }

    bool f32_parity = f32_status == VK_RUNTIME_STATUS_OK;
    float max_absolute_error = 0.0f;
    float max_relative_error = 0.0f;
    for (uint32_t i = 0u; i < VALUE_COUNT; ++i) {
        float absolute_error = fabsf(f32_output[i] - f32_expected[i]);
        float denominator = fabsf(f32_expected[i]);
        float relative_error =
            denominator > 0.0f ? absolute_error / denominator
                               : absolute_error;
        if (absolute_error > max_absolute_error) {
            max_absolute_error = absolute_error;
        }
        if (relative_error > max_relative_error) {
            max_relative_error = relative_error;
        }
        float allowed = FLOAT_ABSOLUTE_TOLERANCE +
                        FLOAT_RELATIVE_TOLERANCE * denominator;
        if (!isfinite(f32_output[i]) || absolute_error > allowed) {
            f32_parity = false;
        }
    }

    VkRuntimeStatus close_status = vk_runtime_close(&runtime);
    bool validation_clean =
        runtime.report.validation_warning_count == 0u &&
        runtime.report.validation_error_count == 0u;
    bool report_ok = write_report(&options,
                                  &runtime,
                                  u32_input,
                                  u32_expected,
                                  u32_output,
                                  f32_input,
                                  f32_expected,
                                  f32_output,
                                  u32_parity,
                                  f32_parity,
                                  max_absolute_error,
                                  max_relative_error,
                                  binding_status,
                                  shader_status,
                                  &u32_result,
                                  &f32_result);
    vk_runtime_shutdown(&runtime);
    free(u32_spirv.words);
    free(f32_spirv.words);

    if (runtime_status != VK_RUNTIME_STATUS_OK ||
        close_status != VK_RUNTIME_STATUS_OK ||
        !u32_parity || !f32_parity || !negative_ok ||
        !validation_clean || !report_ok) {
        fprintf(stderr,
                "compute proof failed: runtime=%s u32=%s f32=%s "
                "binding=%s shader=%s close=%s validation=%s report=%s\n",
                vk_runtime_status_name(runtime_status),
                vk_runtime_status_name(u32_status),
                vk_runtime_status_name(f32_status),
                vk_runtime_status_name(binding_status),
                vk_runtime_status_name(shader_status),
                vk_runtime_status_name(close_status),
                validation_clean ? "clean" : "dirty",
                report_ok ? "ok" : "failed");
        return 1;
    }
    printf("vk_runtime S2 compute proof: ok\n");
    return 0;
}
