#include "app/app_internal.h"
#include "app/app_ui_internal.h"

#include "ui/font.h"

#include <stdio.h>
#include <string.h>

static const char *app_layer_label(TileLayerKind kind) {
    return layer_policy_label(kind);
}

static const char *app_layer_runtime_state_label(const AppState *app, TileLayerKind kind) {
    if (!app) {
        return "off";
    }
    return app_layer_active_runtime(app, kind) ? "on" : "off";
}

static int app_layer_debug_line_count(void) {
    return 8 + (int)layer_policy_count();
}

static int app_digits_u32(uint32_t value) {
    int digits = 1;
    while (value >= 10u) {
        value /= 10u;
        digits += 1;
    }
    return digits;
}

static uint64_t app_hash_mix_u64(uint64_t seed, uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    return seed;
}

static uint64_t app_layer_debug_layout_hash(const AppState *app) {
    if (!app) {
        return 0ull;
    }

    uint64_t hash = 1469598103934665603ull;
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->width);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->height);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->tile_state_bridge.visible_tile_count);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->tile_state_bridge.visible_ideal_count);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->tile_state_bridge.visible_renderable_count);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->tile_state_bridge.visible_missing_count);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)(app->tile_state_bridge.visible_coverage_ratio * 1000.0f));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->tile_state_bridge.active_layer_kind);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->tile_state_bridge.active_layer_valid);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.loading_done));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.loading_expected));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->route_state_bridge.route_snap_debug_cells));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->route_state_bridge.route_snap_debug_segments));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->route_state_bridge.route_snap_debug_hits));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.draw_path_vk_count));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.draw_path_fallback_count));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.transition_blend_draw_count));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.presenter_invariant_fail_count));
    hash = app_hash_mix_u64(hash, app->tile_state_bridge.lifecycle_frame_index);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.lifecycle_transition_count));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.lifecycle_invalid_transition_count));
    hash = app_hash_mix_u64(hash, app->tile_state_bridge.lifecycle_invalid_transition_total);
    hash = app_hash_mix_u64(hash, app->worker_state_bridge.vk_poly_prep_quarantine_job_count);
    hash = app_hash_mix_u64(hash, app->worker_state_bridge.vk_poly_prep_quarantine_polygon_count);
    hash = app_hash_mix_u64(hash, app->worker_state_bridge.vk_poly_prep_quarantine_ring_bounds_count);
    hash = app_hash_mix_u64(hash, app->worker_state_bridge.vk_poly_prep_quarantine_ring_min_points_count);
    hash = app_hash_mix_u64(hash, app->worker_state_bridge.vk_poly_prep_quarantine_ring_degenerate_count);
    hash = app_hash_mix_u64(hash, app->worker_state_bridge.vk_poly_prep_winding_normalized_count);
    for (size_t i = 0; i < APP_TILE_LIFECYCLE_STATE_COUNT; ++i) {
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.lifecycle_transition_to_state[i]));
    }
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->region.tile_source.storage_kind);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app->region.tile_source.policy_mode);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)(app->region.has_tile_archive ? 1u : 0u));
    hash = app_hash_mix_u64(hash, app->region.archive_rollup_total_rows);
    hash = app_hash_mix_u64(hash, app->region.archive_rollup_total_bytes);

    TileSourceRuntimeStats source_stats = {0};
    tile_source_runtime_stats_get(&source_stats);
    hash = app_hash_mix_u64(hash, source_stats.archive_request_count);
    hash = app_hash_mix_u64(hash, source_stats.archive_hit_count);
    hash = app_hash_mix_u64(hash, source_stats.archive_extract_count);
    hash = app_hash_mix_u64(hash, source_stats.archive_extract_fail_count);
    hash = app_hash_mix_u64(hash, source_stats.archive_fallback_tree_count);
    hash = app_hash_mix_u64(hash, source_stats.archive_policy_block_count);

    for (size_t i = 0; i < TILE_BAND_COUNT; ++i) {
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.band_visible_loaded[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.band_visible_expected[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.band_queue_depth[i]));
    }
    for (size_t i = 0; i < TILE_QUEUE_LANE_COUNT; ++i) {
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.lane_queue_depth[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.lane_service_count[i]));
    }
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.lane_l0_pending));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)(app->tile_state_bridge.lane_l0_latency_ms * 10.0f));
    hash = app_hash_mix_u64(hash, app->tile_state_bridge.lane_l0_saturation_total);
    hash = app_hash_mix_u64(hash, app->tile_state_bridge.lane_l0_dropped_visible_requests);
    hash = app_hash_mix_u64(hash, app->tile_state_bridge.lane_l0_retry_visible_requests);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.coverage_suppressed_frame));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.coverage_suppressed_visible_frame));
    hash = app_hash_mix_u64(hash, app->tile_state_bridge.coverage_suppressed_total);
    hash = app_hash_mix_u64(hash, app->tile_state_bridge.coverage_suppressed_visible_total);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.coverage_gate_deferred_count));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.coverage_gate_timeout_count));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.band_commit_frame_count));
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.queue_rebuild_frame_count));
    hash = app_hash_mix_u64(hash, app->tile_state_bridge.band_commit_total);
    hash = app_hash_mix_u64(hash, app->tile_state_bridge.queue_rebuild_total);
    hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.cache_evicted_frame_total));
    hash = app_hash_mix_u64(hash, app->tile_state_bridge.cache_evicted_total);
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.cache_target[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.cache_resident[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.cache_evicted_frame[i]));
        hash = app_hash_mix_u64(hash, app->tile_state_bridge.cache_evicted_total_by_layer[i]);
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.layer_expected[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.layer_done[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.layer_visible_loaded[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.layer_visible_expected[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)app_digits_u32(app->tile_state_bridge.layer_inflight[i]));
        hash = app_hash_mix_u64(hash, (uint64_t)(uint32_t)(app->tile_state_bridge.layer_coverage_ratio[i] * 1000.0f));
        hash = app_hash_mix_u64(hash, app->worker_state_bridge.vk_poly_prep_quarantine_jobs_by_layer[i]);
        hash = app_hash_mix_u64(hash, app->worker_state_bridge.vk_poly_prep_quarantine_rings_by_layer[i]);
    }
    return hash;
}

static bool app_layer_debug_format_line(const AppState *app, int index, char *line, size_t line_size) {
    if (!app || !line || line_size == 0u) {
        return false;
    }
    if (index == 0) {
        snprintf(line, line_size, "Visible tiles: %u viewset i=%u r=%u m=%u cov=%.2f",
                 app->tile_state_bridge.visible_tile_count,
                 app->tile_state_bridge.visible_ideal_count,
                 app->tile_state_bridge.visible_renderable_count,
                 app->tile_state_bridge.visible_missing_count,
                 app->tile_state_bridge.visible_coverage_ratio);
        return true;
    }
    if (index == 1) {
        if (app->tile_state_bridge.active_layer_valid) {
            snprintf(line, line_size, "Active layer: %s", app_layer_label(app->tile_state_bridge.active_layer_kind));
        } else {
            snprintf(line, line_size, "Active layer: none");
        }
        return true;
    }
    if (index == 2) {
        snprintf(line, line_size, "Load total: %u/%u no_data=%.1fs",
                 app->tile_state_bridge.loading_done, app->tile_state_bridge.loading_expected, app->tile_state_bridge.loading_no_data_time);
        return true;
    }
    if (index == 3) {
        snprintf(line, line_size, "Bands vis c=%u/%u m=%u/%u f=%u/%u d=%u/%u q(c=%u m=%u f=%u d=%u) lanes q=%u/%u/%u/%u svc=%u/%u/%u/%u l0(p=%u lat=%.1fms drop=%llu retry=%llu) covsup=%u/%u total=%llu/%llu gate=%u/%u fallback=%u churn(b=%u q=%u) cache(evict=%u total=%llu)",
                 app->tile_state_bridge.band_visible_loaded[TILE_BAND_COARSE], app->tile_state_bridge.band_visible_expected[TILE_BAND_COARSE],
                 app->tile_state_bridge.band_visible_loaded[TILE_BAND_MID], app->tile_state_bridge.band_visible_expected[TILE_BAND_MID],
                 app->tile_state_bridge.band_visible_loaded[TILE_BAND_FINE], app->tile_state_bridge.band_visible_expected[TILE_BAND_FINE],
                 app->tile_state_bridge.band_visible_loaded[TILE_BAND_DEFAULT], app->tile_state_bridge.band_visible_expected[TILE_BAND_DEFAULT],
                 app->tile_state_bridge.band_queue_depth[TILE_BAND_COARSE], app->tile_state_bridge.band_queue_depth[TILE_BAND_MID],
                 app->tile_state_bridge.band_queue_depth[TILE_BAND_FINE], app->tile_state_bridge.band_queue_depth[TILE_BAND_DEFAULT],
                 app->tile_state_bridge.lane_queue_depth[TILE_QUEUE_LANE_L0_VISIBLE_MISSING],
                 app->tile_state_bridge.lane_queue_depth[TILE_QUEUE_LANE_L1_VISIBLE_REFINE],
                 app->tile_state_bridge.lane_queue_depth[TILE_QUEUE_LANE_L2_NEAR_PREFETCH],
                 app->tile_state_bridge.lane_queue_depth[TILE_QUEUE_LANE_L3_FAR_PREFETCH],
                 app->tile_state_bridge.lane_service_count[TILE_QUEUE_LANE_L0_VISIBLE_MISSING],
                 app->tile_state_bridge.lane_service_count[TILE_QUEUE_LANE_L1_VISIBLE_REFINE],
                 app->tile_state_bridge.lane_service_count[TILE_QUEUE_LANE_L2_NEAR_PREFETCH],
                 app->tile_state_bridge.lane_service_count[TILE_QUEUE_LANE_L3_FAR_PREFETCH],
                 app->tile_state_bridge.lane_l0_pending,
                 app->tile_state_bridge.lane_l0_latency_ms,
                 (unsigned long long)app->tile_state_bridge.lane_l0_dropped_visible_requests,
                 (unsigned long long)app->tile_state_bridge.lane_l0_retry_visible_requests,
                 app->tile_state_bridge.coverage_suppressed_frame,
                 app->tile_state_bridge.coverage_suppressed_visible_frame,
                 (unsigned long long)app->tile_state_bridge.coverage_suppressed_total,
                 (unsigned long long)app->tile_state_bridge.coverage_suppressed_visible_total,
                 app->tile_state_bridge.coverage_gate_deferred_count,
                 app->tile_state_bridge.coverage_gate_timeout_count,
                 app->tile_state_bridge.vk_road_band_fallback_draws,
                 app->tile_state_bridge.band_commit_frame_count,
                 app->tile_state_bridge.queue_rebuild_frame_count,
                 app->tile_state_bridge.cache_evicted_frame_total,
                 (unsigned long long)app->tile_state_bridge.cache_evicted_total);
        return true;
    }
    if (index == 4) {
        snprintf(line, line_size, "Route snap cells=%u seg=%u hits=%u q=%.2fms",
                 app->route_state_bridge.route_snap_debug_cells,
                 app->route_state_bridge.route_snap_debug_segments,
                 app->route_state_bridge.route_snap_debug_hits,
                 app->route_state_bridge.route_snap_debug_query_ms);
        return true;
    }
    if (index == 5) {
        snprintf(line, line_size, "Draw vk=%u fallback=%u blend=%u hold %u/%u upd=%u inv_fail=%u life f=%llu tx=%u bad=%u req=%u cpu=%u gpu=%u ren=%u st=%u polyq(j=%llu p=%llu b=%llu m=%llu d=%llu w=%llu) ql(w=%llu p=%llu l=%llu b=%llu)",
                 app->tile_state_bridge.draw_path_vk_count,
                 app->tile_state_bridge.draw_path_fallback_count,
                 app->tile_state_bridge.transition_blend_draw_count,
                 app->tile_state_bridge.present_hold_hits,
                 app->tile_state_bridge.present_hold_misses,
                 app->tile_state_bridge.present_hold_updates,
                 app->tile_state_bridge.presenter_invariant_fail_count,
                 (unsigned long long)app->tile_state_bridge.lifecycle_frame_index,
                 app->tile_state_bridge.lifecycle_transition_count,
                 app->tile_state_bridge.lifecycle_invalid_transition_count,
                 app->tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_REQUESTED],
                 app->tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_DECODED_CPU],
                 app->tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_UPLOADED_GPU],
                 app->tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_RENDERABLE],
                 app->tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_STALE],
                 (unsigned long long)app->worker_state_bridge.vk_poly_prep_quarantine_job_count,
                 (unsigned long long)app->worker_state_bridge.vk_poly_prep_quarantine_polygon_count,
                 (unsigned long long)app->worker_state_bridge.vk_poly_prep_quarantine_ring_bounds_count,
                 (unsigned long long)app->worker_state_bridge.vk_poly_prep_quarantine_ring_min_points_count,
                 (unsigned long long)app->worker_state_bridge.vk_poly_prep_quarantine_ring_degenerate_count,
                 (unsigned long long)app->worker_state_bridge.vk_poly_prep_winding_normalized_count,
                 (unsigned long long)app->worker_state_bridge.vk_poly_prep_quarantine_rings_by_layer[TILE_LAYER_POLY_WATER],
                 (unsigned long long)app->worker_state_bridge.vk_poly_prep_quarantine_rings_by_layer[TILE_LAYER_POLY_PARK],
                 (unsigned long long)app->worker_state_bridge.vk_poly_prep_quarantine_rings_by_layer[TILE_LAYER_POLY_LANDUSE],
                 (unsigned long long)app->worker_state_bridge.vk_poly_prep_quarantine_rings_by_layer[TILE_LAYER_POLY_BUILDING]);
        return true;
    }
    if (index == 6) {
        TileSourceRuntimeStats source_stats = {0};
        tile_source_runtime_stats_get(&source_stats);
        snprintf(line, line_size, "Tile source=%s policy=%s archive(req=%llu hit=%llu ext=%llu fail=%llu tree=%llu block=%llu)",
                 tile_storage_kind_label(app->region.tile_source.storage_kind),
                 tile_source_policy_mode_label(app->region.tile_source.policy_mode),
                 (unsigned long long)source_stats.archive_request_count,
                 (unsigned long long)source_stats.archive_hit_count,
                 (unsigned long long)source_stats.archive_extract_count,
                 (unsigned long long)source_stats.archive_extract_fail_count,
                 (unsigned long long)source_stats.archive_fallback_tree_count,
                 (unsigned long long)source_stats.archive_policy_block_count);
        return true;
    }

    if (index == 7) {
        if (app->region.has_archive_rollups) {
            uint64_t fine_roads = app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_ARTERY] +
                app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_LOCAL];
            uint64_t fine_polys = app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_WATER] +
                app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_PARK] +
                app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_LANDUSE] +
                app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_BUILDING];
            snprintf(line, line_size, "Pkg rollup rows=%llu bytes=%llu fine(road=%llu poly=%llu)",
                     (unsigned long long)app->region.archive_rollup_total_rows,
                     (unsigned long long)app->region.archive_rollup_total_bytes,
                     (unsigned long long)fine_roads,
                     (unsigned long long)fine_polys);
        } else {
            snprintf(line, line_size, "Pkg rollup rows=n/a bytes=n/a");
        }
        return true;
    }

    int policy_index = index - 8;
    if (policy_index < 0 || (size_t)policy_index >= layer_policy_count()) {
        return false;
    }
    const LayerPolicy *policy = layer_policy_at((size_t)policy_index);
    if (!policy) {
        return false;
    }
    TileLayerKind kind = policy->kind;
    float start = app_layer_zoom_start(app, kind);
    snprintf(line, line_size, "%s z>=%.2f band=%s exp %u done %u vis %u/%u cov %.2f in %u cache %u/%u ev=%u state=%s runtime=%s",
             app_layer_label(kind),
             start,
             layer_policy_band_label(app->tile_state_bridge.layer_target_band[kind]),
             app->tile_state_bridge.layer_expected[kind],
             app->tile_state_bridge.layer_done[kind],
             app->tile_state_bridge.layer_visible_loaded[kind],
             app->tile_state_bridge.layer_visible_expected[kind],
             app->tile_state_bridge.layer_coverage_ratio[kind],
             app->tile_state_bridge.layer_inflight[kind],
             app->tile_state_bridge.cache_resident[kind],
             app->tile_state_bridge.cache_target[kind],
             app->tile_state_bridge.cache_evicted_frame[kind],
             layer_policy_readiness_label(app->tile_state_bridge.layer_state[kind]),
             app_layer_runtime_state_label(app, kind));
    return true;
}

void app_draw_layer_debug(AppState *app) {
    if (!app) {
        return;
    }
    memset(&app->ui_state_bridge.hud_layer_debug_panel_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_panel_rect));
    memset(&app->ui_state_bridge.hud_layer_debug_collapse_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_collapse_rect));
    memset(&app->ui_state_bridge.hud_layer_debug_handle_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_handle_rect));
    app->ui_state_bridge.hud_layer_debug_layout_dirty = true;
}

bool app_handle_hud_clicks(AppState *app) {
    if (!app) {
        return false;
    }
    bool any_click = app->ui_state_bridge.input.left_click_pressed || app->ui_state_bridge.input.right_click_pressed || app->ui_state_bridge.input.middle_click_pressed;
    if (!any_click) {
        return false;
    }

    if (app_route_panel_handle_click(app)) {
        return true;
    }
    return false;
}

void app_copy_overlay_text(AppState *app) {
    if (!app) {
        return;
    }

    char buffer[2048];
    size_t offset = 0;
    uint64_t fine_rollup_roads = app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_ARTERY] +
        app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_LOCAL];
    uint64_t fine_rollup_polys = app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_WATER] +
        app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_PARK] +
        app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_LANDUSE] +
        app->region.archive_rollup_rows[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_BUILDING];
    int written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "Region: %s\nZoom: %.2f\nVisible tiles: %u\nLoad total: %u/%u no_data=%.1fs\n"
                           "Bands vis c=%u/%u m=%u/%u f=%u/%u d=%u/%u q(c=%u m=%u f=%u d=%u) covsup=%u/%u total=%llu/%llu fallback=%u churn(b=%u q=%u)\n"
                           "Cache pressure evict=%u total=%llu\n"
                           "Draw vk=%u fallback=%u blend=%u hold %u/%u upd=%u inv_fail=%u\n"
                           "Hardening invariants=%s contour=%s\n"
                           "Pkg rollup rows=%llu bytes=%llu fine(road=%llu poly=%llu)\n",
                           app->region.name,
                           app->view_state_bridge.camera.zoom,
                           app->tile_state_bridge.visible_tile_count,
                           app->tile_state_bridge.loading_done,
                           app->tile_state_bridge.loading_expected,
                           app->tile_state_bridge.loading_no_data_time,
                           app->tile_state_bridge.band_visible_loaded[TILE_BAND_COARSE], app->tile_state_bridge.band_visible_expected[TILE_BAND_COARSE],
                           app->tile_state_bridge.band_visible_loaded[TILE_BAND_MID], app->tile_state_bridge.band_visible_expected[TILE_BAND_MID],
                           app->tile_state_bridge.band_visible_loaded[TILE_BAND_FINE], app->tile_state_bridge.band_visible_expected[TILE_BAND_FINE],
                           app->tile_state_bridge.band_visible_loaded[TILE_BAND_DEFAULT], app->tile_state_bridge.band_visible_expected[TILE_BAND_DEFAULT],
                           app->tile_state_bridge.band_queue_depth[TILE_BAND_COARSE], app->tile_state_bridge.band_queue_depth[TILE_BAND_MID],
                           app->tile_state_bridge.band_queue_depth[TILE_BAND_FINE], app->tile_state_bridge.band_queue_depth[TILE_BAND_DEFAULT],
                           app->tile_state_bridge.coverage_suppressed_frame,
                           app->tile_state_bridge.coverage_suppressed_visible_frame,
                           (unsigned long long)app->tile_state_bridge.coverage_suppressed_total,
                           (unsigned long long)app->tile_state_bridge.coverage_suppressed_visible_total,
                           app->tile_state_bridge.vk_road_band_fallback_draws,
                           app->tile_state_bridge.band_commit_frame_count,
                           app->tile_state_bridge.queue_rebuild_frame_count,
                           app->tile_state_bridge.cache_evicted_frame_total,
                           (unsigned long long)app->tile_state_bridge.cache_evicted_total,
                           app->tile_state_bridge.draw_path_vk_count,
                           app->tile_state_bridge.draw_path_fallback_count,
                           app->tile_state_bridge.transition_blend_draw_count,
                           app->tile_state_bridge.present_hold_hits,
                           app->tile_state_bridge.present_hold_misses,
                           app->tile_state_bridge.present_hold_updates,
                           app->tile_state_bridge.presenter_invariant_fail_count,
                           app->tile_state_bridge.presenter_invariants_enabled ? "on" : "off",
                           app->tile_state_bridge.contour_runtime_enabled ? "on" : "off",
                           (unsigned long long)app->region.archive_rollup_total_rows,
                           (unsigned long long)app->region.archive_rollup_total_bytes,
                           (unsigned long long)fine_rollup_roads,
                           (unsigned long long)fine_rollup_polys);
    if (written < 0) {
        return;
    }
    offset += (size_t)written;

    if (app->tile_state_bridge.active_layer_valid) {
        written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "Active layer: %s\n",
                           app_layer_label(app->tile_state_bridge.active_layer_kind));
    } else {
        written = snprintf(buffer + offset, sizeof(buffer) - offset, "Active layer: none\n");
    }
    if (written < 0) {
        return;
    }
    offset += (size_t)written;

    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy) {
            continue;
        }
        TileLayerKind kind = policy->kind;
        float start = app_layer_zoom_start(app, kind);
        written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "%s z>=%.2f band=%s exp %u done %u vis %u/%u in %u cache %u/%u ev=%u state=%s runtime=%s\n",
                           app_layer_label(kind),
                           start,
                           layer_policy_band_label(app->tile_state_bridge.layer_target_band[kind]),
                           app->tile_state_bridge.layer_expected[kind],
                           app->tile_state_bridge.layer_done[kind],
                           app->tile_state_bridge.layer_visible_loaded[kind],
                           app->tile_state_bridge.layer_visible_expected[kind],
                           app->tile_state_bridge.layer_inflight[kind],
                           app->tile_state_bridge.cache_resident[kind],
                           app->tile_state_bridge.cache_target[kind],
                           app->tile_state_bridge.cache_evicted_frame[kind],
                           layer_policy_readiness_label(app->tile_state_bridge.layer_state[kind]),
                           app_layer_runtime_state_label(app, kind));
        if (written < 0) {
            return;
        }
        offset += (size_t)written;
        if (offset >= sizeof(buffer)) {
            break;
        }
    }

    SDL_SetClipboardText(buffer);
}
