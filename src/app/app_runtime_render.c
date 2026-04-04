#include "app/app_internal.h"

#include "core/time.h"
#include "map/road_renderer.h"
#include "route/route_render.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"

#include <stdio.h>
#include <string.h>

void app_runtime_render_derive_title_frame(const AppState *app,
                                           uint32_t visible_tiles,
                                           uint32_t cached_tiles,
                                           uint32_t cache_capacity,
                                           AppRuntimeRenderTitleFrame *out_title) {
    AppRuntimeRenderTitleFrame local = {0};
    if (!out_title) {
        out_title = &local;
    }
    memset(out_title, 0, sizeof(*out_title));

    if (!app) {
        return;
    }

    out_title->visible_tiles = visible_tiles;
    out_title->cached_tiles = cached_tiles;
    out_title->cache_capacity = cache_capacity;
    out_title->custom_title_enabled = app->ui_state_bridge.overlay.enabled;

    if (!out_title->custom_title_enabled) {
        snprintf(out_title->window_title, sizeof(out_title->window_title), "MapForge");
        return;
    }

    {
        char title[sizeof(out_title->window_title)];
        title[0] = '\0';
        const char *profile = route_objective_label(app->route_state_bridge.route.objective);
        const char *tier = road_renderer_zoom_tier_label(app->view_state_bridge.camera.zoom);
        const char *mode = app->route_state_bridge.route.mode == ROUTE_MODE_CAR ? "car" : "walk";
        const RoutePath *primary_path = app_route_primary_path(app, NULL);
        if (primary_path && primary_path->count > 1) {
            float km = primary_path->total_length_m / 1000.0f;
            float minutes = primary_path->total_time_s / 60.0f;
            if (app->route_state_bridge.route.mode == ROUTE_MODE_CAR &&
                primary_path == &app->route_state_bridge.route.path &&
                app->route_state_bridge.route.drive_path.count > 1 &&
                app->route_state_bridge.route.walk_path.count > 1) {
                float drive_minutes = app->route_state_bridge.route.drive_path.total_time_s / 60.0f;
                float walk_minutes = app->route_state_bridge.route.walk_path.total_time_s / 60.0f;
                snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | drive %.1f min + walk %.1f min | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                         app->region.name, mode, profile, tier, km, drive_minutes, walk_minutes, app->route_state_bridge.playback_speed, app->ui_state_bridge.overlay.fps, visible_tiles, cached_tiles, cache_capacity);
            } else if (app->route_state_bridge.route.mode == ROUTE_MODE_WALK) {
                snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | %.1f min walk | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                         app->region.name, mode, profile, tier, km, minutes, app->route_state_bridge.playback_speed, app->ui_state_bridge.overlay.fps, visible_tiles, cached_tiles, cache_capacity);
            } else {
                snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | %.1f min | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                         app->region.name, mode, profile, tier, km, minutes, app->route_state_bridge.playback_speed, app->ui_state_bridge.overlay.fps, visible_tiles, cached_tiles, cache_capacity);
            }
        } else {
            const char *graph_status = app->route_state_bridge.route.loaded ? "graph ok" : "graph missing";
            snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %s | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                     app->region.name, mode, profile, tier, graph_status, app->route_state_bridge.playback_speed, app->ui_state_bridge.overlay.fps, visible_tiles, cached_tiles, cache_capacity);
        }
        snprintf(out_title->window_title, sizeof(out_title->window_title), "%s", title);
    }
}

void app_runtime_render_apply_title_frame(AppState *app,
                                          const AppRuntimeRenderTitleFrame *title) {
    if (!app || !title) {
        return;
    }

    app->ui_state_bridge.overlay.visible_tiles = title->visible_tiles;
    app->ui_state_bridge.overlay.cached_tiles = title->cached_tiles;
    app->ui_state_bridge.overlay.cache_capacity = title->cache_capacity;

    if (title->custom_title_enabled) {
        SDL_SetWindowTitle(app->window, title->window_title);
    } else {
        SDL_SetWindowTitle(app->window, "MapForge");
    }
}

void app_runtime_render_derive_frame(const AppState *app,
                                     RendererBackend *io_last_backend,
                                     const AppRuntimeInputFrame *input_frame,
                                     AppRuntimeRenderDeriveFrame *out_derive) {
    AppRuntimeRenderDeriveFrame local = {0};
    if (!out_derive) {
        out_derive = &local;
    }
    memset(out_derive, 0, sizeof(*out_derive));

    if (!app || !io_last_backend) {
        return;
    }

    out_derive->frame_backend = renderer_get_backend(&app->renderer);
    out_derive->backend_changed = (out_derive->frame_backend != *io_last_backend);
    if (input_frame) {
        out_derive->input_invalidation_reason_bits = input_frame->invalidation.invalidation_reason_bits;
    }

    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        out_derive->cached_tiles += tile_manager_count(&app->tile_state_bridge.tile_managers[i]);
        out_derive->cache_capacity += tile_manager_capacity(&app->tile_state_bridge.tile_managers[i]);
    }

    MapForgeThemePalette palette = {0};
    if (mapforge_shared_theme_resolve_palette(&palette)) {
        out_derive->clear_r = palette.background_clear.r;
        out_derive->clear_g = palette.background_clear.g;
        out_derive->clear_b = palette.background_clear.b;
        out_derive->clear_a = palette.background_clear.a;
    } else {
        out_derive->clear_r = 20u;
        out_derive->clear_g = 20u;
        out_derive->clear_b = 28u;
        out_derive->clear_a = 255u;
    }
}

void app_runtime_render_submit_frame(AppState *app,
                                     const AppRuntimeRenderDeriveFrame *derive,
                                     AppRuntimeRenderSubmitFrame *out_submit) {
    AppRuntimeRenderSubmitFrame local = {0};
    if (!out_submit) {
        out_submit = &local;
    }
    memset(out_submit, 0, sizeof(*out_submit));

    if (!app || !derive) {
        return;
    }

    renderer_begin_frame(&app->renderer);
    if (derive->backend_changed) {
        ui_font_invalidate_cache(&app->renderer);
        app->tile_state_bridge.vk_assets_enabled = derive->frame_backend == RENDERER_BACKEND_VULKAN;
        if (!app->tile_state_bridge.vk_assets_enabled) {
            vk_tile_cache_clear_with_renderer(&app->tile_state_bridge.vk_tile_cache, app->renderer.vk);
            app_vk_poly_prep_clear(app);
            app_vk_asset_queue_clear(app);
        }
    }
    renderer_clear(&app->renderer, derive->clear_r, derive->clear_g, derive->clear_b, derive->clear_a);

    road_renderer_stats_reset();
    app_draw_visible_tiles(app, &out_submit->tile_stats);
    out_submit->draw_pass_count += 1u;

    app_draw_region_bounds(app);
    route_render_draw(&app->renderer, &app->view_state_bridge.camera, &app->route_state_bridge.route.graph, &app->route_state_bridge.route.path, &app->route_state_bridge.route.drive_path, &app->route_state_bridge.route.walk_path,
        &app->route_state_bridge.route.alternatives, app->route_state_bridge.route.objective, app->route_state_bridge.route_alt_visible,
        app->route_state_bridge.route.has_start, app->route_state_bridge.route.start_node, app->route_state_bridge.start_anchor.valid, app->route_state_bridge.start_anchor.world_x, app->route_state_bridge.start_anchor.world_y,
        app->route_state_bridge.route.has_goal, app->route_state_bridge.route.goal_node, app->route_state_bridge.goal_anchor.valid, app->route_state_bridge.goal_anchor.world_x, app->route_state_bridge.goal_anchor.world_y,
        app->route_state_bridge.route.has_transfer, app->route_state_bridge.route.transfer_node);
    app_draw_hover_marker(app);
    app_draw_playback_marker(app);
    app_draw_route_panel(app);
    app_draw_header_bar(app);
    app_draw_layer_debug(app);
    debug_overlay_render(&app->ui_state_bridge.overlay, &app->renderer);
    out_submit->draw_pass_count += 1u;

    out_submit->before_present = time_now_seconds();
    renderer_end_frame(&app->renderer);
    out_submit->after_render = time_now_seconds();
}

void app_runtime_render_frame(AppState *app,
                              RendererBackend *io_last_backend,
                              const AppRuntimeInputFrame *input_frame,
                              AppVisibleTileRenderStats *out_tile_stats,
                              double *out_after_render_derive,
                              uint32_t *out_draw_pass_count,
                              uint32_t *out_render_invalidation_reason_bits,
                              double *out_before_present,
                              double *out_after_render) {
    AppVisibleTileRenderStats local_stats = {0};
    if (!out_tile_stats) {
        out_tile_stats = &local_stats;
    }
    memset(out_tile_stats, 0, sizeof(*out_tile_stats));

    if (!app || !io_last_backend) {
        if (out_after_render_derive) {
            *out_after_render_derive = 0.0;
        }
        if (out_draw_pass_count) {
            *out_draw_pass_count = 0u;
        }
        if (out_render_invalidation_reason_bits) {
            *out_render_invalidation_reason_bits = 0u;
        }
        if (out_before_present) {
            *out_before_present = 0.0;
        }
        if (out_after_render) {
            *out_after_render = 0.0;
        }
        return;
    }

    AppRuntimeRenderDeriveFrame derive = {0};
    app_runtime_render_derive_frame(app, io_last_backend, input_frame, &derive);
    double after_render_derive = time_now_seconds();

    AppRuntimeRenderSubmitFrame submit = {0};
    app_runtime_render_submit_frame(app, &derive, &submit);

    *io_last_backend = derive.frame_backend;
    *out_tile_stats = submit.tile_stats;
    AppRuntimeRenderTitleFrame title = {0};
    app_runtime_render_derive_title_frame(app,
                                          submit.tile_stats.visible_tiles,
                                          derive.cached_tiles,
                                          derive.cache_capacity,
                                          &title);
    app_runtime_render_apply_title_frame(app, &title);

    if (out_after_render_derive) {
        *out_after_render_derive = after_render_derive;
    }
    if (out_draw_pass_count) {
        *out_draw_pass_count = submit.draw_pass_count;
    }
    if (out_render_invalidation_reason_bits) {
        *out_render_invalidation_reason_bits = derive.input_invalidation_reason_bits;
    }
    if (out_before_present) {
        *out_before_present = submit.before_present;
    }
    if (out_after_render) {
        *out_after_render = submit.after_render;
    }
    app_bridge_sync_to_legacy(app);
}
