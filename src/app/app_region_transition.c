#include "app/app_region_transition.h"
#include "app/region_prepare.h"
#include "app/app_runtime_ingest_internal.h"
#include "app/app_place_tools.h"
#include "app/app_ui_internal.h"
#include "core/time.h"
#include "core/log.h"
#include "ui/font.h"
#include <stdio.h>
#include <string.h>

// Releases uncommitted CPU/GPU ownership while retaining the active map.
static void transition_discard(AppState *app) {
    if (app->region_gpu_initialized) {
        vk_tile_cache_clear_with_renderer(&app->region_pending_gpu, app->renderer.vk);
        vk_tile_cache_shutdown(&app->region_pending_gpu);
    }
    memset(&app->region_pending_gpu, 0, sizeof(app->region_pending_gpu));
    app->region_gpu_initialized = false;
    if (app->region_budgets_reserved) {
        for (unsigned k = 0; k < TILE_LAYER_COUNT; ++k)
            app->tile_state_bridge.tile_managers[k].byte_limit = app->region_previous_cpu_limits[k];
        app->tile_state_bridge.vk_tile_cache.byte_limit = app->region_previous_gpu_limit;
        app->region_budgets_reserved = false;
    }
    region_prepare_destroy(app->region_prepare);
    app->region_prepare = NULL;
}

bool app_region_transition_start(AppState *app, const RegionInfo *info, int index) {
    if (!app || !info) return false;
    if (app->region_prepare) {
        snprintf(app->ingest_status, sizeof(app->ingest_status), "A map is preparing; cancel it before opening another");
        return false;
    }
    if (app->ui_state_bridge.pin_editor_has_draft || app_place_tools_active()) {
        snprintf(app->ingest_status, sizeof(app->ingest_status), "Finish or cancel location edits before changing maps");
        return false;
    }
    uint64_t cpu_limits[TILE_LAYER_COUNT];
    for (unsigned k = 0; k < TILE_LAYER_COUNT; ++k) {
        uint64_t limit = (k <= TILE_LAYER_ROAD_LOCAL ? 64ull : 32ull) * 1024 * 1024;
        uint64_t used = tile_manager_payload_bytes(&app->tile_state_bridge.tile_managers[k]);
        cpu_limits[k] = used < limit ? limit - used : 0;
    }
    app->region_prepare = region_prepare_start(info, &app->view_state_bridge.camera, app->width, app->height, cpu_limits);
    if (!app->region_prepare) return false;
    // Reserve staging headroom without letting concurrent active-map refinement spend it.
    for (unsigned k = 0; k < TILE_LAYER_COUNT; ++k) {
        TileManager *manager = &app->tile_state_bridge.tile_managers[k];
        app->region_previous_cpu_limits[k] = manager->byte_limit;
        uint64_t used = tile_manager_payload_bytes(manager);
        manager->byte_limit = used ? used : 1;
    }
    app->region_previous_gpu_limit = app->tile_state_bridge.vk_tile_cache.byte_limit;
    uint64_t gpu_used = app->tile_state_bridge.vk_tile_cache.mesh_bytes;
    app->tile_state_bridge.vk_tile_cache.byte_limit = gpu_used ? gpu_used : 1;
    app->region_budgets_reserved = true;
    app->region_pending_index = index;
    app->region_upload_cursor = app->region_uploaded = 0;
    app->region_prepare_started = time_now_seconds();
    snprintf(app->ingest_status, sizeof(app->ingest_status), "Preparing %s; current map remains available", info->name);
    return true;
}

// Transfers fully prepared ownership at a between-frame boundary and starts ordinary refinement.
static bool transition_publish(AppState *app, RegionPrepare *p) {
    int region_index = app->region_pending_index;
    app->region_index = region_index;
    app->region = p->region;
    snprintf(app->active_region_name, sizeof(app->active_region_name), "%s", p->name);
    app->region.name = app->active_region_name;
    region_log_archive_rollup_summary(&app->region, "switch");
    log_info("Region '%s' runtime source policy=%s storage=%s archive=%s",
             app->region.name ? app->region.name : "unknown",
             tile_source_policy_mode_label(app->region.tile_source.policy_mode),
             tile_storage_kind_label(app->region.tile_source.storage_kind),
             app->region.has_tile_archive ? "yes" : "no");
    if (p->validation.archive_storage && p->validation.archive_fallback_tree) {
        log_info("Region '%s' runtime source degraded: tree fallback enabled (policy=%s archive_reader_supported=%s)",
                 app->region.name ? app->region.name : "unknown",
                 tile_source_policy_mode_label(app->region.tile_source.policy_mode),
                 p->validation.archive_reader_supported ? "yes" : "no");
    }
    tile_source_runtime_stats_reset();
    app_worker_contract_bump_world_generation(app);
    app_worker_contract_bump_tile_generation(app);
    app_clear_tile_queue(app);
    tile_loader_shutdown(&app->tile_state_bridge.tile_loader);
    vk_tile_cache_clear_with_renderer(&app->tile_state_bridge.vk_tile_cache, app->renderer.vk);
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        tile_manager_shutdown(&app->tile_state_bridge.tile_managers[i]);
        app->tile_state_bridge.tile_managers[i] = p->managers[i];
        memset(&p->managers[i], 0, sizeof(p->managers[i]));
        app->tile_state_bridge.tile_managers[i].byte_limit = i <= TILE_LAYER_ROAD_LOCAL ? 64ull * 1024 * 1024 : 32ull * 1024 * 1024;
    }
    vk_tile_cache_shutdown(&app->tile_state_bridge.vk_tile_cache);
    app->tile_state_bridge.vk_tile_cache = app->region_pending_gpu;
    memset(&app->region_pending_gpu, 0, sizeof(app->region_pending_gpu));
    app->tile_state_bridge.vk_tile_cache.byte_limit = 512ull * 1024 * 1024;
    for (unsigned kind = 0; kind < TILE_LAYER_COUNT; ++kind) {
        TileManager *manager = &app->tile_state_bridge.tile_managers[kind];
        for (uint32_t i = 0; i < manager->capacity; ++i) {
            TileEntry *entry = &manager->entries[i];
            if (!entry->occupied) continue;
            app_tile_lifecycle_transition(app, (TileLayerKind)kind, entry->coord, entry->band, APP_TILE_LIFECYCLE_REQUESTED, false, false, false, false);
            app_tile_lifecycle_transition(app, (TileLayerKind)kind, entry->coord, entry->band, APP_TILE_LIFECYCLE_DECODED_CPU, true, false, false, false);
        }
    }
    if (!tile_loader_init_with_source(&app->tile_state_bridge.tile_loader, &app->region.tile_source))
        log_error("New region is drawable but its background loader could not start");
    app_tile_viewport_invalidate(app);
    app->tile_state_bridge.loading_expected = 0;
    app->tile_state_bridge.loading_done = 0;
    app->tile_state_bridge.loading_no_data_time = 0.0f;
    if (!app_load_route_graph(app)) {
        log_error("Route graph load kickoff failed for region '%s'; skipping route interactions in this region.", app->region.name);
    }
    app_playback_reset(app);
    memset(&app->route_state_bridge.hover_anchor, 0, sizeof(app->route_state_bridge.hover_anchor));
    memset(&app->route_state_bridge.start_anchor, 0, sizeof(app->route_state_bridge.start_anchor));
    memset(&app->route_state_bridge.goal_anchor, 0, sizeof(app->route_state_bridge.goal_anchor));
    app->view_state_bridge.building_zoom_bias = app_building_zoom_bias_for_region(&app->region);
    app->view_state_bridge.road_zoom_bias = app_road_zoom_bias_for_region(&app->region);
    app->view_state_bridge.camera = p->camera;
    app_reload_pins_state(app);
    app_runtime_format_region_package_status(app->region.name, &p->validation, app->ingest_package_status, sizeof(app->ingest_package_status));
    snprintf(app->ingest_status, sizeof(app->ingest_status), "Opened region package: %s", app->ingest_package_status);
    snprintf(app->latest_imported_region, sizeof(app->latest_imported_region), "%s", app->region.name ? app->region.name : "");
    return true;
}

void app_region_transition_tick(AppState *app) {
    if (!app || !app->region_prepare) return;
    RegionPrepare *p = app->region_prepare;
    RegionPrepareProgress progress = region_prepare_progress(p);
    if (!progress.done) {
        if (progress.canceled) {
            snprintf(app->ingest_status, sizeof(app->ingest_status), "Canceling map preparation; current map retained");
            return;
        }
        snprintf(app->ingest_status, sizeof(app->ingest_status), "%s: %.120s (%u/%u)", p->name, progress.message, progress.examined, progress.total);
        return;
    }
    if (!progress.ok || progress.canceled) {
        snprintf(app->ingest_status, sizeof(app->ingest_status), "%.180s; current map retained", progress.canceled ? "Map preparation canceled" : progress.message);
        transition_discard(app);
        return;
    }
    if (!app->region_gpu_initialized) {
        if (!vk_tile_cache_init(&app->region_pending_gpu, app->tile_state_bridge.vk_tile_cache.capacity)) goto rejected;
        app->region_gpu_initialized = true;
        uint64_t used = app->tile_state_bridge.vk_tile_cache.mesh_bytes;
        if (used >= app->region_pending_gpu.byte_limit) goto rejected;
        app->region_pending_gpu.byte_limit -= used;
    }
    double started = time_now_seconds();
    unsigned built = 0;
    while (app->region_upload_cursor < TILE_LAYER_COUNT * 256u) {
        unsigned k = app->region_upload_cursor / 256u, i = app->region_upload_cursor % 256u;
        app->region_upload_cursor++;
        TileEntry *entry = &p->managers[k].entries[i];
        if (!entry->occupied) continue;
        if (app->renderer.vk && !vk_tile_cache_on_tile_loaded(&app->region_pending_gpu, app->renderer.vk,
                (TileLayerKind)k, entry->coord, entry->band, &entry->tile)) goto rejected;
#if defined(MAPFORGE_HAVE_VK)
        if (app->renderer.vk && k <= TILE_LAYER_ROAD_LOCAL && entry->tile.point_total) {
            const VkTileCacheEntry *asset = vk_tile_cache_peek(&app->region_pending_gpu, (TileLayerKind)k, entry->coord, entry->band);
            bool ready = false;
            if (asset) for (unsigned road = 0; road <= ROAD_CLASS_PATH; ++road) ready |= asset->road_mesh_ready[road];
            if (!ready) goto rejected;
        }
#endif
        for (uint32_t n = 0; n < app->region_pending_gpu.capacity; ++n)
            if (app->region_pending_gpu.entries[n].occupied) app->region_pending_gpu.entries[n].presentation_pinned = true;
        app->region_uploaded++;
        if (++built >= 2u || time_now_seconds() - started >= .003) break;
    }
    snprintf(app->ingest_status, sizeof(app->ingest_status), "Preparing %s: display %u/%u; Esc cancels", p->name, app->region_uploaded, progress.ready);
    if (app->region_upload_cursor < TILE_LAYER_COUNT * 256u) return;
    if (app->ui_state_bridge.pin_editor_has_draft || app_place_tools_active()) goto rejected;
    for (unsigned k = 0; k < TILE_LAYER_COUNT; ++k) {
        uint64_t limit = (k <= TILE_LAYER_ROAD_LOCAL ? 64ull : 32ull) * 1024 * 1024;
        if (tile_manager_payload_bytes(&p->managers[k]) + tile_manager_payload_bytes(&app->tile_state_bridge.tile_managers[k]) > limit) goto rejected;
    }
    if (app->region_pending_gpu.mesh_bytes + app->tile_state_bridge.vk_tile_cache.mesh_bytes > 512ull * 1024 * 1024) goto rejected;
    transition_publish(app, p);
    log_info("region_stage published=%s tiles=%u elapsed_ms=%.2f", app->region.name, progress.ready, (time_now_seconds() - app->region_prepare_started) * 1000);
    app->region_gpu_initialized = false;
    app->region_budgets_reserved = false;
    transition_discard(app);
    return;
rejected:
    snprintf(app->ingest_status, sizeof(app->ingest_status), "Map preparation refused by display/memory/edit guard; current map retained");
    transition_discard(app);
}

// Shares one compact bottom status geometry between draw and Cancel hit testing.
static SDL_FRect transition_cancel_rect(const AppState *app) {
    return (SDL_FRect){(float)app->width - 136, (float)app->height - 65, 120, 30};
}

bool app_region_transition_handle_event(AppState *app, const SDL_Event *event) {
    if (!app || !app->region_prepare || !event) return false;
    SDL_FRect cancel = transition_cancel_rect(app);
    bool key = event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE;
    bool click = event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT &&
        event->button.x >= cancel.x && event->button.x <= cancel.x + cancel.w &&
        event->button.y >= cancel.y && event->button.y <= cancel.y + cancel.h;
    if (!key && !click) return false;
    region_prepare_cancel(app->region_prepare);
    snprintf(app->ingest_status, sizeof(app->ingest_status), "Canceling map preparation; current map retained");
    return true;
}

void app_region_transition_draw(AppState *app) {
    if (!app || !app->region_prepare) return;
    MapForgeThemePalette palette = app_ui_theme_palette();
    SDL_FRect box = {12, (float)app->height - 78, (float)app->width - 24, 64};
    SDL_FRect cancel = transition_cancel_rect(app);
    renderer_set_draw_color(&app->renderer, palette.button_fill.r, palette.button_fill.g, palette.button_fill.b, 255);
    renderer_fill_rect(&app->renderer, &box);
    renderer_set_draw_color(&app->renderer, palette.button_outline.r, palette.button_outline.g, palette.button_outline.b, 255);
    renderer_draw_rect(&app->renderer, &box);
    renderer_draw_rect(&app->renderer, &cancel);
    ui_draw_text_clipped(&app->renderer, 24, (int)box.y + 12, app->ingest_status, palette.text_primary, .9f, app->width - 180);
    ui_draw_text_clipped(&app->renderer, (int)cancel.x + 9, (int)cancel.y + 7, "CANCEL (ESC)", palette.text_primary, .8f, 108);
}

void app_region_transition_shutdown(AppState *app) {
    if (!app || !app->region_prepare) return;
    region_prepare_cancel(app->region_prepare);
    transition_discard(app);
}
