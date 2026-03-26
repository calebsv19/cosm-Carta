#include "app/app_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t g_vk_enqueue_calls = 0u;
static uint32_t g_polygon_draw_calls = 0u;
static uint32_t g_road_draw_calls = 0u;
static bool g_vk_cached_polygon_draw_result = false;
static bool g_vk_cached_road_draw_result = false;
static AppState *g_tile_lookup_app = NULL;
static TileCoord g_tile_lookup_coord = {0};
static bool g_tile_lookup_coord_enabled = false;
static const MftTile *g_tile_lookup_band_tiles[TILE_LAYER_COUNT][TILE_BAND_COUNT];
static uint64_t g_fake_ticks = 1000u;

// Link stubs for presenter dependencies not exercised by this policy test.
const MftTile *tile_manager_peek_tile(const TileManager *manager, TileCoord coord, TileZoomBand band) {
    if (!manager || !g_tile_lookup_app || band < 0 || band >= TILE_BAND_COUNT) {
        return NULL;
    }
    if (g_tile_lookup_coord_enabled &&
        (coord.z != g_tile_lookup_coord.z || coord.x != g_tile_lookup_coord.x || coord.y != g_tile_lookup_coord.y)) {
        return NULL;
    }
    for (int kind = 0; kind < TILE_LAYER_COUNT; ++kind) {
        if (manager == &g_tile_lookup_app->tile_state_bridge.tile_managers[kind]) {
            return g_tile_lookup_band_tiles[kind][band];
        }
    }
    return NULL;
}

void polygon_renderer_draw_tile(Renderer *renderer,
                                const Camera *camera,
                                MftTile *tile,
                                bool show_landuse,
                                float building_zoom_bias,
                                bool building_fill_enabled,
                                bool polygon_outline_only,
                                float opacity_scale) {
    (void)renderer;
    (void)camera;
    (void)tile;
    (void)show_landuse;
    (void)building_zoom_bias;
    (void)building_fill_enabled;
    (void)polygon_outline_only;
    (void)opacity_scale;
    g_polygon_draw_calls += 1u;
}

void road_renderer_draw_tile(Renderer *renderer,
                             const Camera *camera,
                             const MftTile *tile,
                             bool single_line,
                             float zoom_bias,
                             float opacity_scale) {
    (void)renderer;
    (void)camera;
    (void)tile;
    (void)single_line;
    (void)zoom_bias;
    (void)opacity_scale;
    g_road_draw_calls += 1u;
}

bool app_vk_asset_enqueue(AppState *app, TileLayerKind kind, TileCoord coord, TileZoomBand band) {
    (void)app;
    (void)kind;
    (void)coord;
    (void)band;
    g_vk_enqueue_calls += 1u;
    return true;
}

bool app_try_draw_vk_cached_polygon_tile(AppState *app,
                                         TileLayerKind kind,
                                         TileCoord coord,
                                         TileZoomBand band,
                                         VkPolyFillBudget *budget,
                                         VkPolyAssetBuildBudget *asset_build_budget) {
    (void)app;
    (void)kind;
    (void)coord;
    (void)band;
    (void)budget;
    (void)asset_build_budget;
    return g_vk_cached_polygon_draw_result;
}

bool app_try_draw_vk_cached_tile(AppState *app, TileLayerKind kind, TileCoord coord, TileZoomBand band) {
    (void)app;
    (void)kind;
    (void)coord;
    (void)band;
    return g_vk_cached_road_draw_result;
}

RendererBackend renderer_get_backend(const Renderer *renderer) {
    if (!renderer) {
        return RENDERER_BACKEND_SDL;
    }
    return renderer->backend;
}

uint64_t SDL_GetTicks64(void) {
    return g_fake_ticks++;
}

void log_error(const char *fmt, ...) {
    (void)fmt;
}

int main(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    app.tile_state_bridge.present_hold_tick = 1u;

    TileCoord coord = {14u, 1234u, 5678u};
    TileZoomBand out_band = TILE_BAND_DEFAULT;

    // Initial miss.
    assert(!app_tile_presenter_present_hold_lookup(&app, TILE_LAYER_ROAD_LOCAL, coord, 10.0, &out_band));
    assert(app.tile_state_bridge.present_hold_misses == 1u);

    // Remember + hit before TTL expiry.
    app_tile_presenter_present_hold_remember(&app, TILE_LAYER_ROAD_LOCAL, coord, TILE_BAND_MID, 10.0);
    assert(app.tile_state_bridge.present_hold_updates == 1u);
    assert(app_tile_presenter_present_hold_lookup(&app, TILE_LAYER_ROAD_LOCAL, coord, 10.10, &out_band));
    assert(out_band == TILE_BAND_MID);
    assert(app.tile_state_bridge.present_hold_hits == 1u);

    // Expire and ensure miss.
    assert(!app_tile_presenter_present_hold_lookup(&app, TILE_LAYER_ROAD_LOCAL, coord, 10.40, &out_band));
    assert(app.tile_state_bridge.present_hold_misses == 2u);

    // Reset counters contract.
    app_tile_presenter_reset_frame_counters(&app);
    assert(app.tile_state_bridge.present_hold_hits == 0u);
    assert(app.tile_state_bridge.present_hold_misses == 0u);
    assert(app.tile_state_bridge.present_hold_updates == 0u);
    assert(app.tile_state_bridge.transition_blend_draw_count == 0u);

    // Capacity-pressure replacement: with no expired slots, oldest slot should be reused.
    app.tile_state_bridge.present_hold_tick = 1u;
    for (uint32_t i = 0u; i < APP_TILE_PRESENT_HOLD_CAPACITY; ++i) {
        TileCoord slot_coord = {13u, 1000u + i, 2000u + i};
        app_tile_presenter_present_hold_remember(&app, TILE_LAYER_POLY_WATER, slot_coord, TILE_BAND_DEFAULT, 20.0);
    }
    TileCoord overflow = {13u, 999999u, 888888u};
    app_tile_presenter_present_hold_remember(&app, TILE_LAYER_POLY_WATER, overflow, TILE_BAND_MID, 20.0);

    TileCoord first = {13u, 1000u, 2000u};
    assert(!app_tile_presenter_present_hold_lookup(&app, TILE_LAYER_POLY_WATER, first, 20.01, &out_band));
    assert(app_tile_presenter_present_hold_lookup(&app, TILE_LAYER_POLY_WATER, overflow, 20.01, &out_band));
    assert(out_band == TILE_BAND_MID);

    // No VK asset + no immediate fallback: enqueue only, no fallback draw.
    memset(&app, 0, sizeof(app));
    app.renderer.backend = RENDERER_BACKEND_VULKAN;
    app.tile_state_bridge.vk_assets_enabled = true;
    g_vk_cached_polygon_draw_result = false;
    g_vk_enqueue_calls = 0u;
    g_polygon_draw_calls = 0u;
    uint32_t vk_misses = 0u;
    MftTile tile = {0};
    bool drew = app_tile_presenter_draw_polygon_layer(&app,
                                                      TILE_LAYER_POLY_PARK,
                                                      coord,
                                                      &tile,
                                                      TILE_BAND_MID,
                                                      0.0f,
                                                      1.0f,
                                                      false,
                                                      false,
                                                      NULL,
                                                      NULL,
                                                      30.0,
                                                      &vk_misses);
    assert(!drew);
    assert(vk_misses == 1u);
    assert(g_vk_enqueue_calls == 1u);
    assert(g_polygon_draw_calls == 0u);

    // Road path fallback: no cached mesh still enqueues and draws fallback line tile.
    memset(&app, 0, sizeof(app));
    app.renderer.backend = RENDERER_BACKEND_VULKAN;
    app.tile_state_bridge.vk_assets_enabled = true;
    g_vk_cached_road_draw_result = false;
    g_vk_enqueue_calls = 0u;
    g_road_draw_calls = 0u;
    vk_misses = 0u;
    drew = app_tile_presenter_draw_road_layer(&app,
                                              TILE_LAYER_ROAD_LOCAL,
                                              coord,
                                              &tile,
                                              TILE_BAND_MID,
                                              true,
                                              0.0f,
                                              1.0f,
                                              40.0,
                                              &vk_misses);
    assert(drew);
    assert(vk_misses == 1u);
    assert(g_vk_enqueue_calls == 1u);
    assert(g_road_draw_calls == 1u);

    // Road cached path: uses VK draw path and skips fallback line draw/enqueue.
    memset(&app, 0, sizeof(app));
    app.renderer.backend = RENDERER_BACKEND_VULKAN;
    app.tile_state_bridge.vk_assets_enabled = true;
    g_vk_cached_road_draw_result = true;
    g_vk_enqueue_calls = 0u;
    g_road_draw_calls = 0u;
    vk_misses = 0u;
    drew = app_tile_presenter_draw_road_layer(&app,
                                              TILE_LAYER_ROAD_LOCAL,
                                              coord,
                                              &tile,
                                              TILE_BAND_MID,
                                              true,
                                              0.0f,
                                              1.0f,
                                              41.0,
                                              &vk_misses);
    assert(drew);
    assert(vk_misses == 0u);
    assert(g_vk_enqueue_calls == 0u);
    assert(g_road_draw_calls == 0u);
    assert(app.tile_state_bridge.draw_path_vk_count == 1u);

    MftTile mid_tile = {0};
    MftTile fine_tile = {0};

    // Rapid band oscillation should not drop draw path when both bands are available.
    memset(&app, 0, sizeof(app));
    app.tile_state_bridge.present_hold_tick = 1u;
    g_tile_lookup_app = &app;
    g_tile_lookup_coord_enabled = true;
    g_tile_lookup_coord = coord;
    memset(g_tile_lookup_band_tiles, 0, sizeof(g_tile_lookup_band_tiles));
    g_tile_lookup_band_tiles[TILE_LAYER_ROAD_LOCAL][TILE_BAND_MID] = &mid_tile;
    g_tile_lookup_band_tiles[TILE_LAYER_ROAD_LOCAL][TILE_BAND_FINE] = &fine_tile;
    g_vk_enqueue_calls = 0u;
    g_road_draw_calls = 0u;
    for (int i = 0; i < 8; ++i) {
        bool fine_target = (i % 2) == 0;
        app.tile_state_bridge.previous_target_band[TILE_LAYER_ROAD_LOCAL] = fine_target ? TILE_BAND_MID : TILE_BAND_FINE;
        app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_LOCAL] = fine_target ? TILE_BAND_FINE : TILE_BAND_MID;
        app.tile_state_bridge.layer_band_last_change_time[TILE_LAYER_ROAD_LOCAL] = 60.0 + (double)i;
        vk_misses = 0u;
        drew = app_tile_presenter_draw_road_layer(&app,
                                                  TILE_LAYER_ROAD_LOCAL,
                                                  coord,
                                                  fine_target ? &fine_tile : &mid_tile,
                                                  fine_target ? TILE_BAND_FINE : TILE_BAND_MID,
                                                  true,
                                                  0.0f,
                                                  1.0f,
                                                  60.05 + (double)i,
                                                  &vk_misses);
        assert(drew);
        assert(vk_misses == 0u);
    }
    assert(g_vk_enqueue_calls == 0u);
    assert(g_road_draw_calls >= 16u);

    // Rapid band-switch path: blended draw occurs during transition and hold continuity resolves after source tile drops.
    memset(&app, 0, sizeof(app));
    app.tile_state_bridge.present_hold_tick = 1u;
    app.tile_state_bridge.previous_target_band[TILE_LAYER_ROAD_LOCAL] = TILE_BAND_MID;
    app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_LOCAL] = TILE_BAND_FINE;
    app.tile_state_bridge.layer_band_last_change_time[TILE_LAYER_ROAD_LOCAL] = 50.0;
    g_tile_lookup_app = &app;
    g_tile_lookup_coord_enabled = true;
    g_tile_lookup_coord = coord;
    memset(g_tile_lookup_band_tiles, 0, sizeof(g_tile_lookup_band_tiles));
    g_tile_lookup_band_tiles[TILE_LAYER_ROAD_LOCAL][TILE_BAND_MID] = &mid_tile;
    g_tile_lookup_band_tiles[TILE_LAYER_ROAD_LOCAL][TILE_BAND_FINE] = &fine_tile;
    g_road_draw_calls = 0u;
    drew = app_tile_presenter_draw_road_layer(&app,
                                              TILE_LAYER_ROAD_LOCAL,
                                              coord,
                                              &fine_tile,
                                              TILE_BAND_FINE,
                                              true,
                                              0.0f,
                                              1.0f,
                                              50.05,
                                              NULL);
    assert(drew);
    assert(g_road_draw_calls == 2u);
    assert(app.tile_state_bridge.transition_blend_draw_count == 1u);
    assert(app.tile_state_bridge.draw_path_fallback_count == 1u);

    // Drop source/target tiles, keep only hold-band tile, and ensure resolve uses hold continuity.
    g_tile_lookup_band_tiles[TILE_LAYER_ROAD_LOCAL][TILE_BAND_FINE] = NULL;
    g_tile_lookup_band_tiles[TILE_LAYER_ROAD_LOCAL][TILE_BAND_MID] = &mid_tile;
    const MftTile *resolved_tile = NULL;
    TileZoomBand resolved_band = TILE_BAND_DEFAULT;
    assert(app_tile_presenter_resolve_tile_for_present(&app,
                                                       TILE_LAYER_ROAD_LOCAL,
                                                       coord,
                                                       50.10,
                                                       &resolved_tile,
                                                       &resolved_band));
    assert(resolved_tile == &mid_tile);
    assert(resolved_band == TILE_BAND_MID);
    g_tile_lookup_app = NULL;
    g_tile_lookup_coord_enabled = false;

    // Frame invariant checks.
    memset(&app, 0, sizeof(app));
    app.renderer.backend = RENDERER_BACKEND_VULKAN;
    app.tile_state_bridge.vk_assets_enabled = true;
    app.tile_state_bridge.draw_path_vk_count = 2u;
    app.tile_state_bridge.draw_path_fallback_count = 3u;
    app.tile_state_bridge.transition_blend_draw_count = 1u;
    assert(app_tile_presenter_validate_frame_invariants(&app, 5u, 0u));
    app.tile_state_bridge.transition_blend_draw_count = 4u;
    assert(!app_tile_presenter_validate_frame_invariants(&app, 5u, 0u));
    app.tile_state_bridge.transition_blend_draw_count = 1u;
    app.renderer.backend = RENDERER_BACKEND_SDL;
    assert(!app_tile_presenter_validate_frame_invariants(&app, 5u, 0u));

    printf("app_tile_presenter_policy_test: success\n");
    return 0;
}
