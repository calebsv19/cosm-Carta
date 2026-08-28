#include <mach-o/dyld.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MAPFORGE_PACKAGE_PROFILE
#define MAPFORGE_PACKAGE_PROFILE "standard"
#endif
#ifndef MAPFORGE_RUNTIME_NAMESPACE
#define MAPFORGE_RUNTIME_NAMESPACE "Carta"
#endif
#ifndef MAPFORGE_LOG_NAMESPACE
#define MAPFORGE_LOG_NAMESPACE "Carta"
#endif
#ifndef MAPFORGE_BUILD_LABEL
#define MAPFORGE_BUILD_LABEL "Carta-local"
#endif

static int set_default_environment(const char *name, const char *value) {
    if (getenv(name) != NULL) {
        return 0;
    }
    return setenv(name, value, 1);
}

int main(int argc, char **argv) {
    if (set_default_environment("MAPFORGE_PACKAGE_PROFILE", MAPFORGE_PACKAGE_PROFILE) != 0 ||
        set_default_environment("MAPFORGE_RUNTIME_NAMESPACE", MAPFORGE_RUNTIME_NAMESPACE) != 0 ||
        set_default_environment("MAPFORGE_LOG_NAMESPACE", MAPFORGE_LOG_NAMESPACE) != 0 ||
        set_default_environment("MAPFORGE_BUILD_LABEL", MAPFORGE_BUILD_LABEL) != 0) {
        perror("Carta launcher: setenv");
        return 71;
    }

    char executable[PATH_MAX];
    uint32_t executable_size = (uint32_t)sizeof(executable);
    if (_NSGetExecutablePath(executable, &executable_size) != 0) {
        fputs("Carta launcher: executable path is too long\n", stderr);
        return 70;
    }

    const char marker[] = "/Contents/MacOS/";
    char *suffix = strstr(executable, marker);
    if (suffix == NULL) {
        fputs("Carta launcher: unexpected application layout\n", stderr);
        return 70;
    }

    char script[PATH_MAX];
    const size_t prefix_size = (size_t)(suffix - executable);
    const char resource[] = "/Contents/Resources/mapforge-launcher.sh";
    if (prefix_size + sizeof(resource) > sizeof(script)) {
        fputs("Carta launcher: resource path is too long\n", stderr);
        return 70;
    }
    memcpy(script, executable, prefix_size);
    memcpy(script + prefix_size, resource, sizeof(resource));

    char **shell_argv = calloc((size_t)argc + 2U, sizeof(*shell_argv));
    if (shell_argv == NULL) {
        fputs("Carta launcher: argument allocation failed\n", stderr);
        return 71;
    }
    shell_argv[0] = "/bin/sh";
    shell_argv[1] = script;
    for (int index = 1; index < argc; ++index) {
        shell_argv[index + 1] = argv[index];
    }
    execv(shell_argv[0], shell_argv);
    perror("Carta launcher: execv");
    free(shell_argv);
    return 71;
}
