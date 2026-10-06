#ifndef MAPFORGE_APP_TERRAIN_H
#define MAPFORGE_APP_TERRAIN_H
#include <stdbool.h>
#include "map/terrain_catalog.h"
struct AppState;
// Reports when the hiking workspace owns the continuous terrain basemap.
bool terrain_active(struct AppState *app);
// Reports actual background readiness and residency for native transition diagnostics.
typedef struct TerrainPresentationStats {
    int area, coarse_ready, coarse_needed, detail_ready, detail_needed;
    size_t resident_tiles, rgba_bytes, gpu_bytes, worker_slots;
    bool pending;
} TerrainPresentationStats;
// Reads the last draw's bounded coverage and resource counters without mutating terrain.
TerrainPresentationStats terrain_presentation_stats(void);
// Draws the offline overview under hiking tracks with bounded texture residency.
void terrain_draw(struct AppState *app);
// Draws explicit coverage, source and preparation status in the hiking viewport.
void terrain_credit_draw(struct AppState *app);
// Cycles Topo, Relief and Off without changing history records.
void terrain_cycle(void);
// Returns the current stable style label.
const char *terrain_style_label(void);
// Reloads only a completely validated replacement pack.
bool terrain_reload(struct AppState *app);
// Releases pack and texture resources before renderer shutdown.
void terrain_shutdown(struct AppState *app);
// Returns or restores the persisted overview preference.
int terrain_style(void);
void terrain_set_style(int style);
// Returns the active area's effective mode without rewriting the user's baseline preference.
int terrain_render_mode(void);
// Exposes the bounded read-only area catalog for coverage/storage UI.
const TerrainCatalog *terrain_area_catalog(void);
// Provides the active vector path; no unrelated sidecar is opened outside coverage.
const char *terrain_vector_path(void);
// Reports selected-route background coverage separately from route/status persistence.
void terrain_coverage_text(double min_lat,double min_lon,double max_lat,double max_lon,char *out,size_t cap);
// Tests all installed area extents without loading another raster/vector pack.
bool terrain_covers(double min_lat,double min_lon,double max_lat,double max_lon);
// Retires a confirmed additional background while protecting baseline packs and personal records.
bool terrain_remove_area(struct AppState *app,size_t index,char *error,size_t cap);
#endif
