#include "app/app_s5_diagnostics.h"
#include "app/terrain.h"
#include "app/terrain_vectors.h"
#include "app/app_internal.h"
#include "app/app_region_transition.h"
#include "app/app_runtime_ingest_internal.h"
#include "app/app_utility_workspace.h"
#include "app/history_ui.h"
#include "app/region_prepare.h"
#include "core/time.h"
#include "map/mercator.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

// Holds one process-local disposable diagnostic run with a fixed 256 MiB capture ceiling.
static struct {
    double start;
    unsigned phase, last_phase, capture, frames;
    unsigned long long capture_bytes;
    FILE *csv;
    bool initialized;
} s5;

// Detects the explicitly requested S5 corpus; ordinary sessions have no diagnostic side effects.
static bool s5_enabled(void) {
    const char *mode = getenv("MAPFORGE_VIEWPORT_SCENARIO");
    return mode && strcmp(mode, "s5") == 0;
}

// Replays two identical passes, slow zoom, reversals, pan, layer toggles and resize.
void app_s5_update(struct AppState *app, double now) {
    if (!app || !s5_enabled())
        return;
    if (!s5.initialized) {
        s5.initialized = true;
        s5.start = now;
        s5.last_phase = UINT32_MAX;
        const char *path = getenv("MAPFORGE_S5_CSV");
        if (path)
            s5.csv = fopen(path, "w");
        if (s5.csv)
            fputs("frame,time,phase,zoom,x,y,frame_ms,update_ms,queue_ms,integrate_ms,render_ms,present_ms,"
                  "cpu_tile_bytes,gpu_mesh_bytes,rss_peak_bytes,ideal,renderable,missing,vk_draw,cpu_draw,"
                  "blend,builds,evictions,capture,l0_pending_age_ms,l0_pending,upload_queue,cpu_budget_"
                  "refusals,gpu_budget_refusals,hold_hits,cpu_evictions,fill_failures,terrain_area,terrain_coarse_ready,terrain_coarse_needed,terrain_detail_ready,terrain_detail_needed,terrain_pending,terrain_resident_tiles,terrain_gpu_bytes,terrain_worker_slots,contour_lines\n",
                  s5.csv);
    }
    double elapsed = now - s5.start;
    const char *passes_env = getenv("MAPFORGE_S5_PASSES");
    unsigned passes = passes_env ? (unsigned)atoi(passes_env) : 2u;
    if (passes < 1u || passes > 4u)
        passes = 2u;
    if (elapsed >= passes * 32.0) {
        app->ui_state_bridge.input.quit = true;
        return;
    }
    if (getenv("MAPFORGE_S5_REGION_ONLY")) {
        if (elapsed > 45) {
            fprintf(stderr, "[s5] region switch failed: preparation timeout\n");
            app->ui_state_bridge.input.quit = true;
            return;
        }
        if (app->region_prepare)
            return;
        unsigned phase = s5.last_phase == UINT32_MAX ? 0u : s5.last_phase - 15u;
        if (phase > 0u) {
            const char *expected = phase == 1u ? "seattle" : "north_seattle_service_area";
            if (!app->region.name || strcmp(app->region.name, expected)) {
                fprintf(stderr, "[s5] region switch failed: %s\n", app->ingest_status);
                app->ui_state_bridge.input.quit = true;
                return;
            }
        }
        if (phase >= 2u) {
            app->ui_state_bridge.input.quit = true;
            return;
        }
        s5.phase = s5.last_phase = 16u + phase;
        app->ingest_active_count = 1;
        app->ingest_selected_active = 0;
        snprintf(app->ingest_active_regions[0], sizeof(app->ingest_active_regions[0]), "%s",
                 phase == 0u ? "seattle" : "north_seattle_service_area");
        if (!app_ingest_open_selected_active_region(app)) {
            fprintf(stderr, "[s5] region switch failed: %s\n", app->ingest_status);
            app->ui_state_bridge.input.quit = true;
        }
        return;
    }
    double t = fmod(elapsed, 32.0);
    s5.phase = (unsigned)(elapsed / 32.0) * 8u + (unsigned)(t / 4.0);
    // An explicit disposable S7 replay exercises production history controls without personal mutations.
    if (getenv("MAPFORGE_S5_HISTORY")) {
        app->ui_state_bridge.utility_workspace = APP_UTILITY_HIKING_HISTORY;
        if (s5.phase != s5.last_phase) {
            s5.last_phase = s5.phase;
            unsigned step = s5.phase % 8;
            if (step == 0) {
                const char *id = getenv("MAPFORGE_S5_HISTORY_ID");
                if (id)
                    snprintf(app->ui_state_bridge.history_selected_id, 64, "%s", id);
            }
            if (getenv("MAPFORGE_S9_VECTORS") && (step==0 || step==5)) terrain_set_style(0);
            SDL_FRect p = app->ui_state_bridge.left_pane_rect;
            if (p.w < 180)
                p = (SDL_FRect){0, 68, 360, app->height - 68};
            SDL_Event e = {0};
            e.type = SDL_MOUSEBUTTONDOWN;
            e.button.button = SDL_BUTTON_LEFT;
            int button = step == 2 ? 2 : step == 4 ? (getenv("MAPFORGE_S8_TERRAIN")?10:3) : step == 5 ? 8 : step == 6 ? 4 : 1;
            if (button == 4 || button == 8) {
                SDL_FRect toggle=history_ui_action_rect(app,button==4?12:13);
                e.button.x=toggle.x+5;e.button.y=toggle.y+5;history_ui_event(app,&e);e.type=SDL_MOUSEBUTTONUP;history_ui_event(app,&e);e.type=SDL_MOUSEBUTTONDOWN;
            }
            SDL_FRect action=history_ui_action_rect(app,button);
            e.button.x=action.x+5;e.button.y=action.y+5;

            if (step == 1) {
                if(getenv("MAPFORGE_S8_TERRAIN")) {app->view_state_bridge.camera.heading_rad=.55f;app->view_state_bridge.camera.heading_target_rad=.55f;}
                app->view_state_bridge.camera.x += 100;
                app->view_state_bridge.camera.x_target = app->view_state_bridge.camera.x;
                app->view_state_bridge.camera.zoom += 1;
                app->view_state_bridge.camera.zoom_target = app->view_state_bridge.camera.zoom;
            } else if (step == 3)
                SDL_SetWindowSize(app->window, 900, 760);
            else if (step == 7) {
                e.type = SDL_KEYDOWN;
                e.key.keysym.sym = SDLK_ESCAPE;
                history_ui_event(app, &e);
                SDL_SetWindowSize(app->window, 1280, 720);
            } else {
                history_ui_event(app, &e);e.type=SDL_MOUSEBUTTONUP;history_ui_event(app,&e);e.type=SDL_MOUSEBUTTONDOWN;
            }
            if(getenv("MAPFORGE_S8_TERRAIN")&&step==4){SDL_FRect fit=history_ui_action_rect(app,1);e.button.x=fit.x+5;e.button.y=fit.y+5;history_ui_event(app,&e);e.type=SDL_MOUSEBUTTONUP;history_ui_event(app,&e);e.type=SDL_MOUSEBUTTONDOWN;}
        }
        return;
    }
    float zoom = 13.0f;
    if (t < 4)
        zoom = 13;
    else if (t < 12)
        zoom = 13 + (float)((t - 4) * 0.5);
    else if (t < 16)
        zoom = 17;
    else if (t < 20)
        zoom = ((unsigned)((t - 16) * 2) % 2) ? 15.1f : 13.9f;
    else if (t < 24)
        zoom = 15;
    else if (t < 28)
        zoom = 16;
    else
        zoom = 17;
    MercatorMeters origin = mercator_from_latlon((LatLon){47.66, -122.30});
    Camera *camera = &app->view_state_bridge.camera;
    camera->x = camera->x_target = (float)origin.x + (t >= 20 && t < 24 ? sinf((float)t * 2) * 600 : 0);
    camera->y = camera->y_target = (float)origin.y;
    camera->zoom = camera->zoom_target = zoom;
    camera->heading_rad = camera->heading_target_rad = 0;
    app->view_state_bridge.layer_user_enabled[TILE_LAYER_POLY_BUILDING] = !(t >= 24 && t < 26);
    if (s5.phase != s5.last_phase) {
        s5.last_phase = s5.phase;
        if (t >= 28)
            SDL_SetWindowSize(app->window, 1100, 760);
        else if (t < 4)
            SDL_SetWindowSize(app->window, 1280, 720);
    }
    app->ui_state_bridge.input.mouse_x = 800;
    app->ui_state_bridge.input.mouse_y = 400;
}

// Uses the shared Vulkan capture API with at most 16 frames and an explicit byte estimate ceiling.
void app_s5_capture(struct AppState *app) {
    if (!app || !s5_enabled())
        return;
    const char *root = getenv("MAPFORGE_S5_CAPTURE_PREFIX");
    double elapsed = time_now_seconds() - s5.start;
    if (!root || s5.capture >= 16 || elapsed < 1.0 + s5.capture * 4.0)
        return;
    unsigned long long bytes =
        (unsigned long long)app->renderer.surface_width * app->renderer.surface_height * 16u + 256;
#if defined(MAPFORGE_HAVE_VK)
    if (app->renderer.vk) {
        VkExtent2D extent = ((VkRenderer *)app->renderer.vk)->context.swapchain.extent;
        bytes = (unsigned long long)extent.width * extent.height * 4u + 256;
    }
#endif
    if (bytes > 256ull * 1024 * 1024 - s5.capture_bytes)
        return;
#if defined(MAPFORGE_HAVE_VK)
    if (app->renderer.vk) {
        char path[1024];
        int size = snprintf(path, sizeof(path), "%s-%02u.bmp", root, s5.capture);
        if (size > 0 && (size_t)size < sizeof(path) &&
            vk_renderer_request_capture(app->renderer.vk, path) == VK_SUCCESS)
            s5.capture_bytes += bytes;
    }
#endif
    s5.capture++;
}

// Counts decoded array storage separately from retained GPU buffer payload and process peak RSS.
void app_s5_measure(struct AppState *app) {
    if (!app || !s5_enabled() || !s5.csv || s5.frames >= 100000)
        return;
    unsigned long long cpu_bytes = 0;
    for (unsigned k = 0; k < TILE_LAYER_COUNT; k++) {
        TileManager *manager = &app->tile_state_bridge.tile_managers[k];
        cpu_bytes += tile_manager_payload_bytes(manager);
    }
    if (app->region_prepare)
        cpu_bytes += region_prepare_progress(app->region_prepare).resident_bytes;
    unsigned long long gpu_bytes =
        app->tile_state_bridge.vk_tile_cache.mesh_bytes + app->region_pending_gpu.mesh_bytes;
    unsigned cpu_refusals = 0;
    for (unsigned k = 0; k < TILE_LAYER_COUNT; ++k)
        cpu_refusals += app->tile_state_bridge.tile_managers[k].budget_refusals;
    struct rusage usage = {0};
    getrusage(RUSAGE_SELF, &usage);
    unsigned long long rss = (unsigned long long)usage.ru_maxrss;
#if !defined(__APPLE__)
    rss *= 1024;
#endif
    fprintf(
        s5.csv,
        "%u,%.5f,%u,%.4f,%.1f,%.1f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%llu,%llu,%llu,%u,%u,%u,%u,%u,%u,%u,%u,%u",
        s5.frames++, time_now_seconds() - s5.start, s5.phase, app->view_state_bridge.camera.zoom,
        app->view_state_bridge.camera.x, app->view_state_bridge.camera.y, app->frame_timings.frame_ms,
        app->frame_timings.update_ms, app->frame_timings.queue_ms, app->frame_timings.integrate_ms,
        app->frame_timings.render_ms, app->frame_timings.present_ms, cpu_bytes, gpu_bytes, rss,
        app->tile_state_bridge.visible_ideal_count, app->tile_state_bridge.visible_renderable_count,
        app->tile_state_bridge.visible_missing_count, app->tile_state_bridge.draw_path_vk_count,
        app->tile_state_bridge.draw_path_fallback_count, app->tile_state_bridge.transition_blend_draw_count,
        app->tile_state_bridge.vk_tile_cache.builds, app->tile_state_bridge.vk_tile_cache.evictions,
        s5.capture);
    fprintf(s5.csv, ",%.4f,%u,%u,%u,%u,%u,%llu,%u", app->tile_state_bridge.lane_l0_latency_ms,
            app->tile_state_bridge.lane_l0_pending, app->worker_state_bridge.vk_asset_job_count, cpu_refusals,
            app->tile_state_bridge.vk_tile_cache.budget_refusals, app->tile_state_bridge.present_hold_hits,
            (unsigned long long)app->tile_state_bridge.cache_evicted_total,
            app->tile_state_bridge.vk_tile_cache.fill_mesh_build_failures);
    TerrainPresentationStats terrain_stats = terrain_presentation_stats();
    fprintf(s5.csv, ",%d,%d,%d,%d,%d,%d,%zu,%zu,%zu,%u\n", terrain_stats.area,
        terrain_stats.coarse_ready, terrain_stats.coarse_needed, terrain_stats.detail_ready,
        terrain_stats.detail_needed, app->terrain_pending, terrain_stats.resident_tiles,
        terrain_stats.gpu_bytes, terrain_stats.worker_slots, terrain_vectors_drawn_lines());
    if (app->ui_state_bridge.input.quit) {
        fclose(s5.csv);
        s5.csv = NULL;
    }
}
