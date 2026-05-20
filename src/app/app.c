#include "app/app.h"
#include "app/app_internal.h"
#include "app/app_persist_state.h"
#include "app/app_trace_runtime.h"
#include "map_forge/map_forge_app_main.h"

#include "core/log.h"
#include "core/time.h"
#include "app/region_loader.h"
#include "map/road_renderer.h"
#include "route/route_render.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"
#include "kit_runtime_diag.h"

#include <SDL.h>
#include <SDL2/SDL_ttf.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
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

int app_run_legacy(void) {
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
    KitRuntimeDiagInputTotals input_totals = {0};

    while (!app.ui_state_bridge.input.quit) {
        AppRuntimeDispatchFrame frame = {0};
        app_runtime_dispatch_frame(&app, &last_time, &last_backend, &frame);
        if (frame.skipped_for_global_controls) {
            continue;
        }
        app_runtime_loop_diag_tick(&frame);

        RoadRenderStats road_stats = {0};
        road_renderer_stats_get(&road_stats);

        if (frame.loading_done != last_loading_done) {
            last_loading_done = frame.loading_done;
            last_loading_progress_time = frame.after_render;
        }

        KitRuntimeDiagStageMarks stage_marks = {
            .frame_begin = frame.frame_begin,
            .after_events = frame.after_events,
            .after_update = frame.after_update,
            .after_queue = frame.after_queue,
            .after_integrate = frame.after_integrate,
            .after_route = frame.after_route,
            .after_render_derive = frame.after_render_derive,
            .before_present = frame.before_present,
            .after_render = frame.after_render,
        };
        KitRuntimeDiagTimings diag_timings = {0};
        kit_runtime_diag_compute_timings(&stage_marks, &diag_timings);

        app.frame_timings.frame_ms = diag_timings.frame_ms;
        app.frame_timings.events_ms = diag_timings.events_ms;
        app.frame_timings.update_ms = diag_timings.update_ms;
        app.frame_timings.queue_ms = diag_timings.queue_ms;
        app.frame_timings.integrate_ms = diag_timings.integrate_ms;
        app.frame_timings.route_ms = diag_timings.route_ms;
        app.frame_timings.render_ms = diag_timings.render_ms;
        app.frame_timings.present_ms = diag_timings.present_ms;
        if (app.trace_enabled) {
            double rel_time_s = frame.after_render - app.trace_start_time;
            app_trace_emit_frame_samples(&app, rel_time_s);
            app_trace_emit_queue_markers(&app, rel_time_s);
        }
        double frame_ms = app.frame_timings.frame_ms;
        double events_ms = app.frame_timings.events_ms;
        double render_derive_ms = diag_timings.render_derive_ms;
        double render_submit_ms = diag_timings.render_submit_ms;
        bool long_frame = frame_ms >= 120.0;
        bool stuck_loading = frame.loading_expected > 0 &&
            frame.loading_done < frame.loading_expected &&
            (frame.after_render - last_loading_progress_time) >= 1.5;
        KitRuntimeDiagInputFrame input_frame = {
            .raw_event_count = frame.input.raw.sdl_event_count,
            .action_count = frame.input.normalized.action_count,
            .text_entry_gate_active = frame.input.normalized.text_entry_gate_active,
            .ignored_count = frame.input.normalized.ignored_count,
            .routed_global_count = frame.input.route.routed_global_count,
            .routed_pane_count = frame.input.route.routed_pane_count,
            .routed_fallback_count = frame.input.route.routed_fallback_count,
            .target_invalidation_count = frame.input.invalidation.target_invalidation_count,
            .full_invalidation_count = frame.input.invalidation.full_invalidation_count,
            .invalidation_reason_bits = frame.input.invalidation.invalidation_reason_bits,
        };
        kit_runtime_diag_input_totals_accumulate(&input_totals, &input_frame);
        if (vk_debug_logs && (long_frame || stuck_loading || frame.after_render >= perf_next_log)) {
            TileLoaderStats stats = {0};
            TileSourceRuntimeStats source_stats = {0};
            tile_loader_get_stats(&app.tile_state_bridge.tile_loader, &stats);
            tile_source_runtime_stats_get(&source_stats);
            if (renderer_get_backend(&app.renderer) == RENDERER_BACKEND_VULKAN) {
                VkTileCacheStats vk_asset_stats = {0};
                VkPolyPrepStats poly_prep_stats = {0};
                vk_tile_cache_get_stats(&app.tile_state_bridge.vk_tile_cache, &vk_asset_stats);
                app_vk_poly_prep_get_stats(&app, &poly_prep_stats);
                uint32_t drawn_major = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_MOTORWAY, ROAD_CLASS_TERTIARY);
                uint32_t drawn_local = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_RESIDENTIAL, ROAD_CLASS_SERVICE);
                uint32_t drawn_path = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_FOOTWAY, ROAD_CLASS_PATH);
                uint32_t filt_major = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_MOTORWAY, ROAD_CLASS_TERTIARY);
                uint32_t filt_local = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_RESIDENTIAL, ROAD_CLASS_SERVICE);
                uint32_t filt_path = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_FOOTWAY, ROAD_CLASS_PATH);
                log_info("perf region=%s backend=vk frame=%.1fms events=%.1f update=%.1f queue=%.1f integrate=%.1f route=%.1f render=%.1f present=%.1f rderive=%.1f rsubmit=%.1f draw_pass=%u zoom=%.2f vis=%u viewset(i=%u r=%u m=%u) load=%u/%u active=%s "
                         "input(frame_raw=%u frame_actions=%u gate=%u route_g=%u route_p=%u route_f=%u inval_t=%u inval_f=%u inval_bits=0x%x) "
                         "input(total_raw=%llu total_actions=%llu total_gated=%llu total_route(g=%llu p=%llu f=%llu) total_inval(t=%llu f=%llu)) "
                         "band_target(a=%s l=%s) band_vis(c=%u/%u m=%u/%u f=%u/%u d=%u/%u) band_q(c=%u m=%u f=%u d=%u) band_fallback=%u "
                         "life(frame=%llu tx=%u bad=%u req=%u cpu=%u gpu=%u ren=%u stale=%u ideal=%u fallback=%u bad_total=%llu) "
                         "req=%u/%u res=%u/%u enq=%llu drop=%llu evict=%llu out=%llu out_drop=%llu out_evict=%llu miss=%llu ok=%llu fail=%llu "
                         "src=%s arch(req=%llu hit=%llu ext=%llu fail=%llu tree=%llu) "
                         "vk_begin=%d vk_begin_fail_total=%llu vk_recreate=%u vk_geom=%u/%u vk_geom_skip=%u vk_lines=%u vk_line_skip=%u vk_line_budget=%u vk_rect=%u vk_fill=%u "
                         "vk_assets=%u/%u builds=%u evict=%u miss=%u jobs(q=%u build=%llu drop=%llu evict=%llu) "
                         "poly_prep(in=%u out=%u enq=%llu done=%llu drop=%llu qj=%llu qp=%llu qb=%llu qm=%llu qd=%llu wind=%llu) "
                         "resident(a=%u l=%u) fill_resident(w=%u p=%u l=%u b=%u) "
                         "mesh(v=%llu b=%llu fail=%u fill_fail=%u) vk_poly_fill(draw=%u skip=%u fail=%u idx=%u) "
                         "road_draw(m=%u l=%u p=%u) road_filter(m=%u l=%u p=%u) draw_path(vk=%u fallback=%u blend=%u) defer(band=%u queue=%u) hold(hit=%u miss=%u upd=%u)",
                         app.region.name,
                         frame_ms, events_ms, app.frame_timings.update_ms,
                         app.frame_timings.queue_ms, app.frame_timings.integrate_ms,
                         app.frame_timings.route_ms, app.frame_timings.render_ms, app.frame_timings.present_ms,
                         render_derive_ms, render_submit_ms, frame.render_draw_pass_count,
                         app.view_state_bridge.camera.zoom,
                         app.tile_state_bridge.visible_tile_count,
                         app.tile_state_bridge.visible_ideal_count,
                         app.tile_state_bridge.visible_renderable_count,
                         app.tile_state_bridge.visible_missing_count,
                         app.tile_state_bridge.loading_done, app.tile_state_bridge.loading_expected,
                         app.tile_state_bridge.active_layer_valid ? layer_policy_label(app.tile_state_bridge.active_layer_kind) : "none",
                         frame.input.raw.sdl_event_count,
                         frame.input.normalized.action_count,
                         frame.input.normalized.text_entry_gate_active ? 1u : 0u,
                         frame.input.route.routed_global_count,
                         frame.input.route.routed_pane_count,
                         frame.input.route.routed_fallback_count,
                         frame.input.invalidation.target_invalidation_count,
                         frame.input.invalidation.full_invalidation_count,
                         frame.input.invalidation.invalidation_reason_bits,
                         (unsigned long long)input_totals.raw_event_count,
                         (unsigned long long)input_totals.action_count,
                         (unsigned long long)input_totals.shortcut_gated_count,
                         (unsigned long long)input_totals.routed_global_count,
                         (unsigned long long)input_totals.routed_pane_count,
                         (unsigned long long)input_totals.routed_fallback_count,
                         (unsigned long long)input_totals.target_invalidation_count,
                         (unsigned long long)input_totals.full_invalidation_count,
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_ARTERY]),
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_LOCAL]),
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_COARSE], app.tile_state_bridge.band_visible_expected[TILE_BAND_COARSE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_MID], app.tile_state_bridge.band_visible_expected[TILE_BAND_MID],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_FINE], app.tile_state_bridge.band_visible_expected[TILE_BAND_FINE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_DEFAULT], app.tile_state_bridge.band_visible_expected[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_COARSE], app.tile_state_bridge.band_queue_depth[TILE_BAND_MID],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_FINE], app.tile_state_bridge.band_queue_depth[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.vk_road_band_fallback_draws,
                         (unsigned long long)app.tile_state_bridge.lifecycle_frame_index,
                         app.tile_state_bridge.lifecycle_transition_count,
                         app.tile_state_bridge.lifecycle_invalid_transition_count,
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_REQUESTED],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_DECODED_CPU],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_UPLOADED_GPU],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_RENDERABLE],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_STALE],
                         app.tile_state_bridge.lifecycle_renderable_ideal_count,
                         app.tile_state_bridge.lifecycle_renderable_fallback_count,
                         (unsigned long long)app.tile_state_bridge.lifecycle_invalid_transition_total,
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
                         tile_storage_kind_label(app.region.tile_source.storage_kind),
                         (unsigned long long)source_stats.archive_request_count,
                         (unsigned long long)source_stats.archive_hit_count,
                         (unsigned long long)source_stats.archive_extract_count,
                         (unsigned long long)source_stats.archive_extract_fail_count,
                         (unsigned long long)source_stats.archive_fallback_tree_count,
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
                         app.tile_state_bridge.vk_asset_misses,
                         app.worker_state_bridge.vk_asset_job_count,
                         (unsigned long long)app.worker_state_bridge.vk_asset_job_build_count,
                         (unsigned long long)app.worker_state_bridge.vk_asset_job_drop_count,
                         (unsigned long long)app.worker_state_bridge.vk_asset_job_evict_count,
                         poly_prep_stats.in_count,
                         poly_prep_stats.out_count,
                         (unsigned long long)poly_prep_stats.enqueued_count,
                         (unsigned long long)poly_prep_stats.done_count,
                         (unsigned long long)poly_prep_stats.drop_count,
                         (unsigned long long)poly_prep_stats.quarantine_job_count,
                         (unsigned long long)poly_prep_stats.quarantine_polygon_count,
                         (unsigned long long)poly_prep_stats.quarantine_ring_bounds_count,
                         (unsigned long long)poly_prep_stats.quarantine_ring_min_points_count,
                         (unsigned long long)poly_prep_stats.quarantine_ring_degenerate_count,
                         (unsigned long long)poly_prep_stats.winding_normalized_count,
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
                         app.tile_state_bridge.vk_poly_fill_drawn,
                         app.tile_state_bridge.vk_poly_fill_skip,
                         app.tile_state_bridge.vk_poly_fill_fail,
                         app.tile_state_bridge.vk_poly_fill_indices,
                         drawn_major, drawn_local, drawn_path,
                         filt_major, filt_local, filt_path,
                         app.tile_state_bridge.draw_path_vk_count,
                         app.tile_state_bridge.draw_path_fallback_count,
                         app.tile_state_bridge.transition_blend_draw_count,
                         app.tile_state_bridge.band_switch_deferred_count,
                         app.tile_state_bridge.queue_rebuild_deferred_count,
                         app.tile_state_bridge.present_hold_hits,
                         app.tile_state_bridge.present_hold_misses,
                         app.tile_state_bridge.present_hold_updates);
                log_info("perf_phase_a cov(global=%.3f layer(a=%.3f l=%.3f c=%.3f w=%.3f p=%.3f lu=%.3f b=%.3f)) "
                         "l0(lat_ms=%.2f pending=%u sat=%llu drop=%llu retry=%llu) "
                         "gate(defer=%u timeout=%u) cache(evict=%u total=%llu a=%u/%u l=%u/%u b=%u/%u) "
                         "churn(frame_band=%u frame_rebuild=%u total_band=%llu total_rebuild=%llu) "
                         "poly_layer(job w=%llu p=%llu lu=%llu b=%llu ring w=%llu p=%llu lu=%llu b=%llu) "
                         "budget(load req=%u app=%u clamp=%u ex=%u lane_hit=%u/%u/%u/%u integ req=%u app=%u clamp=%u ex=%u vk_asset=%u/%u sat=%u vk_poly_asset=%u/%u hit=%u)",
                         app.tile_state_bridge.visible_coverage_ratio,
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_CONTOUR],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_WATER],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_PARK],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_LANDUSE],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.lane_l0_latency_ms,
                         app.tile_state_bridge.lane_l0_pending,
                         (unsigned long long)app.tile_state_bridge.lane_l0_saturation_total,
                         (unsigned long long)app.tile_state_bridge.lane_l0_dropped_visible_requests,
                         (unsigned long long)app.tile_state_bridge.lane_l0_retry_visible_requests,
                         app.tile_state_bridge.coverage_gate_deferred_count,
                         app.tile_state_bridge.coverage_gate_timeout_count,
                         app.tile_state_bridge.cache_evicted_frame_total,
                         (unsigned long long)app.tile_state_bridge.cache_evicted_total,
                         app.tile_state_bridge.cache_resident[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.cache_target[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.cache_resident[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.cache_target[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.cache_resident[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.cache_target[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.band_commit_frame_count,
                         app.tile_state_bridge.queue_rebuild_frame_count,
                         (unsigned long long)app.tile_state_bridge.band_commit_total,
                         (unsigned long long)app.tile_state_bridge.queue_rebuild_total,
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_WATER],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_PARK],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_LANDUSE],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_BUILDING],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_WATER],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_PARK],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_LANDUSE],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.budget_frame.load_budget_requested_total,
                         app.tile_state_bridge.budget_frame.load_budget_applied_total,
                         app.tile_state_bridge.budget_frame.load_budget_clamped_count,
                         app.tile_state_bridge.budget_frame.load_budget_exhausted_count,
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L0_VISIBLE_MISSING],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L1_VISIBLE_REFINE],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L2_NEAR_PREFETCH],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L3_FAR_PREFETCH],
                         app.tile_state_bridge.budget_frame.integrate_budget_requested,
                         app.tile_state_bridge.budget_frame.integrate_budget_applied,
                         app.tile_state_bridge.budget_frame.integrate_budget_clamped_count,
                         app.tile_state_bridge.budget_frame.integrate_budget_exhausted_count,
                         app.tile_state_bridge.budget_frame.vk_asset_jobs_built,
                         app.tile_state_bridge.budget_frame.vk_asset_jobs_budget,
                         app.tile_state_bridge.budget_frame.vk_asset_budget_saturated_count,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_used,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_cap,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_hit_count);
            } else {
                VkPolyPrepStats poly_prep_stats = {0};
                app_vk_poly_prep_get_stats(&app, &poly_prep_stats);
                log_info("perf region=%s backend=sdl frame=%.1fms events=%.1f update=%.1f queue=%.1f integrate=%.1f route=%.1f render=%.1f present=%.1f rderive=%.1f rsubmit=%.1f draw_pass=%u zoom=%.2f vis=%u viewset(i=%u r=%u m=%u) load=%u/%u active=%s "
                         "input(frame_raw=%u frame_actions=%u gate=%u route_g=%u route_p=%u route_f=%u inval_t=%u inval_f=%u inval_bits=0x%x) "
                         "input(total_raw=%llu total_actions=%llu total_gated=%llu total_route(g=%llu p=%llu f=%llu) total_inval(t=%llu f=%llu)) "
                         "band_target(a=%s l=%s) band_vis(c=%u/%u m=%u/%u f=%u/%u d=%u/%u) band_q(c=%u m=%u f=%u d=%u) band_fallback=%u "
                         "life(frame=%llu tx=%u bad=%u req=%u cpu=%u gpu=%u ren=%u stale=%u ideal=%u fallback=%u bad_total=%llu) "
                         "req=%u/%u res=%u/%u enq=%llu drop=%llu evict=%llu out=%llu out_drop=%llu out_evict=%llu miss=%llu ok=%llu fail=%llu "
                         "src=%s arch(req=%llu hit=%llu ext=%llu fail=%llu tree=%llu) "
                         "draw_path(vk=%u fallback=%u blend=%u) defer(band=%u queue=%u) hold(hit=%u miss=%u upd=%u)",
                         app.region.name,
                         frame_ms, events_ms, app.frame_timings.update_ms,
                         app.frame_timings.queue_ms, app.frame_timings.integrate_ms,
                         app.frame_timings.route_ms, app.frame_timings.render_ms, app.frame_timings.present_ms,
                         render_derive_ms, render_submit_ms, frame.render_draw_pass_count,
                         app.view_state_bridge.camera.zoom,
                         app.tile_state_bridge.visible_tile_count,
                         app.tile_state_bridge.visible_ideal_count,
                         app.tile_state_bridge.visible_renderable_count,
                         app.tile_state_bridge.visible_missing_count,
                         app.tile_state_bridge.loading_done, app.tile_state_bridge.loading_expected,
                         app.tile_state_bridge.active_layer_valid ? layer_policy_label(app.tile_state_bridge.active_layer_kind) : "none",
                         frame.input.raw.sdl_event_count,
                         frame.input.normalized.action_count,
                         frame.input.normalized.text_entry_gate_active ? 1u : 0u,
                         frame.input.route.routed_global_count,
                         frame.input.route.routed_pane_count,
                         frame.input.route.routed_fallback_count,
                         frame.input.invalidation.target_invalidation_count,
                         frame.input.invalidation.full_invalidation_count,
                         frame.input.invalidation.invalidation_reason_bits,
                         (unsigned long long)input_totals.raw_event_count,
                         (unsigned long long)input_totals.action_count,
                         (unsigned long long)input_totals.shortcut_gated_count,
                         (unsigned long long)input_totals.routed_global_count,
                         (unsigned long long)input_totals.routed_pane_count,
                         (unsigned long long)input_totals.routed_fallback_count,
                         (unsigned long long)input_totals.target_invalidation_count,
                         (unsigned long long)input_totals.full_invalidation_count,
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_ARTERY]),
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_LOCAL]),
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_COARSE], app.tile_state_bridge.band_visible_expected[TILE_BAND_COARSE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_MID], app.tile_state_bridge.band_visible_expected[TILE_BAND_MID],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_FINE], app.tile_state_bridge.band_visible_expected[TILE_BAND_FINE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_DEFAULT], app.tile_state_bridge.band_visible_expected[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_COARSE], app.tile_state_bridge.band_queue_depth[TILE_BAND_MID],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_FINE], app.tile_state_bridge.band_queue_depth[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.vk_road_band_fallback_draws,
                         (unsigned long long)app.tile_state_bridge.lifecycle_frame_index,
                         app.tile_state_bridge.lifecycle_transition_count,
                         app.tile_state_bridge.lifecycle_invalid_transition_count,
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_REQUESTED],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_DECODED_CPU],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_UPLOADED_GPU],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_RENDERABLE],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_STALE],
                         app.tile_state_bridge.lifecycle_renderable_ideal_count,
                         app.tile_state_bridge.lifecycle_renderable_fallback_count,
                         (unsigned long long)app.tile_state_bridge.lifecycle_invalid_transition_total,
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
                         tile_storage_kind_label(app.region.tile_source.storage_kind),
                         (unsigned long long)source_stats.archive_request_count,
                         (unsigned long long)source_stats.archive_hit_count,
                         (unsigned long long)source_stats.archive_extract_count,
                         (unsigned long long)source_stats.archive_extract_fail_count,
                         (unsigned long long)source_stats.archive_fallback_tree_count,
                         app.tile_state_bridge.draw_path_vk_count,
                         app.tile_state_bridge.draw_path_fallback_count,
                         app.tile_state_bridge.transition_blend_draw_count,
                         app.tile_state_bridge.band_switch_deferred_count,
                         app.tile_state_bridge.queue_rebuild_deferred_count,
                         app.tile_state_bridge.present_hold_hits,
                         app.tile_state_bridge.present_hold_misses,
                         app.tile_state_bridge.present_hold_updates);
                log_info("perf_phase_a cov(global=%.3f layer(a=%.3f l=%.3f c=%.3f w=%.3f p=%.3f lu=%.3f b=%.3f)) "
                         "l0(lat_ms=%.2f pending=%u sat=%llu drop=%llu retry=%llu) "
                         "gate(defer=%u timeout=%u) cache(evict=%u total=%llu a=%u/%u l=%u/%u b=%u/%u) "
                         "churn(frame_band=%u frame_rebuild=%u total_band=%llu total_rebuild=%llu) "
                         "poly_layer(job w=%llu p=%llu lu=%llu b=%llu ring w=%llu p=%llu lu=%llu b=%llu) "
                         "budget(load req=%u app=%u clamp=%u ex=%u lane_hit=%u/%u/%u/%u integ req=%u app=%u clamp=%u ex=%u vk_asset=%u/%u sat=%u vk_poly_asset=%u/%u hit=%u)",
                         app.tile_state_bridge.visible_coverage_ratio,
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_CONTOUR],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_WATER],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_PARK],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_LANDUSE],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.lane_l0_latency_ms,
                         app.tile_state_bridge.lane_l0_pending,
                         (unsigned long long)app.tile_state_bridge.lane_l0_saturation_total,
                         (unsigned long long)app.tile_state_bridge.lane_l0_dropped_visible_requests,
                         (unsigned long long)app.tile_state_bridge.lane_l0_retry_visible_requests,
                         app.tile_state_bridge.coverage_gate_deferred_count,
                         app.tile_state_bridge.coverage_gate_timeout_count,
                         app.tile_state_bridge.cache_evicted_frame_total,
                         (unsigned long long)app.tile_state_bridge.cache_evicted_total,
                         app.tile_state_bridge.cache_resident[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.cache_target[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.cache_resident[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.cache_target[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.cache_resident[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.cache_target[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.band_commit_frame_count,
                         app.tile_state_bridge.queue_rebuild_frame_count,
                         (unsigned long long)app.tile_state_bridge.band_commit_total,
                         (unsigned long long)app.tile_state_bridge.queue_rebuild_total,
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_WATER],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_PARK],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_LANDUSE],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_BUILDING],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_WATER],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_PARK],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_LANDUSE],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.budget_frame.load_budget_requested_total,
                         app.tile_state_bridge.budget_frame.load_budget_applied_total,
                         app.tile_state_bridge.budget_frame.load_budget_clamped_count,
                         app.tile_state_bridge.budget_frame.load_budget_exhausted_count,
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L0_VISIBLE_MISSING],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L1_VISIBLE_REFINE],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L2_NEAR_PREFETCH],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L3_FAR_PREFETCH],
                         app.tile_state_bridge.budget_frame.integrate_budget_requested,
                         app.tile_state_bridge.budget_frame.integrate_budget_applied,
                         app.tile_state_bridge.budget_frame.integrate_budget_clamped_count,
                         app.tile_state_bridge.budget_frame.integrate_budget_exhausted_count,
                         app.tile_state_bridge.budget_frame.vk_asset_jobs_built,
                         app.tile_state_bridge.budget_frame.vk_asset_jobs_budget,
                         app.tile_state_bridge.budget_frame.vk_asset_budget_saturated_count,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_used,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_cap,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_hit_count);
            }
            perf_next_log = frame.after_render + 1.0;
        }
    }

    app_shutdown(&app);
    return 0;
}

int app_run(int argc, char **argv) {
    return map_forge_app_main(argc, argv);
}
