#define _POSIX_C_SOURCE 200809L
#include "app/region_prepare.h"
#include "app/app_region_transition.h"
#include "app/app_ui_internal.h"
#include "ui/font.h"
#include "ui/control_surface.h"
#include "tile_source_test_fixture.h"
#include "map/mercator.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// Replaces only presentation/active-map seams; CPU validation and staging remain production code.
static unsigned publications, retired_loaders;
static bool dialog_active, refuse_upload;
static unsigned cancel_draws;
void carta_ui_control_draw(AppState *app, SDL_FRect rect, const char *label,
                          bool selected, KitUiSurfaceKey key, float scale) {
    (void)selected; (void)scale;
    assert(key.domain==CARTA_CONTROL_PREPARE && key.value==0);
    assert(!strcmp(label,"CANCEL (ESC)") && rect.x+rect.w<=app->width);
    ++cancel_draws;
}
bool app_place_tools_active(void) { return dialog_active; }
void app_reload_pins_state(AppState *app) { (void)app; publications++; }
void app_runtime_format_region_package_status(const char *name, const RegionPackageValidationResult *v, char *out, size_t cap) {
    (void)v; snprintf(out,cap,"%s ready",name);
}
uint32_t app_worker_contract_bump_world_generation(AppState *app) { (void)app; return 1; }
uint32_t app_worker_contract_bump_tile_generation(AppState *app) { (void)app; return 1; }
void app_clear_tile_queue(AppState *app) { (void)app; }
void app_tile_viewport_invalidate(AppState *app) { (void)app; }
void app_tile_lifecycle_transition(AppState *app, TileLayerKind k, TileCoord c, TileZoomBand b, AppTileLifecycleState state, bool cpu, bool gpu, bool fb, bool ideal) {
    (void)app; (void)k; (void)c; (void)b; (void)state; (void)cpu; (void)gpu; (void)fb; (void)ideal;
}
bool app_load_route_graph(AppState *app) { (void)app; return true; }
void app_playback_reset(AppState *app) { (void)app; }
void tile_loader_shutdown(TileLoader *loader) { (void)loader; retired_loaders++; }
bool tile_loader_init_with_source(TileLoader *loader, const TileSourceConfig *source) { (void)loader; (void)source; return true; }
bool vk_tile_cache_init(VkTileCache *cache, uint32_t cap) {
    memset(cache,0,sizeof(*cache)); cache->capacity=cap; cache->byte_limit=512ull*1024*1024;
    cache->entries=calloc(cap,sizeof(*cache->entries)); return cache->entries!=NULL;
}
void vk_tile_cache_shutdown(VkTileCache *cache) { free(cache->entries); memset(cache,0,sizeof(*cache)); }
void vk_tile_cache_clear_with_renderer(VkTileCache *cache, void *renderer) { (void)cache; (void)renderer; }
bool vk_tile_cache_on_tile_loaded(VkTileCache *cache, void *r, TileLayerKind k, TileCoord c, TileZoomBand b, const MftTile *t) {
    (void)cache; (void)r; (void)k; (void)c; (void)b; (void)t; return !refuse_upload;
}
const VkTileCacheEntry *vk_tile_cache_peek(VkTileCache *cache, TileLayerKind k, TileCoord c, TileZoomBand b) {
    (void)cache; (void)k; (void)c; (void)b; return NULL;
}
MapForgeThemePalette app_ui_theme_palette(void) { return (MapForgeThemePalette){0}; }
void renderer_set_draw_color(Renderer *r, uint8_t a, uint8_t b, uint8_t c, uint8_t d) { (void)r;(void)a;(void)b;(void)c;(void)d; }
void renderer_fill_rect(Renderer *r, const SDL_FRect *rect) { (void)r;(void)rect; }
void renderer_draw_rect(Renderer *r, const SDL_FRect *rect) { (void)r;(void)rect; }
void ui_draw_text_clipped(Renderer *r,int x,int y,const char *text,SDL_Color c,float scale,int width) {
    (void)r;(void)x;(void)y;(void)text;(void)c;(void)scale;(void)width;
}

// Waits for a bounded worker result while observing synchronized progress.
static RegionPrepareProgress wait_result(RegionPrepare *p) {
    struct timespec pause = {0, 1000000};
    for (unsigned i=0;i<5000;i++) {
        RegionPrepareProgress progress = region_prepare_progress(p);
        if (progress.done) return progress;
        nanosleep(&pause, NULL);
    }
    assert(!"background preparation timeout");
    return (RegionPrepareProgress){0};
}

// Exercises actual metadata/source/decode ownership in an isolated tiny map pack.
int main(void) {
    MapForgeTileSourceFixture fixture;
    assert(mapforge_tile_source_fixture_init(&fixture));
    assert(setenv("MAPFORGE_REGIONS_DIR", fixture.root, 1)==0);
    char dir[512], meta[512], tiles[512];
    snprintf(dir, sizeof(dir), "%s/candidate", fixture.root);
    assert(mkdir(dir,0700)==0);
    snprintf(meta, sizeof(meta), "%s/meta.json", dir);
    FILE *f = fopen(meta,"w"); assert(f);
    fputs("{\"bounds\":{\"min_lat\":47.6599,\"max_lat\":47.6601,\"min_lon\":-122.3001,\"max_lon\":-122.2999},\"tile\":{\"min_z\":14,\"max_z\":14,\"extent\":4096},\"tile_store\":{\"kind\":\"filesystem_tree\",\"root\":\"tiles\",\"runtime_source_policy\":\"filesystem_only\"}}",f);
    fclose(f);
    snprintf(tiles, sizeof(tiles), "%s/tiles", dir);
    TileCoord coord = tile_from_meters(14, mercator_from_latlon((LatLon){47.66,-122.30}));
    assert(mapforge_tile_source_fixture_write_tree_tile(tiles,coord,TILE_LAYER_ROAD_ARTERY,0));
    Camera camera; camera_init(&camera);
    uint64_t limits[TILE_LAYER_COUNT];
    for (unsigned k=0;k<TILE_LAYER_COUNT;k++) limits[k]=32ull*1024*1024;
    char name[128]="candidate";
    RegionInfo info = {.name=name};
    RegionPrepare *p = region_prepare_start(&info,&camera,320,240,limits); assert(p);
    strcpy(name,"mutated-catalog-name");
    RegionPrepareProgress progress=wait_result(p);
    assert(progress.ok && !progress.canceled && progress.ready==1);
    assert(progress.examined==progress.total);
    assert(!strcmp(p->region.name,"candidate"));
    TileManager retained=p->managers[TILE_LAYER_ROAD_ARTERY];
    memset(&p->managers[TILE_LAYER_ROAD_ARTERY],0,sizeof(TileManager));
    region_prepare_destroy(p);
    assert(tile_manager_peek_tile(&retained,coord,TILE_BAND_COARSE));
    tile_manager_shutdown(&retained);
    info.name="candidate";
    // Cancellation after decode prevents adoption; repeated immediate cancel also drains safely.
    for (unsigned i=0;i<20;i++) {
        p=region_prepare_start(&info,&camera,320,240,limits); assert(p);
        if (i==0) assert(wait_result(p).ok);
        region_prepare_cancel(p);
        assert(region_prepare_progress(p).canceled);
        region_prepare_destroy(p);
    }
    // Drives the actual owner-thread transition through cancel, editor/upload failure and publication.
    AppState *app=calloc(1,sizeof(*app)); assert(app);
    app->width=320; app->height=240; app->region.name="old-map"; app->region_index=9;
    app->view_state_bridge.camera=camera;
    assert(vk_tile_cache_init(&app->tile_state_bridge.vk_tile_cache,256));
    uint64_t previous_gpu_limit=app->tile_state_bridge.vk_tile_cache.byte_limit;
    assert(app_region_transition_start(app,&info,1));
    assert(app->region_budgets_reserved && app->tile_state_bridge.vk_tile_cache.byte_limit==1);
    assert(!app_region_transition_start(app,&info,2));
    app_region_transition_draw(app); assert(cancel_draws==1);
    SDL_FRect cancel=app_region_transition_cancel_rect(app);
    SDL_Event event={0}; event.type=SDL_MOUSEBUTTONDOWN; event.button.button=SDL_BUTTON_LEFT;
    event.button.x=cancel.x+5; event.button.y=cancel.y+5;
    assert(!app_region_transition_handle_event(app,&event));
    assert(!region_prepare_progress(app->region_prepare).canceled);
    event.type=SDL_KEYDOWN; event.key.keysym.sym=SDLK_ESCAPE;
    assert(app_region_transition_handle_event(app,&event));
    wait_result(app->region_prepare); app_region_transition_tick(app);
    assert(!app->region_prepare && !strcmp(app->region.name,"old-map") && publications==0);
    assert(!app->region_budgets_reserved && app->tile_state_bridge.vk_tile_cache.byte_limit==previous_gpu_limit);
    assert(app_region_transition_start(app,&info,1));
    app_region_transition_cancel(app); // Direct semantic intent retains actual worker policy.
    assert(region_prepare_progress(app->region_prepare).canceled);
    wait_result(app->region_prepare); app_region_transition_tick(app);
    assert(!app->region_prepare && !strcmp(app->region.name,"old-map") && publications==0);
    assert(app_region_transition_start(app,&info,1));
    wait_result(app->region_prepare); dialog_active=true;
    for (unsigned i=0;i<5 && app->region_prepare;i++) app_region_transition_tick(app);
    assert(!app->region_prepare && publications==0 && app->region_index==9);
    dialog_active=false; app->renderer.vk=(void *)1; refuse_upload=true;
    assert(app_region_transition_start(app,&info,1));
    wait_result(app->region_prepare); app_region_transition_tick(app);
    assert(!app->region_prepare && publications==0 && app->region_index==9);
    refuse_upload=false; app->renderer.vk=NULL;
    assert(app_region_transition_start(app,&info,1));
    wait_result(app->region_prepare);
    for (unsigned i=0;i<5 && app->region_prepare;i++) app_region_transition_tick(app);
    assert(!app->region_prepare && publications==1 && retired_loaders==1 && app->region_index==1);
    assert(!strcmp(app->region.name,"candidate"));
    assert(tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[TILE_LAYER_ROAD_ARTERY],coord,TILE_BAND_COARSE));
    assert(app_region_transition_start(app,&info,2));
    app_region_transition_shutdown(app); assert(!app->region_prepare && publications==1);
    for (unsigned k=0;k<TILE_LAYER_COUNT;k++) tile_manager_shutdown(&app->tile_state_bridge.tile_managers[k]);
    vk_tile_cache_shutdown(&app->tile_state_bridge.vk_tile_cache); free(app);
    // Combined-memory admission refuses even a valid pack without mutating caller ownership.
    limits[TILE_LAYER_ROAD_ARTERY]=1;
    p=region_prepare_start(&info,&camera,320,240,limits); assert(p);
    assert(!wait_result(p).ok); region_prepare_destroy(p);
    info.name="missing";
    p=region_prepare_start(&info,&camera,320,240,limits); assert(p);
    progress=wait_result(p);
    assert(!progress.ok && strstr(progress.message,"meta.json"));
    region_prepare_destroy(p);
    assert(region_prepare_start(&info,&camera,0,240,limits)==NULL);
    mapforge_tile_source_fixture_cleanup(&fixture);
    unsetenv("MAPFORGE_REGIONS_DIR");
    puts("background region cancellation/failure/ownership tests passed");
    return 0;
}
