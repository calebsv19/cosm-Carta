#ifndef MAPFORGE_APP_APP_H
#define MAPFORGE_APP_APP_H

// Legacy runtime entry implementation retained during scaffold migration.
// Prefer calling map_forge_app_main() for lifecycle-locked startup flow.
int app_run_legacy(void);

// Public app entry; delegates to map_forge_app_main().
int app_run(void);

#endif
