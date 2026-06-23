#include "app/app_headless.h"
#include "app_headless_util.h"

#include <stdio.h>
#include <string.h>

bool map_forge_headless_args_parse(int argc,
                                   char **argv,
                                   MapForgeHeadlessCliOptions *out_options,
                                   char *out_error,
                                   size_t out_error_size) {
    MapForgeHeadlessCliOptions options = {0};
    if (!out_options) {
        return map_forge_headless_fail(out_error, out_error_size, "missing output options");
    }

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!arg) {
            continue;
        }
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            options.show_help = true;
            continue;
        }
        if (strcmp(arg, "--headless") == 0) {
            options.headless = true;
            continue;
        }
        if (strcmp(arg, "--job") == 0) {
            if (i + 1 >= argc) {
                return map_forge_headless_fail(out_error, out_error_size, "--job requires a path");
            }
            options.job_path = argv[++i];
            continue;
        }
        if (strcmp(arg, "--out") == 0) {
            if (i + 1 >= argc) {
                return map_forge_headless_fail(out_error, out_error_size, "--out requires a path");
            }
            options.out_dir = argv[++i];
            continue;
        }
        if (options.headless || options.show_help) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "unknown argument: %s", arg);
            return map_forge_headless_fail(out_error, out_error_size, buffer);
        }
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "interactive mode does not accept argument: %s", arg);
        return map_forge_headless_fail(out_error, out_error_size, buffer);
    }

    if (options.headless) {
        if (!options.job_path || options.job_path[0] == '\0') {
            return map_forge_headless_fail(out_error, out_error_size, "headless mode requires --job <path>");
        }
        if (!options.out_dir || options.out_dir[0] == '\0') {
            return map_forge_headless_fail(out_error, out_error_size, "headless mode requires --out <dir>");
        }
    }

    *out_options = options;
    return true;
}

void map_forge_headless_args_usage(const char *program_name,
                                   char *out_text,
                                   size_t out_text_size) {
    const char *name = (program_name && program_name[0] != '\0') ? program_name : "mapforge";
    if (!out_text || out_text_size == 0u) {
        return;
    }
    snprintf(out_text,
             out_text_size,
             "Usage:\n"
             "  %s\n"
             "  %s --headless --job <job.json|bundle.json> --out <run_dir>\n"
             "  %s --help\n",
             name,
             name,
             name);
}
