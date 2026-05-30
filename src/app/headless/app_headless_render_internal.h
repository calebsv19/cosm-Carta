#ifndef MAPFORGE_APP_HEADLESS_RENDER_INTERNAL_H
#define MAPFORGE_APP_HEADLESS_RENDER_INTERNAL_H

#include "app/app_headless.h"
#include "app/app_internal.h"

bool map_forge_headless_map_layers_init(AppState *app,
                                        const Renderer *renderer,
                                        const RegionInfo *region,
                                        int width,
                                        int height,
                                        const MapForgeHeadlessOutputConfig *output);
void map_forge_headless_map_layers_shutdown(AppState *app);
bool map_forge_headless_map_layers_prepare_frame(AppState *app,
                                                 const Renderer *renderer,
                                                 const Camera *camera,
                                                 const MapForgeHeadlessOutputConfig *output);
void map_forge_headless_map_layers_draw(AppState *app, AppVisibleTileRenderStats *out_stats);

#endif
