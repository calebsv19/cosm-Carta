#ifndef MAP_FORGE_MAP_FORGE_APP_MAIN_H
#define MAP_FORGE_MAP_FORGE_APP_MAIN_H

#include <stdbool.h>

// Canonical scaffold lifecycle wrappers for map_forge app startup.
bool map_forge_app_bootstrap(void);
bool map_forge_app_config_load(void);
bool map_forge_app_state_seed(void);
bool map_forge_app_subsystems_init(void);
bool map_forge_runtime_start(void);
int map_forge_app_run_loop(void);
void map_forge_app_shutdown(void);

// Canonical lifecycle-locked entrypoint for map_forge.
int map_forge_app_main(void);

#endif
