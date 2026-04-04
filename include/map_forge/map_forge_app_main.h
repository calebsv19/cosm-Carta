#ifndef MAP_FORGE_MAP_FORGE_APP_MAIN_H
#define MAP_FORGE_MAP_FORGE_APP_MAIN_H

#include <stdbool.h>

typedef enum MapForgeAppStage {
    MAP_FORGE_APP_STAGE_INIT = 0,
    MAP_FORGE_APP_STAGE_BOOTSTRAPPED,
    MAP_FORGE_APP_STAGE_CONFIG_LOADED,
    MAP_FORGE_APP_STAGE_STATE_SEEDED,
    MAP_FORGE_APP_STAGE_SUBSYSTEMS_READY,
    MAP_FORGE_APP_STAGE_RUNTIME_STARTED,
    MAP_FORGE_APP_STAGE_LOOP_COMPLETED,
    MAP_FORGE_APP_STAGE_SHUTDOWN_COMPLETED
} MapForgeAppStage;

typedef struct MapForgeAppContext {
    int argc;
    char **argv;
    int exit_code;
    int dispatch_count;
    int dispatch_succeeded;
    int last_dispatch_exit_code;
    int wrapper_error;
    int legacy_delegate_invoked;
    int shutdown_executed;
    MapForgeAppStage stage;
} MapForgeAppContext;

// Canonical scaffold lifecycle wrappers for map_forge app startup.
bool map_forge_app_bootstrap(MapForgeAppContext *ctx);
bool map_forge_app_config_load(MapForgeAppContext *ctx);
bool map_forge_app_state_seed(MapForgeAppContext *ctx);
bool map_forge_app_subsystems_init(MapForgeAppContext *ctx);
bool map_forge_runtime_start(MapForgeAppContext *ctx);
int map_forge_app_run_loop(MapForgeAppContext *ctx);
void map_forge_app_shutdown(MapForgeAppContext *ctx);

// Canonical lifecycle-locked entrypoint for map_forge.
int map_forge_app_main(void);

#endif
