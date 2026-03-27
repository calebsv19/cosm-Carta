#include "map_forge/map_forge_app_main.h"

#include "app/app.h"

bool map_forge_app_bootstrap(void) {
    return true;
}

bool map_forge_app_config_load(void) {
    return true;
}

bool map_forge_app_state_seed(void) {
    return true;
}

bool map_forge_app_subsystems_init(void) {
    return true;
}

bool map_forge_runtime_start(void) {
    return true;
}

int map_forge_app_run_loop(void) {
    return app_run_legacy();
}

void map_forge_app_shutdown(void) {
}

int map_forge_app_main(void) {
    if (!map_forge_app_bootstrap()) {
        return 1;
    }
    if (!map_forge_app_config_load()) {
        map_forge_app_shutdown();
        return 1;
    }
    if (!map_forge_app_state_seed()) {
        map_forge_app_shutdown();
        return 1;
    }
    if (!map_forge_app_subsystems_init()) {
        map_forge_app_shutdown();
        return 1;
    }
    if (!map_forge_runtime_start()) {
        map_forge_app_shutdown();
        return 1;
    }

    const int status = map_forge_app_run_loop();
    map_forge_app_shutdown();
    return status;
}
