#include "vk_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ProbeOptions {
    bool pretty;
    bool fail_on_validation_error;
    const char *output_path;
    VkRuntimeConfig runtime;
} ProbeOptions;

static void print_usage(const char *program) {
    fprintf(stderr,
            "usage: %s [options]\n"
            "  --validate                 request validation when available\n"
            "  --require-validation       fail when validation is unavailable\n"
            "  --graphics                 require a graphics queue\n"
            "  --no-compute               do not require a compute queue\n"
            "  --transfer                 require a transfer queue\n"
            "  --no-device                discover only; skip logical device creation\n"
            "  --pretty                   pretty-print JSON\n"
            "  --output <path>            write JSON to a file instead of stdout\n"
            "  --fail-on-validation-error return failure if validation reports errors\n",
            program);
}

static bool parse_options(int argc,
                          char **argv,
                          ProbeOptions *options) {
    memset(options, 0, sizeof(*options));
    vk_runtime_config_defaults(&options->runtime);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--validate") == 0) {
            options->runtime.enable_validation = true;
        } else if (strcmp(argv[i], "--require-validation") == 0) {
            options->runtime.enable_validation = true;
            options->runtime.require_validation = true;
        } else if (strcmp(argv[i], "--graphics") == 0) {
            options->runtime.require_graphics_queue = true;
        } else if (strcmp(argv[i], "--no-compute") == 0) {
            options->runtime.require_compute_queue = false;
        } else if (strcmp(argv[i], "--transfer") == 0) {
            options->runtime.require_transfer_queue = true;
        } else if (strcmp(argv[i], "--no-device") == 0) {
            options->runtime.create_logical_device = false;
        } else if (strcmp(argv[i], "--pretty") == 0) {
            options->pretty = true;
        } else if (strcmp(argv[i], "--fail-on-validation-error") == 0) {
            options->fail_on_validation_error = true;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                return false;
            }
            options->output_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv) {
    ProbeOptions options;
    if (!parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return 2;
    }

    VkRuntime runtime;
    VkRuntimeStatus initialize_status =
        vk_runtime_initialize(&runtime, &options.runtime);
    VkRuntimeStatus close_status = vk_runtime_close(&runtime);
    const VkRuntimeCapabilityReport *report =
        vk_runtime_get_capability_report(&runtime);

    size_t json_size = 0u;
    VkRuntimeStatus serialization_status =
        vk_runtime_capability_report_to_json(
            report, options.pretty, NULL, 0u, &json_size);
    if (serialization_status != VK_RUNTIME_STATUS_OK ||
        json_size == 0u) {
        fprintf(stderr,
                "capability report size failed: %s\n",
                vk_runtime_status_name(serialization_status));
        vk_runtime_shutdown(&runtime);
        return 3;
    }

    char *json = (char *)malloc(json_size);
    if (!json) {
        fprintf(stderr, "capability report allocation failed\n");
        vk_runtime_shutdown(&runtime);
        return 3;
    }

    serialization_status = vk_runtime_capability_report_to_json(
        report, options.pretty, json, json_size, &json_size);
    if (serialization_status != VK_RUNTIME_STATUS_OK) {
        fprintf(stderr,
                "capability report serialization failed: %s\n",
                vk_runtime_status_name(serialization_status));
        free(json);
        vk_runtime_shutdown(&runtime);
        return 3;
    }

    FILE *output = stdout;
    if (options.output_path) {
        output = fopen(options.output_path, "wb");
        if (!output) {
            fprintf(stderr,
                    "could not open output: %s\n",
                    options.output_path);
            free(json);
            vk_runtime_shutdown(&runtime);
            return 4;
        }
    }
    size_t json_length = strlen(json);
    bool write_ok = fwrite(json, 1u, json_length, output) == json_length;
    if (output != stdout) {
        write_ok = fclose(output) == 0 && write_ok;
    }
    free(json);

    bool validation_failed =
        options.fail_on_validation_error &&
        report->validation_error_count > 0u;
    bool runtime_failed =
        initialize_status != VK_RUNTIME_STATUS_OK ||
        close_status != VK_RUNTIME_STATUS_OK;
    vk_runtime_shutdown(&runtime);

    if (!write_ok) {
        fprintf(stderr, "capability report write failed\n");
        return 4;
    }
    if (validation_failed) {
        fprintf(stderr, "validation reported one or more errors\n");
        return 5;
    }
    if (runtime_failed) {
        fprintf(stderr,
                "vk_runtime lifecycle failed: initialize=%s close=%s\n",
                vk_runtime_status_name(initialize_status),
                vk_runtime_status_name(close_status));
        return 1;
    }
    return 0;
}
