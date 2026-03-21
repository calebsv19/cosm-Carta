#include "app/app_internal.h"

#include "core/time.h"
#include "map/road_renderer.h"
#include "route/route_render.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"

#include <stdio.h>

void app_runtime_render_frame(AppState *app,
                              RendererBackend *io_last_backend,
                              uint32_t *out_visible_tiles,
                              double *out_before_present,
                              double *out_after_render) {
    if (!app || !io_last_backend) {
        if (out_visible_tiles) {
            *out_visible_tiles = 0u;
        }
        if (out_before_present) {
            *out_before_present = 0.0;
        }
        if (out_after_render) {
            *out_after_render = 0.0;
        }
        return;
    }

    renderer_begin_frame(&app->renderer);
    RendererBackend frame_backend = renderer_get_backend(&app->renderer);
    if (frame_backend != *io_last_backend) {
        ui_font_invalidate_cache(&app->renderer);
        app->vk_assets_enabled = frame_backend == RENDERER_BACKEND_VULKAN;
        if (!app->vk_assets_enabled) {
            vk_tile_cache_clear_with_renderer(&app->vk_tile_cache, app->renderer.vk);
            app_vk_poly_prep_clear(app);
            app_vk_asset_queue_clear(app);
        }
        *io_last_backend = frame_backend;
    }
    {
        MapForgeThemePalette palette = {0};
        if (mapforge_shared_theme_resolve_palette(&palette)) {
            renderer_clear(&app->renderer,
                           palette.background_clear.r,
                           palette.background_clear.g,
                           palette.background_clear.b,
                           palette.background_clear.a);
        } else {
            renderer_clear(&app->renderer, 20, 20, 28, 255);
        }
    }

    road_renderer_stats_reset();
    uint32_t visible_tiles = app_draw_visible_tiles(app);

    app_draw_region_bounds(app);
    route_render_draw(&app->renderer, &app->camera, &app->route.graph, &app->route.path, &app->route.drive_path, &app->route.walk_path,
        &app->route.alternatives, app->route.objective, app->route_alt_visible,
        app->route.has_start, app->route.start_node, app->start_anchor.valid, app->start_anchor.world_x, app->start_anchor.world_y,
        app->route.has_goal, app->route.goal_node, app->goal_anchor.valid, app->goal_anchor.world_x, app->goal_anchor.world_y,
        app->route.has_transfer, app->route.transfer_node);
    app_draw_hover_marker(app);
    app_draw_playback_marker(app);
    app_draw_route_panel(app);
    app_draw_header_bar(app);
    app_draw_layer_debug(app);
    debug_overlay_render(&app->overlay, &app->renderer);

    double before_present = time_now_seconds();
    renderer_end_frame(&app->renderer);
    double after_render = time_now_seconds();

    app->overlay.visible_tiles = visible_tiles;
    uint32_t cached_total = 0u;
    uint32_t capacity_total = 0u;
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        cached_total += tile_manager_count(&app->tile_managers[i]);
        capacity_total += tile_manager_capacity(&app->tile_managers[i]);
    }
    app->overlay.cached_tiles = cached_total;
    app->overlay.cache_capacity = capacity_total;

    if (app->overlay.enabled) {
        char title[128];
        const char *profile = route_objective_label(app->route.objective);
        const char *tier = road_renderer_zoom_tier_label(app->camera.zoom);
        const char *mode = app->route.mode == ROUTE_MODE_CAR ? "car" : "walk";
        const RoutePath *primary_path = app_route_primary_path(app, NULL);
        if (primary_path && primary_path->count > 1) {
            float km = primary_path->total_length_m / 1000.0f;
            float minutes = primary_path->total_time_s / 60.0f;
            if (app->route.mode == ROUTE_MODE_CAR &&
                primary_path == &app->route.path &&
                app->route.drive_path.count > 1 &&
                app->route.walk_path.count > 1) {
                float drive_minutes = app->route.drive_path.total_time_s / 60.0f;
                float walk_minutes = app->route.walk_path.total_time_s / 60.0f;
                snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | drive %.1f min + walk %.1f min | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                         app->region.name, mode, profile, tier, km, drive_minutes, walk_minutes, app->playback_speed, app->overlay.fps, app->overlay.visible_tiles, app->overlay.cached_tiles, app->overlay.cache_capacity);
            } else if (app->route.mode == ROUTE_MODE_WALK) {
                snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | %.1f min walk | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                         app->region.name, mode, profile, tier, km, minutes, app->playback_speed, app->overlay.fps, app->overlay.visible_tiles, app->overlay.cached_tiles, app->overlay.cache_capacity);
            } else {
                snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | %.1f min | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                         app->region.name, mode, profile, tier, km, minutes, app->playback_speed, app->overlay.fps, app->overlay.visible_tiles, app->overlay.cached_tiles, app->overlay.cache_capacity);
            }
        } else {
            const char *graph_status = app->route.loaded ? "graph ok" : "graph missing";
            snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %s | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                     app->region.name, mode, profile, tier, graph_status, app->playback_speed, app->overlay.fps, app->overlay.visible_tiles, app->overlay.cached_tiles, app->overlay.cache_capacity);
        }
        SDL_SetWindowTitle(app->window, title);
    } else {
        SDL_SetWindowTitle(app->window, "MapForge");
    }

    if (out_visible_tiles) {
        *out_visible_tiles = visible_tiles;
    }
    if (out_before_present) {
        *out_before_present = before_present;
    }
    if (out_after_render) {
        *out_after_render = after_render;
    }
}
