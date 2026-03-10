#include "app/app.h"
#include "app/app_internal.h"

#include "core/log.h"
#include "core/time.h"
#include "app/region_loader.h"
#include "map/road_renderer.h"
#include "route/route_render.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"

#include <SDL.h>
#include <SDL2/SDL_ttf.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

static uint32_t app_sum_road_classes(const uint32_t *values, int first_class, int last_class) {
    if (!values || first_class < 0 || last_class < first_class) {
        return 0;
    }
    uint32_t sum = 0;
    for (int i = first_class; i <= last_class; ++i) {
        sum += values[i];
    }
    return sum;
}

static bool app_env_flag_enabled(const char *name) {
    if (!name) {
        return false;
    }
    const char *value = getenv(name);
    if (!value || value[0] == '\0') {
        return false;
    }
    if (strcmp(value, "1") == 0 ||
        strcmp(value, "true") == 0 ||
        strcmp(value, "TRUE") == 0 ||
        strcmp(value, "yes") == 0 ||
        strcmp(value, "YES") == 0 ||
        strcmp(value, "on") == 0 ||
        strcmp(value, "ON") == 0) {
        return true;
    }
    return false;
}

static bool app_trace_ensure_dirs(void) {
    if (mkdir("build", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("build/traces", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static void app_trace_emit_frame_samples(AppState *app, double rel_time_s) {
    if (!app || !app->trace_enabled) {
        return;
    }
    core_trace_emit_sample_f32(&app->trace_session, "frame", rel_time_s, (float)app->frame_timings.frame_ms);
    core_trace_emit_sample_f32(&app->trace_session, "events", rel_time_s, (float)app->frame_timings.events_ms);
    core_trace_emit_sample_f32(&app->trace_session, "update", rel_time_s, (float)app->frame_timings.update_ms);
    core_trace_emit_sample_f32(&app->trace_session, "queue", rel_time_s, (float)app->frame_timings.queue_ms);
    core_trace_emit_sample_f32(&app->trace_session, "integrate", rel_time_s, (float)app->frame_timings.integrate_ms);
    core_trace_emit_sample_f32(&app->trace_session, "route", rel_time_s, (float)app->frame_timings.route_ms);
    core_trace_emit_sample_f32(&app->trace_session, "render", rel_time_s, (float)app->frame_timings.render_ms);
    core_trace_emit_sample_f32(&app->trace_session, "present", rel_time_s, (float)app->frame_timings.present_ms);
}

static void app_trace_emit_queue_markers(AppState *app, double rel_time_s) {
    if (!app || !app->trace_enabled) {
        return;
    }

    TileLoaderStats stats = {0};
    tile_loader_get_stats(&app->tile_loader, &stats);
    if (stats.enqueue_drop_count > app->trace_last_tile_enqueue_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_enq_drop");
    }
    if (stats.enqueue_evict_count > app->trace_last_tile_enqueue_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_enq_evict");
    }
    if (stats.result_drop_count > app->trace_last_tile_result_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_res_drop");
    }
    if (stats.result_evict_count > app->trace_last_tile_result_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_res_evict");
    }
    if (app->vk_asset_job_drop_count > app->trace_last_vk_asset_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "vk_job_drop");
    }
    if (app->vk_asset_job_evict_count > app->trace_last_vk_asset_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "vk_job_evict");
    }

    app->trace_last_tile_enqueue_drop_count = stats.enqueue_drop_count;
    app->trace_last_tile_enqueue_evict_count = stats.enqueue_evict_count;
    app->trace_last_tile_result_drop_count = stats.result_drop_count;
    app->trace_last_tile_result_evict_count = stats.result_evict_count;
    app->trace_last_vk_asset_drop_count = app->vk_asset_job_drop_count;
    app->trace_last_vk_asset_evict_count = app->vk_asset_job_evict_count;
}

static void app_trace_shutdown(AppState *app) {
    if (!app || !app->trace_enabled) {
        return;
    }

    CoreResult final_result = core_trace_finalize(&app->trace_session);
    if (final_result.code != CORE_OK) {
        log_error("core_trace_finalize failed: %s", final_result.message);
    } else if (!app_trace_ensure_dirs()) {
        log_error("failed to create build/traces directory");
    } else {
        time_t now = time(NULL);
        struct tm local_tm = {0};
        localtime_r(&now, &local_tm);
        char path[256];
        strftime(path, sizeof(path), "build/traces/mapforge_trace_%Y%m%d_%H%M%S.pack", &local_tm);
        CoreResult export_result = core_trace_export_pack(&app->trace_session, path);
        if (export_result.code != CORE_OK) {
            log_error("core_trace_export_pack failed: %s", export_result.message);
        } else {
            log_info("trace exported: %s", path);
        }
    }

    core_trace_session_reset(&app->trace_session);
    app->trace_enabled = false;
}

static bool app_init(AppState *app) {
    if (!app) {
        return false;
    }

    mapforge_shared_theme_load_persisted();
    app->width = 1280;
    app->height = 720;
    renderer_set_backend(&app->renderer, RENDERER_BACKEND_SDL);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    const char *backend_env = getenv("MAPFORGE_RENDER_BACKEND");
    if (backend_env && strcmp(backend_env, "vulkan") == 0) {
        renderer_set_backend(&app->renderer, RENDERER_BACKEND_VULKAN);
    }
    log_info("Requested render backend env='%s' resolved='%s'",
             backend_env ? backend_env : "",
             renderer_backend_name(renderer_get_backend(&app->renderer)));

    uint32_t window_flags = SDL_WINDOW_SHOWN;
    if (renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN) {
        window_flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    }

    app->window = SDL_CreateWindow(
        "MapForge",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        app->width,
        app->height,
        (int)window_flags
    );

    if (!app->window) {
        log_error("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    if (!renderer_init(&app->renderer, app->window, app->width, app->height)) {
        if (renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN) {
            log_error("renderer_init vulkan path failed, retrying SDL fallback: %s", SDL_GetError());
            SDL_DestroyWindow(app->window);
            app->window = SDL_CreateWindow(
                "MapForge",
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                app->width,
                app->height,
                SDL_WINDOW_SHOWN
            );
            if (!app->window) {
                log_error("SDL_CreateWindow fallback failed: %s", SDL_GetError());
                return false;
            }
            renderer_set_backend(&app->renderer, RENDERER_BACKEND_SDL);
            if (!renderer_init(&app->renderer, app->window, app->width, app->height)) {
                log_error("renderer_init SDL fallback failed: %s", SDL_GetError());
                return false;
            }
        } else {
            log_error("renderer_init failed: %s", SDL_GetError());
            return false;
        }
    }
    log_info("Render backend: %s (vulkan_available=%s)",
             renderer_backend_name(renderer_get_backend(&app->renderer)),
             app->renderer.vulkan_available ? "yes" : "no");

    app->vk_assets_enabled = renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN;
    if (!vk_tile_cache_init(&app->vk_tile_cache, 2048)) {
        log_error("vk_tile_cache_init failed");
        return false;
    }
    if (!app_vk_poly_prep_init(app)) {
        log_error("app_vk_poly_prep_init failed");
        return false;
    }
    if (!app_vk_asset_worker_init(app)) {
        log_error("app_vk_asset_worker_init failed");
        return false;
    }
    if (!app_route_worker_init(app)) {
        log_error("app_route_worker_init failed");
        return false;
    }
    if (TTF_Init() != 0) {
        log_error("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    {
        char shared_font_path[384] = {0};
        int shared_font_size = 0;
        if (mapforge_shared_font_resolve_ui_regular(shared_font_path, sizeof(shared_font_path), &shared_font_size)) {
            ui_font_set(shared_font_path, shared_font_size);
        } else {
            ui_font_set("assets/fonts/Montserrat-Regular.ttf", 10);
        }
    }

    app->region_index = 0;
    const RegionInfo *info = region_get(app->region_index);
    if (!info) {
        log_error("No region configured");
        return false;
    }

    app->region = *info;
    region_load_meta(info, &app->region);

    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        if (!tile_manager_init(&app->tile_managers[i], 256, app->region.tiles_dir)) {
            log_error("tile_manager_init failed");
            return false;
        }
    }
    if (!tile_loader_init(&app->tile_loader, app->region.tiles_dir)) {
        log_error("tile_loader_init failed");
        return false;
    }
    app->trace_enabled = false;
    CoreTraceConfig trace_cfg = {
        .sample_capacity = APP_TRACE_SAMPLE_CAPACITY,
        .marker_capacity = APP_TRACE_MARKER_CAPACITY
    };
    CoreResult trace_init = core_trace_session_init(&app->trace_session, &trace_cfg);
    if (trace_init.code != CORE_OK) {
        log_error("core_trace_session_init failed: %s", trace_init.message);
    } else {
        TileLoaderStats trace_stats = {0};
        tile_loader_get_stats(&app->tile_loader, &trace_stats);
        app->trace_enabled = true;
        app->trace_start_time = time_now_seconds();
        app->trace_last_tile_enqueue_drop_count = trace_stats.enqueue_drop_count;
        app->trace_last_tile_enqueue_evict_count = trace_stats.enqueue_evict_count;
        app->trace_last_tile_result_drop_count = trace_stats.result_drop_count;
        app->trace_last_tile_result_evict_count = trace_stats.result_evict_count;
        app->trace_last_vk_asset_drop_count = app->vk_asset_job_drop_count;
        app->trace_last_vk_asset_evict_count = app->vk_asset_job_evict_count;
    }

    input_init(&app->input);
    camera_init(&app->camera);
    if (app->region.has_center) {
        MercatorMeters center = mercator_from_latlon((LatLon){app->region.center_lat, app->region.center_lon});
        app->camera.x = (float)center.x;
        app->camera.y = (float)center.y;
    }
    app_center_camera_on_region(&app->camera, &app->region, app->width, app->height);
    debug_overlay_init(&app->overlay);
    app->single_line = false;
    route_state_init(&app->route);
    app_load_route_graph(app);
    app->dragging_start = false;
    app->dragging_goal = false;
    app->has_hover = false;
    app->playback_playing = false;
    app->playback_time_s = 0.0f;
    app->playback_speed = 1.0f;
    app->show_landuse = false;
    app->building_zoom_bias = app_building_zoom_bias_for_region(&app->region);
    app->building_fill_enabled = false;
    app->road_zoom_bias = app_road_zoom_bias_for_region(&app->region);
    app->polygon_outline_only = false;
    app->tile_request_id = 1;
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        memset(&app->tile_queues[i], 0, sizeof(app->tile_queues[i]));
    }
    app->queue_valid = false;
    app->visible_valid = false;
    app->loading_expected = 0;
    app->loading_done = 0;
    app->loading_no_data_time = 0.0f;
    app->loading_layer_index = 0;

    return true;
}

static void app_shutdown(AppState *app) {
    if (!app) {
        return;
    }

    mapforge_shared_theme_save_persisted();

    if (TTF_WasInit()) {
        ui_font_shutdown(&app->renderer);
        TTF_Quit();
    }

    vk_tile_cache_clear_with_renderer(&app->vk_tile_cache, app->renderer.vk);
    renderer_shutdown(&app->renderer);
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        tile_manager_shutdown(&app->tile_managers[i]);
    }
    tile_loader_shutdown(&app->tile_loader);
    app_vk_poly_prep_shutdown(app);
    app_vk_asset_worker_shutdown(app);
    app_route_worker_shutdown(app);
    app_trace_shutdown(app);
    vk_tile_cache_shutdown(&app->vk_tile_cache);
    route_state_shutdown(&app->route);
    app_clear_tile_queue(app);

    if (app->window) {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
    }

    SDL_Quit();
}

int app_run(void) {
    AppState app = {0};
    if (!app_init(&app)) {
        app_shutdown(&app);
        return 1;
    }

    double last_time = time_now_seconds();
    double perf_next_log = last_time + 1.0;
    uint32_t last_loading_done = 0;
    double last_loading_progress_time = last_time;
    RendererBackend last_backend = renderer_get_backend(&app.renderer);
    bool vk_debug_logs = app_env_flag_enabled("MAPFORGE_VK_DEBUG");

    while (!app.input.quit) {
        double frame_begin = time_now_seconds();
        memset(&app.frame_timings, 0, sizeof(app.frame_timings));
        input_begin_frame(&app.input);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input_handle_event(&app.input, &event);
        }
        double after_events = time_now_seconds();

        if (app.input.toggle_debug_pressed) {
            app.overlay.enabled = !app.overlay.enabled;
        }
        if (app.input.toggle_single_line_pressed) {
            app.single_line = !app.single_line;
        }
        if (app.input.toggle_region_pressed) {
            app.region_index = (app.region_index + 1) % region_count();
            const RegionInfo *info = region_get(app.region_index);
            if (info) {
                app.region = *info;
                region_load_meta(info, &app.region);
                for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
                    tile_manager_shutdown(&app.tile_managers[i]);
                    tile_manager_init(&app.tile_managers[i], 256, app.region.tiles_dir);
                }
                tile_loader_shutdown(&app.tile_loader);
                tile_loader_init(&app.tile_loader, app.region.tiles_dir);
                vk_tile_cache_clear_with_renderer(&app.vk_tile_cache, app.renderer.vk);
                app_clear_tile_queue(&app);
                app.visible_valid = false;
                app.loading_expected = 0;
                app.loading_done = 0;
                app.loading_no_data_time = 0.0f;
                app.tile_request_id += 1;
                app_load_route_graph(&app);
                route_state_clear(&app.route);
                app_route_worker_clear(&app);
                app_playback_reset(&app);
                app.building_zoom_bias = app_building_zoom_bias_for_region(&app.region);
                app.road_zoom_bias = app_road_zoom_bias_for_region(&app.region);
                app_center_camera_on_region(&app.camera, &app.region, app.width, app.height);
            }
        }
        if (app.input.toggle_profile_pressed) {
            app.route.fastest = !app.route.fastest;
            if (app.route.has_start && app.route.has_goal) {
                app_route_schedule_recompute(&app, 0.0);
            }
        }
        if (app.input.toggle_landuse_pressed) {
            app.show_landuse = !app.show_landuse;
        }
        if (app.input.toggle_building_fill_pressed) {
            app.building_fill_enabled = !app.building_fill_enabled;
        }
        if (app.input.toggle_polygon_outline_pressed) {
            app.polygon_outline_only = !app.polygon_outline_only;
        }
        if (app.input.theme_cycle_next_pressed) {
            mapforge_shared_theme_cycle_next();
            mapforge_shared_theme_save_persisted();
        }
        if (app.input.theme_cycle_prev_pressed) {
            mapforge_shared_theme_cycle_prev();
            mapforge_shared_theme_save_persisted();
        }
        if (app.input.toggle_playback_pressed && app.route.path.count >= 2) {
            app.playback_playing = !app.playback_playing;
        }
        if (app.input.playback_step_forward && app.route.path.total_time_s > 0.0f) {
            app.playback_time_s += 5.0f;
            if (app.playback_time_s > app.route.path.total_time_s) {
                app.playback_time_s = app.route.path.total_time_s;
            }
        }
        if (app.input.playback_step_back && app.route.path.total_time_s > 0.0f) {
            app.playback_time_s -= 5.0f;
            if (app.playback_time_s < 0.0f) {
                app.playback_time_s = 0.0f;
            }
        }
        if (app.input.playback_speed_up) {
            app.playback_speed = app_next_playback_speed(app.playback_speed, 1);
        }
        if (app.input.playback_speed_down) {
            app.playback_speed = app_next_playback_speed(app.playback_speed, -1);
        }

        double now = time_now_seconds();
        float dt = (float)(now - last_time);
        last_time = now;

        if (!app.dragging_start && !app.dragging_goal && app.input.left_click_pressed) {
            bool over_start = app.route.has_start && app_mouse_over_node(&app, app.route.start_node, 7.0f);
            bool over_goal = app.route.has_goal && app_mouse_over_node(&app, app.route.goal_node, 7.0f);
            if (over_goal && !over_start) {
                app.dragging_goal = true;
            } else if (over_start) {
                app.dragging_start = true;
            }
        }
        if (!app.dragging_goal && app.input.right_click_pressed && app.route.has_goal &&
            app_mouse_over_node(&app, app.route.goal_node, 7.0f)) {
            app.dragging_goal = true;
        }

        bool over_start = app.route.has_start && app_mouse_over_node(&app, app.route.start_node, 7.0f);
        bool over_goal = app.route.has_goal && app_mouse_over_node(&app, app.route.goal_node, 7.0f);
        bool allow_mouse_pan = !(app.dragging_start || app.dragging_goal) &&
            !((app.input.mouse_buttons & SDL_BUTTON_LMASK) && (over_start || over_goal));
        camera_handle_input(&app.camera, &app.input, app.width, app.height, dt, allow_mouse_pan);
        camera_update(&app.camera, dt);
        debug_overlay_update(&app.overlay, dt);

        app_update_hover(&app);
        double after_update = time_now_seconds();

        app_route_poll_result(&app);
        app_update_tile_queue(&app);
        double after_queue = time_now_seconds();
        uint32_t integrate_budget = app.active_layer_valid
            ? app_tile_integrate_budget(app.active_layer_kind, app.active_layer_expected)
            : APP_TILE_INTEGRATE_BUDGET;
        app_drain_tile_results(&app, integrate_budget);
        app_vk_poly_prep_drain(
            &app,
            APP_VK_POLY_PREP_INTEGRATE_BUDGET,
            APP_VK_POLY_PREP_INTEGRATE_TIME_SLICE_SEC);
        app_process_vk_asset_queue(&app, APP_VK_ASSET_BUILD_BUDGET, APP_VK_ASSET_BUILD_TIME_SLICE_SEC);
        app_refresh_layer_states(&app);
        app_update_vk_line_budget(&app);
        if (app.input.copy_overlay_pressed) {
            app_copy_overlay_text(&app);
        }
        double after_integrate = time_now_seconds();

        bool consumed_click = false;
        if (app.input.left_click_pressed && app_header_button_hit(&app, app.input.mouse_x, app.input.mouse_y)) {
            app.route.mode = (app.route.mode == ROUTE_MODE_CAR) ? ROUTE_MODE_WALK : ROUTE_MODE_CAR;
            if (app.route.has_start && app.route.has_goal) {
                app_route_schedule_recompute(&app, 0.0);
            }
            consumed_click = true;
        } else if ((app.input.left_click_pressed || app.input.right_click_pressed || app.input.middle_click_pressed) &&
                   app.input.mouse_y <= (int)APP_HEADER_HEIGHT) {
            consumed_click = true;
        }

        if (!consumed_click && (app.input.left_click_pressed || app.input.right_click_pressed || app.input.middle_click_pressed)) {
            if (app.input.middle_click_pressed) {
                route_state_clear(&app.route);
                app_playback_reset(&app);
                app.dragging_start = false;
                app.dragging_goal = false;
            } else if (app.route.loaded) {
                float world_x = 0.0f;
                float world_y = 0.0f;
                camera_screen_to_world(&app.camera, (float)app.input.mouse_x, (float)app.input.mouse_y, app.width, app.height, &world_x, &world_y);
                if (app.input.left_click_pressed) {
                    if (app.has_hover && app.route.has_start && app.hover_node == app.route.start_node) {
                        app.dragging_start = true;
                    } else if (app.input.shift_down) {
                        uint32_t node = 0;
                        if (app_is_near_node(&app, world_x, world_y, &node)) {
                            app.route.start_node = node;
                            app.route.has_start = true;
                        }
                    }
                }
                if (app.input.right_click_pressed) {
                    if (app.has_hover && app.route.has_goal && app.hover_node == app.route.goal_node) {
                        app.dragging_goal = true;
                    } else {
                        uint32_t node = 0;
                        if (app_is_near_node(&app, world_x, world_y, &node)) {
                            app.route.goal_node = node;
                            app.route.has_goal = true;
                        }
                    }
                }
            }
        }
        if (app.input.enter_pressed && app.route.has_start && app.route.has_goal) {
            app_route_schedule_recompute(&app, 0.0);
        }

        if (app.dragging_start || app.dragging_goal) {
            uint32_t node = 0;
            if (app.has_hover) {
                node = app.hover_node;
            }
            if (app.has_hover) {
                bool changed = false;
                if (app.dragging_start && node != app.route.start_node) {
                    app.route.start_node = node;
                    app.route.has_start = true;
                    changed = true;
                }
                if (app.dragging_goal && node != app.route.goal_node) {
                    app.route.goal_node = node;
                    app.route.has_goal = true;
                    changed = true;
                }
                if (changed && app.route.has_start && app.route.has_goal) {
                    app_route_schedule_recompute(&app, APP_ROUTE_DRAG_DEBOUNCE_SEC);
                }
            }
        }

        if (app.input.left_click_released) {
            if (app.dragging_start) {
                app.dragging_start = false;
                if (app.route.has_start && app.route.has_goal) {
                    app_route_schedule_recompute(&app, 0.0);
                }
            }
            if (app.dragging_goal) {
                app.dragging_goal = false;
                if (app.route.has_start && app.route.has_goal) {
                    app_route_schedule_recompute(&app, 0.0);
                }
            }
        }
        if (app.input.right_click_released) {
            if (app.dragging_goal) {
                app.dragging_goal = false;
                if (app.route.has_start && app.route.has_goal) {
                    app_route_schedule_recompute(&app, 0.0);
                }
            }
        }
        app_route_poll_result(&app);
        double after_route = time_now_seconds();

        app_playback_update(&app, dt);

        renderer_begin_frame(&app.renderer);
        RendererBackend frame_backend = renderer_get_backend(&app.renderer);
        if (frame_backend != last_backend) {
            ui_font_invalidate_cache(&app.renderer);
            app.vk_assets_enabled = frame_backend == RENDERER_BACKEND_VULKAN;
            if (!app.vk_assets_enabled) {
                vk_tile_cache_clear_with_renderer(&app.vk_tile_cache, app.renderer.vk);
                app_vk_poly_prep_clear(&app);
                app_vk_asset_queue_clear(&app);
            }
            last_backend = frame_backend;
        }
        {
            MapForgeThemePalette palette = {0};
            if (mapforge_shared_theme_resolve_palette(&palette)) {
                renderer_clear(&app.renderer,
                               palette.background_clear.r,
                               palette.background_clear.g,
                               palette.background_clear.b,
                               palette.background_clear.a);
            } else {
                renderer_clear(&app.renderer, 20, 20, 28, 255);
            }
        }
        road_renderer_stats_reset();
        uint32_t visible_tiles = 0;
        visible_tiles = app_draw_visible_tiles(&app);
        RoadRenderStats road_stats = {0};
        road_renderer_stats_get(&road_stats);
        if (app.loading_expected > 0 && app.loading_done == 0) {
            app.loading_no_data_time += dt;
        } else {
            app.loading_no_data_time = 0.0f;
        }
        app_draw_region_bounds(&app);
        route_render_draw(&app.renderer, &app.camera, &app.route.graph, &app.route.path, &app.route.drive_path, &app.route.walk_path,
            app.route.has_start, app.route.start_node,
            app.route.has_goal, app.route.goal_node,
            app.route.has_transfer, app.route.transfer_node);
        app_draw_hover_marker(&app);
        app_draw_playback_marker(&app);
        app_draw_route_panel(&app);
        app_draw_header_bar(&app);
        app_draw_layer_debug(&app);
        debug_overlay_render(&app.overlay, &app.renderer);
        double before_present = time_now_seconds();
        renderer_end_frame(&app.renderer);
        double after_render = time_now_seconds();

        app.overlay.visible_tiles = visible_tiles;
        uint32_t cached_total = 0;
        uint32_t capacity_total = 0;
        for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
            cached_total += tile_manager_count(&app.tile_managers[i]);
            capacity_total += tile_manager_capacity(&app.tile_managers[i]);
        }
        app.overlay.cached_tiles = cached_total;
        app.overlay.cache_capacity = capacity_total;

        if (app.overlay.enabled) {
            char title[128];
            const char *profile = app.route.fastest ? "fastest" : "shortest";
            const char *tier = road_renderer_zoom_tier_label(app.camera.zoom);
            const char *mode = app.route.mode == ROUTE_MODE_CAR ? "car" : "walk";
            if (app.route.path.count > 1) {
                float km = app.route.path.total_length_m / 1000.0f;
                float minutes = app.route.path.total_time_s / 60.0f;
                if (app.route.drive_path.count > 1 && app.route.walk_path.count > 1) {
                    float drive_minutes = app.route.drive_path.total_time_s / 60.0f;
                    float walk_minutes = app.route.walk_path.total_time_s / 60.0f;
                    snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | drive %.1f min + walk %.1f min | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                             app.region.name, mode, profile, tier, km, drive_minutes, walk_minutes, app.playback_speed, app.overlay.fps, app.overlay.visible_tiles, app.overlay.cached_tiles, app.overlay.cache_capacity);
                } else {
                    snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | %.1f min | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                             app.region.name, mode, profile, tier, km, minutes, app.playback_speed, app.overlay.fps, app.overlay.visible_tiles, app.overlay.cached_tiles, app.overlay.cache_capacity);
                }
            } else {
                const char *graph_status = app.route.loaded ? "graph ok" : "graph missing";
                snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %s | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                         app.region.name, mode, profile, tier, graph_status, app.playback_speed, app.overlay.fps, app.overlay.visible_tiles, app.overlay.cached_tiles, app.overlay.cache_capacity);
            }
            SDL_SetWindowTitle(app.window, title);
        } else {
            SDL_SetWindowTitle(app.window, "MapForge");
        }

        if (app.loading_done != last_loading_done) {
            last_loading_done = app.loading_done;
            last_loading_progress_time = after_render;
        }

        app.frame_timings.frame_ms = (after_render - frame_begin) * 1000.0;
        app.frame_timings.events_ms = (after_events - frame_begin) * 1000.0;
        app.frame_timings.update_ms = (after_update - after_events) * 1000.0;
        app.frame_timings.queue_ms = (after_queue - after_update) * 1000.0;
        app.frame_timings.integrate_ms = (after_integrate - after_queue) * 1000.0;
        app.frame_timings.route_ms = (after_route - after_integrate) * 1000.0;
        app.frame_timings.render_ms = (before_present - after_route) * 1000.0;
        app.frame_timings.present_ms = (after_render - before_present) * 1000.0;
        if (app.trace_enabled) {
            double rel_time_s = after_render - app.trace_start_time;
            app_trace_emit_frame_samples(&app, rel_time_s);
            app_trace_emit_queue_markers(&app, rel_time_s);
        }
        double frame_ms = app.frame_timings.frame_ms;
        double events_ms = app.frame_timings.events_ms;
        bool long_frame = frame_ms >= 120.0;
        bool stuck_loading = app.loading_expected > 0 &&
            app.loading_done < app.loading_expected &&
            (after_render - last_loading_progress_time) >= 1.5;
        if (vk_debug_logs && (long_frame || stuck_loading || after_render >= perf_next_log)) {
            TileLoaderStats stats = {0};
            tile_loader_get_stats(&app.tile_loader, &stats);
            if (renderer_get_backend(&app.renderer) == RENDERER_BACKEND_VULKAN) {
                VkTileCacheStats vk_asset_stats = {0};
                VkPolyPrepStats poly_prep_stats = {0};
                vk_tile_cache_get_stats(&app.vk_tile_cache, &vk_asset_stats);
                app_vk_poly_prep_get_stats(&app, &poly_prep_stats);
                uint32_t drawn_major = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_MOTORWAY, ROAD_CLASS_TERTIARY);
                uint32_t drawn_local = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_RESIDENTIAL, ROAD_CLASS_SERVICE);
                uint32_t drawn_path = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_FOOTWAY, ROAD_CLASS_PATH);
                uint32_t filt_major = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_MOTORWAY, ROAD_CLASS_TERTIARY);
                uint32_t filt_local = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_RESIDENTIAL, ROAD_CLASS_SERVICE);
                uint32_t filt_path = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_FOOTWAY, ROAD_CLASS_PATH);
                log_info("perf region=%s backend=vk frame=%.1fms events=%.1f update=%.1f queue=%.1f integrate=%.1f route=%.1f render=%.1f present=%.1f zoom=%.2f vis=%u load=%u/%u active=%s "
                         "band_target(a=%s l=%s) band_vis(c=%u/%u m=%u/%u f=%u/%u d=%u/%u) band_q(c=%u m=%u f=%u d=%u) band_fallback=%u "
                         "req=%u/%u res=%u/%u enq=%llu drop=%llu evict=%llu out=%llu out_drop=%llu out_evict=%llu miss=%llu ok=%llu fail=%llu "
                         "vk_begin=%d vk_begin_fail_total=%llu vk_recreate=%u vk_geom=%u/%u vk_geom_skip=%u vk_lines=%u vk_line_skip=%u vk_line_budget=%u vk_rect=%u vk_fill=%u "
                         "vk_assets=%u/%u builds=%u evict=%u miss=%u jobs(q=%u build=%llu drop=%llu evict=%llu) poly_prep(in=%u out=%u enq=%llu done=%llu drop=%llu) resident(a=%u l=%u) fill_resident(w=%u p=%u l=%u b=%u) "
                         "mesh(v=%llu b=%llu fail=%u fill_fail=%u) vk_poly_fill(draw=%u skip=%u fail=%u idx=%u) "
                         "road_draw(m=%u l=%u p=%u) road_filter(m=%u l=%u p=%u)",
                         app.region.name,
                         frame_ms, events_ms, app.frame_timings.update_ms,
                         app.frame_timings.queue_ms, app.frame_timings.integrate_ms,
                         app.frame_timings.route_ms, app.frame_timings.render_ms, app.frame_timings.present_ms,
                         app.camera.zoom,
                         app.visible_tile_count,
                         app.loading_done, app.loading_expected,
                         app.active_layer_valid ? layer_policy_label(app.active_layer_kind) : "none",
                         layer_policy_band_label(app.layer_target_band[TILE_LAYER_ROAD_ARTERY]),
                         layer_policy_band_label(app.layer_target_band[TILE_LAYER_ROAD_LOCAL]),
                         app.band_visible_loaded[TILE_BAND_COARSE], app.band_visible_expected[TILE_BAND_COARSE],
                         app.band_visible_loaded[TILE_BAND_MID], app.band_visible_expected[TILE_BAND_MID],
                         app.band_visible_loaded[TILE_BAND_FINE], app.band_visible_expected[TILE_BAND_FINE],
                         app.band_visible_loaded[TILE_BAND_DEFAULT], app.band_visible_expected[TILE_BAND_DEFAULT],
                         app.band_queue_depth[TILE_BAND_COARSE], app.band_queue_depth[TILE_BAND_MID],
                         app.band_queue_depth[TILE_BAND_FINE], app.band_queue_depth[TILE_BAND_DEFAULT],
                         app.vk_road_band_fallback_draws,
                         stats.req_count, stats.req_capacity,
                         stats.res_count, stats.res_capacity,
                         (unsigned long long)stats.enqueued_count,
                         (unsigned long long)stats.enqueue_drop_count,
                         (unsigned long long)stats.enqueue_evict_count,
                         (unsigned long long)stats.produced_count,
                         (unsigned long long)stats.result_drop_count,
                         (unsigned long long)stats.result_evict_count,
                         (unsigned long long)stats.missing_count,
                         (unsigned long long)stats.load_ok_count,
                         (unsigned long long)stats.load_fail_count,
                         app.renderer.vk_last_begin_result,
                         (unsigned long long)app.renderer.vk_begin_failures_total,
                         app.renderer.vk_swapchain_recreates,
                         app.renderer.vk_geom_used,
                         app.renderer.vk_geom_budget,
                         app.renderer.vk_geom_budget_skips,
                         app.renderer.vk_lines_drawn,
                         app.renderer.vk_line_budget_skips,
                         app.renderer.vk_line_budget,
                         app.renderer.vk_rects_drawn,
                         app.renderer.vk_rects_filled,
                         vk_asset_stats.count,
                         vk_asset_stats.capacity,
                         vk_asset_stats.builds,
                         vk_asset_stats.evictions,
                         app.vk_asset_misses,
                         app.vk_asset_job_count,
                         (unsigned long long)app.vk_asset_job_build_count,
                         (unsigned long long)app.vk_asset_job_drop_count,
                         (unsigned long long)app.vk_asset_job_evict_count,
                         poly_prep_stats.in_count,
                         poly_prep_stats.out_count,
                         (unsigned long long)poly_prep_stats.enqueued_count,
                         (unsigned long long)poly_prep_stats.done_count,
                         (unsigned long long)poly_prep_stats.drop_count,
                         vk_asset_stats.resident_artery,
                         vk_asset_stats.resident_local,
                         vk_asset_stats.resident_fill_water,
                         vk_asset_stats.resident_fill_park,
                         vk_asset_stats.resident_fill_landuse,
                         vk_asset_stats.resident_fill_building,
                         (unsigned long long)vk_asset_stats.mesh_vertices,
                         (unsigned long long)vk_asset_stats.mesh_bytes,
                         vk_asset_stats.mesh_build_failures,
                         vk_asset_stats.fill_mesh_build_failures,
                         app.vk_poly_fill_drawn,
                         app.vk_poly_fill_skip,
                         app.vk_poly_fill_fail,
                         app.vk_poly_fill_indices,
                         drawn_major, drawn_local, drawn_path,
                         filt_major, filt_local, filt_path);
            } else {
                log_info("perf region=%s backend=sdl frame=%.1fms events=%.1f update=%.1f queue=%.1f integrate=%.1f route=%.1f render=%.1f present=%.1f zoom=%.2f vis=%u load=%u/%u active=%s "
                         "band_target(a=%s l=%s) band_vis(c=%u/%u m=%u/%u f=%u/%u d=%u/%u) band_q(c=%u m=%u f=%u d=%u) band_fallback=%u "
                         "req=%u/%u res=%u/%u enq=%llu drop=%llu evict=%llu out=%llu out_drop=%llu out_evict=%llu miss=%llu ok=%llu fail=%llu",
                         app.region.name,
                         frame_ms, events_ms, app.frame_timings.update_ms,
                         app.frame_timings.queue_ms, app.frame_timings.integrate_ms,
                         app.frame_timings.route_ms, app.frame_timings.render_ms, app.frame_timings.present_ms,
                         app.camera.zoom,
                         app.visible_tile_count,
                         app.loading_done, app.loading_expected,
                         app.active_layer_valid ? layer_policy_label(app.active_layer_kind) : "none",
                         layer_policy_band_label(app.layer_target_band[TILE_LAYER_ROAD_ARTERY]),
                         layer_policy_band_label(app.layer_target_band[TILE_LAYER_ROAD_LOCAL]),
                         app.band_visible_loaded[TILE_BAND_COARSE], app.band_visible_expected[TILE_BAND_COARSE],
                         app.band_visible_loaded[TILE_BAND_MID], app.band_visible_expected[TILE_BAND_MID],
                         app.band_visible_loaded[TILE_BAND_FINE], app.band_visible_expected[TILE_BAND_FINE],
                         app.band_visible_loaded[TILE_BAND_DEFAULT], app.band_visible_expected[TILE_BAND_DEFAULT],
                         app.band_queue_depth[TILE_BAND_COARSE], app.band_queue_depth[TILE_BAND_MID],
                         app.band_queue_depth[TILE_BAND_FINE], app.band_queue_depth[TILE_BAND_DEFAULT],
                         app.vk_road_band_fallback_draws,
                         stats.req_count, stats.req_capacity,
                         stats.res_count, stats.res_capacity,
                         (unsigned long long)stats.enqueued_count,
                         (unsigned long long)stats.enqueue_drop_count,
                         (unsigned long long)stats.enqueue_evict_count,
                         (unsigned long long)stats.produced_count,
                         (unsigned long long)stats.result_drop_count,
                         (unsigned long long)stats.result_evict_count,
                         (unsigned long long)stats.missing_count,
                         (unsigned long long)stats.load_ok_count,
                         (unsigned long long)stats.load_fail_count);
            }
            perf_next_log = after_render + 1.0;
        }
    }

    app_shutdown(&app);
    return 0;
}
