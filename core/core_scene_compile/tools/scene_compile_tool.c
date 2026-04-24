#include "core_scene_compile.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    char diagnostics[512];
    CoreResult r;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <scene_authoring.json> <scene_runtime.json>\n",
                argv[0] ? argv[0] : "scene_compile_tool");
        return 2;
    }

    memset(diagnostics, 0, sizeof(diagnostics));
    r = core_scene_compile_authoring_file_to_runtime_file(argv[1], argv[2], diagnostics, sizeof(diagnostics));
    if (r.code != CORE_OK) {
        fprintf(stderr, "scene_compile_tool: compile failed: %s\n", diagnostics[0] ? diagnostics : "(no diagnostics)");
        return 1;
    }

    printf("scene_compile_tool: PASS authoring=%s runtime=%s\n", argv[1], argv[2]);
    return 0;
}
