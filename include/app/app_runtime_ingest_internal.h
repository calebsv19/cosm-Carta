#ifndef MAPFORGE_APP_RUNTIME_INGEST_INTERNAL_H
#define MAPFORGE_APP_RUNTIME_INGEST_INTERNAL_H

#include "app/app_internal.h"

void app_draw_ingest_panel(AppState *app);
void app_ingest_rescan_sources(AppState *app);
void app_ingest_rescan_active_regions(AppState *app);
bool app_ingest_open_selected_active_region(AppState *app);
void app_reload_pins_state(AppState *app);
void app_runtime_format_region_package_status(const char *region_name,
                                              const RegionPackageValidationResult *validation,
                                              char *out_status,
                                              size_t out_size);
bool app_runtime_ingest_tick(AppState *app);
bool app_runtime_cycle_next_region(AppState *app);

#endif
