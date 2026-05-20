#include "app/app_internal.h"

#include "core/time.h"

#include <math.h>
#include <string.h>

static void app_run_viewport_scenario_phase_a(AppState *app, double now_sec) {
    if (!app || !app->viewport_scenario_active || app->viewport_scenario_completed) {
        return;
    }
    Camera *camera = &app->view_state_bridge.camera;
    if (app->viewport_scenario_start_time <= 0.0) {
        app->viewport_scenario_start_time = now_sec;
        app->viewport_scenario_origin_x = camera->x;
        app->viewport_scenario_origin_y = camera->y;
        app->viewport_scenario_origin_zoom = camera->zoom;
    }

    double elapsed = now_sec - app->viewport_scenario_start_time;
    double duration = app->viewport_scenario_duration_sec;
    if (duration <= 0.0) {
        duration = 45.0;
    }
    if (elapsed >= duration) {
        app->viewport_scenario_completed = true;
        app->viewport_scenario_active = false;
        app->ui_state_bridge.input.quit = true;
        return;
    }

    float t = (float)(elapsed / duration);
    const float tau = 6.28318530718f;
    float arc = t * tau * 2.0f;
    float arc_minor = t * tau * 3.0f;
    float pan_x = cosf(arc) * 640.0f + cosf(arc_minor) * 160.0f;
    float pan_y = sinf(arc * 0.82f) * 520.0f + sinf(arc_minor * 1.21f) * 120.0f;
    float zoom_wave = sinf(t * tau * 4.0f) * 0.72f + sinf(t * tau * 1.35f) * 0.22f;
    float zoom = app->viewport_scenario_origin_zoom + zoom_wave;
    float min_zoom = app->viewport_scenario_origin_zoom - 1.2f;
    float max_zoom = app->viewport_scenario_origin_zoom + 0.9f;
    zoom = app_clampf(zoom, min_zoom, max_zoom);

    camera->x = app->viewport_scenario_origin_x + pan_x;
    camera->y = app->viewport_scenario_origin_y + pan_y;
    camera->x_target = camera->x;
    camera->y_target = camera->y;
    camera->zoom = zoom;
    camera->zoom_target = zoom;
    camera->heading_rad = 0.0f;
    camera->heading_target_rad = 0.0f;
}

static void app_run_viewport_scenario_phase_b(AppState *app, double now_sec) {
    if (!app || !app->viewport_scenario_active || app->viewport_scenario_completed) {
        return;
    }
    Camera *camera = &app->view_state_bridge.camera;
    if (app->viewport_scenario_start_time <= 0.0) {
        app->viewport_scenario_start_time = now_sec;
        app->viewport_scenario_origin_x = camera->x;
        app->viewport_scenario_origin_y = camera->y;
        app->viewport_scenario_origin_zoom = camera->zoom;
    }

    double elapsed = now_sec - app->viewport_scenario_start_time;
    double duration = app->viewport_scenario_duration_sec;
    if (duration <= 0.0) {
        duration = 45.0;
    }
    if (elapsed >= duration) {
        app->viewport_scenario_completed = true;
        app->viewport_scenario_active = false;
        app->ui_state_bridge.input.quit = true;
        return;
    }

    float t = (float)(elapsed / duration);
    const float tau = 6.28318530718f;

    /* Box-like sweep with harmonic jitter to force continuity churn under pan. */
    float cycle = fmodf(t * 6.5f, 4.0f);
    float side_t = cycle - floorf(cycle);
    float edge_x = 0.0f;
    float edge_y = 0.0f;
    if (cycle < 1.0f) {
        edge_x = -1.0f + 2.0f * side_t;
        edge_y = -1.0f;
    } else if (cycle < 2.0f) {
        edge_x = 1.0f;
        edge_y = -1.0f + 2.0f * side_t;
    } else if (cycle < 3.0f) {
        edge_x = 1.0f - 2.0f * side_t;
        edge_y = 1.0f;
    } else {
        edge_x = -1.0f;
        edge_y = 1.0f - 2.0f * side_t;
    }

    float jitter_x = cosf(t * tau * 9.0f) * 90.0f + sinf(t * tau * 5.0f) * 55.0f;
    float jitter_y = sinf(t * tau * 8.0f) * 80.0f + cosf(t * tau * 6.0f) * 48.0f;
    float pan_x = edge_x * 860.0f + jitter_x;
    float pan_y = edge_y * 680.0f + jitter_y;

    /* Aggressive stepped zoom with wave overlay to stress cross-band continuity. */
    int step_phase = ((int)floorf(t * 10.0f)) % 4;
    float zoom_step = 0.0f;
    if (step_phase == 0) {
        zoom_step = -0.85f;
    } else if (step_phase == 1) {
        zoom_step = 0.45f;
    } else if (step_phase == 2) {
        zoom_step = -0.28f;
    } else {
        zoom_step = 0.68f;
    }
    float zoom_wave = sinf(t * tau * 7.5f) * 0.22f + cosf(t * tau * 3.2f) * 0.14f;
    float zoom = app->viewport_scenario_origin_zoom + zoom_step + zoom_wave;
    float min_zoom = app->viewport_scenario_origin_zoom - 1.5f;
    float max_zoom = app->viewport_scenario_origin_zoom + 1.2f;
    zoom = app_clampf(zoom, min_zoom, max_zoom);

    camera->x = app->viewport_scenario_origin_x + pan_x;
    camera->y = app->viewport_scenario_origin_y + pan_y;
    camera->x_target = camera->x;
    camera->y_target = camera->y;
    camera->zoom = zoom;
    camera->zoom_target = zoom;
    camera->heading_rad = 0.0f;
    camera->heading_target_rad = 0.0f;
}

static bool app_runtime_has_manual_camera_override(const InputState *input,
                                                   bool allow_mouse_pan) {
    if (!input) {
        return false;
    }
    if (input->pan_left || input->pan_right || input->pan_up || input->pan_down) {
        return true;
    }
    if (allow_mouse_pan &&
        (input->mouse_buttons & SDL_BUTTON_LMASK) != 0u &&
        (input->mouse_dx != 0 || input->mouse_dy != 0)) {
        return true;
    }
    return false;
}

void app_runtime_update_frame(AppState *app,
                              double *io_last_time,
                              float *out_dt,
                              double *out_after_update,
                              double *out_after_queue,
                              double *out_after_integrate,
                              double *out_after_route) {
    if (!app || !io_last_time) {
        if (out_dt) {
            *out_dt = 0.0f;
        }
        if (out_after_update) {
            *out_after_update = 0.0;
        }
        if (out_after_queue) {
            *out_after_queue = 0.0;
        }
        if (out_after_integrate) {
            *out_after_integrate = 0.0;
        }
        if (out_after_route) {
            *out_after_route = 0.0;
        }
        return;
    }

    double now = time_now_seconds();
    float dt = (float)(now - *io_last_time);
    *io_last_time = now;
    app_pin_panel_layout(app);
    SDL_FRect map_viewport = app_map_viewport_rect(app);
    bool mouse_in_map = app_map_viewport_contains_screen_point(app,
                                                               app->ui_state_bridge.input.mouse_x,
                                                               app->ui_state_bridge.input.mouse_y);

    if (mouse_in_map &&
        !app->route_state_bridge.dragging_start &&
        !app->route_state_bridge.dragging_goal &&
        app->ui_state_bridge.input.left_click_pressed) {
        bool over_start = app->route_state_bridge.route.has_start && app_mouse_over_anchor(app, &app->route_state_bridge.start_anchor, 7.0f);
        bool over_goal = app->route_state_bridge.route.has_goal && app_mouse_over_anchor(app, &app->route_state_bridge.goal_anchor, 7.0f);
        if (over_goal && !over_start) {
            app->route_state_bridge.dragging_goal = true;
            app_pin_panel_clear_route_goal(app);
        } else if (over_start) {
            app->route_state_bridge.dragging_start = true;
            app_pin_panel_clear_route_start(app);
        }
    }
    if (mouse_in_map &&
        !app->route_state_bridge.dragging_goal &&
        !app->ui_state_bridge.pin_add_mode_active &&
        app->ui_state_bridge.input.right_click_pressed &&
        app->route_state_bridge.route.has_goal &&
        app_mouse_over_anchor(app, &app->route_state_bridge.goal_anchor, 7.0f)) {
        app->route_state_bridge.dragging_goal = true;
        app_pin_panel_clear_route_goal(app);
    }

    bool over_start = mouse_in_map && app->route_state_bridge.route.has_start && app_mouse_over_anchor(app, &app->route_state_bridge.start_anchor, 7.0f);
    bool over_goal = mouse_in_map && app->route_state_bridge.route.has_goal && app_mouse_over_anchor(app, &app->route_state_bridge.goal_anchor, 7.0f);
    bool allow_mouse_pan = !(app->route_state_bridge.dragging_start || app->route_state_bridge.dragging_goal) &&
        mouse_in_map &&
        !((app->ui_state_bridge.input.mouse_buttons & SDL_BUTTON_LMASK) && (over_start || over_goal));
    InputState camera_input = app->ui_state_bridge.input;
    if (mouse_in_map) {
        camera_input.mouse_x -= (int)map_viewport.x;
        camera_input.mouse_y -= (int)map_viewport.y;
    } else {
        camera_input.mouse_buttons = 0u;
        camera_input.mouse_dx = 0;
        camera_input.mouse_dy = 0;
        camera_input.mouse_wheel_y = 0;
    }
    if (app_pin_panel_name_edit_active(app)) {
        camera_input.pan_left = false;
        camera_input.pan_right = false;
        camera_input.pan_up = false;
        camera_input.pan_down = false;
    }
    if (app->viewport_scenario_active && !app->viewport_scenario_completed) {
        memset(&camera_input, 0, sizeof(camera_input));
        allow_mouse_pan = false;
    }
    if (app_header_layer_scroll_update(app)) {
        camera_input.mouse_wheel_y = 0;
    }
    if (app->route_state_bridge.preview_follow_enabled &&
        !app->viewport_scenario_active &&
        app_runtime_has_manual_camera_override(&camera_input, allow_mouse_pan)) {
        app->route_state_bridge.preview_follow_enabled = false;
    }
    camera_handle_input(&app->view_state_bridge.camera,
                        &camera_input,
                        (int)map_viewport.w,
                        (int)map_viewport.h,
                        dt,
                        allow_mouse_pan);

    app_route_poll_result(app);
    app_route_panel_model_update(app);
    app_playback_update(app, dt);
    app_route_preview_update(app);
    camera_update(&app->view_state_bridge.camera, dt);
    if (app->viewport_scenario_mode == APP_VIEWPORT_SCENARIO_PHASE_B) {
        app_run_viewport_scenario_phase_b(app, now);
    } else {
        app_run_viewport_scenario_phase_a(app, now);
    }
    debug_overlay_update(&app->ui_state_bridge.overlay, dt);
    app_update_hover(app);
    double after_update = time_now_seconds();

    if (app->tile_state_bridge.budget_policy.integrate_cap == 0u) {
        app_runtime_budget_policy_init(app);
    }
    app_update_tile_queue(app);
    double after_queue = time_now_seconds();
    uint32_t integrate_fallback_budget = app->tile_state_bridge.budget_policy.integrate_fallback_budget > 0u
        ? app->tile_state_bridge.budget_policy.integrate_fallback_budget
        : APP_TILE_INTEGRATE_BUDGET;
    uint32_t integrate_cap = app->tile_state_bridge.budget_policy.integrate_cap > 0u
        ? app->tile_state_bridge.budget_policy.integrate_cap
        : 64u;
    uint32_t poly_prep_integrate_budget = app->tile_state_bridge.budget_policy.vk_poly_prep_integrate_budget > 0u
        ? app->tile_state_bridge.budget_policy.vk_poly_prep_integrate_budget
        : APP_VK_POLY_PREP_INTEGRATE_BUDGET;
    double poly_prep_slice_sec = app->tile_state_bridge.budget_policy.vk_poly_prep_integrate_time_slice_sec > 0.0
        ? app->tile_state_bridge.budget_policy.vk_poly_prep_integrate_time_slice_sec
        : APP_VK_POLY_PREP_INTEGRATE_TIME_SLICE_SEC;
    uint32_t vk_asset_build_budget = app->tile_state_bridge.budget_policy.vk_asset_build_budget > 0u
        ? app->tile_state_bridge.budget_policy.vk_asset_build_budget
        : APP_VK_ASSET_BUILD_BUDGET;
    double vk_asset_build_slice_sec = app->tile_state_bridge.budget_policy.vk_asset_build_time_slice_sec > 0.0
        ? app->tile_state_bridge.budget_policy.vk_asset_build_time_slice_sec
        : APP_VK_ASSET_BUILD_TIME_SLICE_SEC;

    uint32_t integrate_requested = app->tile_state_bridge.active_layer_valid
        ? app_tile_integrate_budget(app->tile_state_bridge.active_layer_kind, app->tile_state_bridge.active_layer_expected)
        : integrate_fallback_budget;
    uint32_t integrate_budget = integrate_requested;
    app->tile_state_bridge.budget_frame.integrate_budget_requested = integrate_requested;
    if (integrate_budget > integrate_cap) {
        integrate_budget = integrate_cap;
        app->tile_state_bridge.budget_frame.integrate_budget_clamped_count += 1u;
    }
    app->tile_state_bridge.budget_frame.integrate_budget_applied = integrate_budget;
    app_drain_tile_results(app, integrate_budget);
    if (integrate_budget > 0u &&
        app->tile_state_bridge.active_layer_valid &&
        app->tile_state_bridge.layer_inflight[app->tile_state_bridge.active_layer_kind] > 0u &&
        app->tile_state_bridge.layer_done[app->tile_state_bridge.active_layer_kind] <
            app->tile_state_bridge.layer_expected[app->tile_state_bridge.active_layer_kind]) {
        app->tile_state_bridge.budget_frame.integrate_budget_exhausted_count += 1u;
    }
    app_vk_poly_prep_drain(
        app,
        poly_prep_integrate_budget,
        poly_prep_slice_sec);
    uint64_t vk_asset_build_before = app->worker_state_bridge.vk_asset_job_build_count;
    uint32_t vk_asset_queue_before = app->worker_state_bridge.vk_asset_job_count;
    app_process_vk_asset_queue(app,
                               vk_asset_build_budget,
                               vk_asset_build_slice_sec);
    uint64_t vk_asset_build_after = app->worker_state_bridge.vk_asset_job_build_count;
    uint64_t built_delta = vk_asset_build_after >= vk_asset_build_before
        ? (vk_asset_build_after - vk_asset_build_before)
        : 0u;
    app->tile_state_bridge.budget_frame.vk_asset_jobs_budget = vk_asset_build_budget;
    app->tile_state_bridge.budget_frame.vk_asset_jobs_built = (uint32_t)built_delta;
    if (vk_asset_build_budget > 0u &&
        built_delta >= (uint64_t)vk_asset_build_budget &&
        (app->worker_state_bridge.vk_asset_job_count > 0u || vk_asset_queue_before > (uint32_t)built_delta)) {
        app->tile_state_bridge.budget_frame.vk_asset_budget_saturated_count += 1u;
    }
    app_refresh_layer_states(app);
    app_update_vk_line_budget(app);
    if (app->ui_state_bridge.input.copy_overlay_pressed) {
        app_copy_overlay_text(app);
    }
    double after_integrate = time_now_seconds();

    bool consumed_click = app_header_layer_slider_update(app);
    if (app->ui_state_bridge.input.left_click_pressed && app_header_button_hit(app, app->ui_state_bridge.input.mouse_x, app->ui_state_bridge.input.mouse_y)) {
        app->route_state_bridge.route.mode = (app->route_state_bridge.route.mode == ROUTE_MODE_CAR) ? ROUTE_MODE_WALK : ROUTE_MODE_CAR;
        if (app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
            app_route_schedule_recompute(app, 0.0);
        }
        consumed_click = true;
    } else if (app->ui_state_bridge.input.left_click_pressed && app_header_layer_toggle_click(app, app->ui_state_bridge.input.mouse_x, app->ui_state_bridge.input.mouse_y)) {
        consumed_click = true;
    } else if ((app->ui_state_bridge.input.left_click_pressed || app->ui_state_bridge.input.right_click_pressed || app->ui_state_bridge.input.middle_click_pressed) &&
               app->ui_state_bridge.input.mouse_y <= (int)APP_HEADER_HEIGHT) {
        consumed_click = true;
    }
    if (app->ui_state_bridge.input.pin_panel_toggle_pressed) {
        app->ui_state_bridge.left_pane_open = !app->ui_state_bridge.left_pane_open;
        app_pin_panel_layout(app);
        app->tile_state_bridge.queue_valid = false;
        app->tile_state_bridge.visible_valid = false;
    }
    if (app_pin_panel_handle_runtime_inputs(app)) {
        consumed_click = true;
    }
    if (!consumed_click &&
        app_pin_panel_handle_click(app,
                                   app->ui_state_bridge.input.mouse_x,
                                   app->ui_state_bridge.input.mouse_y)) {
        consumed_click = true;
    }
    if (!consumed_click && app_handle_hud_clicks(app)) {
        consumed_click = true;
    }

    if (!consumed_click && (app->ui_state_bridge.input.left_click_pressed || app->ui_state_bridge.input.right_click_pressed || app->ui_state_bridge.input.middle_click_pressed)) {
        if (app->ui_state_bridge.input.middle_click_pressed) {
            route_state_clear(&app->route_state_bridge.route);
            app_pin_panel_clear_route_bindings(app);
            app_playback_reset(app);
            app->route_state_bridge.dragging_start = false;
            app->route_state_bridge.dragging_goal = false;
            memset(&app->route_state_bridge.start_anchor, 0, sizeof(app->route_state_bridge.start_anchor));
            memset(&app->route_state_bridge.goal_anchor, 0, sizeof(app->route_state_bridge.goal_anchor));
        } else if (app->ui_state_bridge.input.left_click_pressed &&
                   !app->ui_state_bridge.input.shift_down &&
                   app_select_pin_at_screen_point(app,
                                                  app->ui_state_bridge.input.mouse_x,
                                                  app->ui_state_bridge.input.mouse_y)) {
            consumed_click = true;
        } else if (app_pin_panel_handle_map_click(app,
                                                  app->ui_state_bridge.input.mouse_x,
                                                  app->ui_state_bridge.input.mouse_y)) {
            consumed_click = true;
        } else if (app->route_state_bridge.route.loaded) {
            float world_x = 0.0f;
            float world_y = 0.0f;
            if (!app_map_screen_to_world(app,
                                         (float)app->ui_state_bridge.input.mouse_x,
                                         (float)app->ui_state_bridge.input.mouse_y,
                                         &world_x,
                                         &world_y)) {
                world_x = 0.0f;
                world_y = 0.0f;
            }
            if (app->ui_state_bridge.input.left_click_pressed) {
                if (app->route_state_bridge.route.has_start && app_mouse_over_anchor(app, &app->route_state_bridge.start_anchor, 7.0f)) {
                    app->route_state_bridge.dragging_start = true;
                    app_pin_panel_clear_route_start(app);
                } else if (app->ui_state_bridge.input.shift_down) {
                    RouteEndpointAnchor anchor = {0};
                    if (app_pick_route_anchor_unbounded(app, world_x, world_y, &anchor)) {
                        app_pin_panel_clear_route_start(app);
                        app->route_state_bridge.route.start_node = anchor.node;
                        app->route_state_bridge.route.has_start = true;
                        app->route_state_bridge.start_anchor = anchor;
                    }
                }
            }
            if (app->ui_state_bridge.input.right_click_pressed) {
                if (!app->ui_state_bridge.pin_add_mode_active &&
                    app->route_state_bridge.route.has_goal &&
                    app_mouse_over_anchor(app, &app->route_state_bridge.goal_anchor, 7.0f)) {
                    app->route_state_bridge.dragging_goal = true;
                    app_pin_panel_clear_route_goal(app);
                } else if (!app->ui_state_bridge.pin_add_mode_active) {
                    RouteEndpointAnchor anchor = {0};
                    if (app_pick_route_anchor_unbounded(app, world_x, world_y, &anchor)) {
                        app_pin_panel_clear_route_goal(app);
                        app->route_state_bridge.route.goal_node = anchor.node;
                        app->route_state_bridge.route.has_goal = true;
                        app->route_state_bridge.goal_anchor = anchor;
                    }
                }
            }
        }
    }
    if (app->ui_state_bridge.hud_ingest_panel_collapsed &&
        app->ui_state_bridge.input.enter_pressed &&
        app->route_state_bridge.route.has_start &&
        app->route_state_bridge.route.has_goal) {
        app_route_schedule_recompute(app, 0.0);
    }

    if (app->route_state_bridge.dragging_start || app->route_state_bridge.dragging_goal) {
        float world_x = 0.0f;
        float world_y = 0.0f;
        if (app_map_screen_to_world(app,
                                    (float)app->ui_state_bridge.input.mouse_x,
                                    (float)app->ui_state_bridge.input.mouse_y,
                                    &world_x,
                                    &world_y)) {
            RouteEndpointAnchor anchor = {0};
            if (app_pick_route_anchor_unbounded(app, world_x, world_y, &anchor)) {
                bool changed = false;
                if (app->route_state_bridge.dragging_start &&
                    (anchor.node != app->route_state_bridge.route.start_node || fabsf(anchor.world_x - app->route_state_bridge.start_anchor.world_x) > 0.01f ||
                     fabsf(anchor.world_y - app->route_state_bridge.start_anchor.world_y) > 0.01f)) {
                    app->route_state_bridge.route.start_node = anchor.node;
                    app->route_state_bridge.route.has_start = true;
                    app->route_state_bridge.start_anchor = anchor;
                    changed = true;
                }
                if (app->route_state_bridge.dragging_goal &&
                    (anchor.node != app->route_state_bridge.route.goal_node || fabsf(anchor.world_x - app->route_state_bridge.goal_anchor.world_x) > 0.01f ||
                     fabsf(anchor.world_y - app->route_state_bridge.goal_anchor.world_y) > 0.01f)) {
                    app->route_state_bridge.route.goal_node = anchor.node;
                    app->route_state_bridge.route.has_goal = true;
                    app->route_state_bridge.goal_anchor = anchor;
                    changed = true;
                }
                if (changed && app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
                    app_route_schedule_recompute(app, APP_ROUTE_DRAG_DEBOUNCE_SEC);
                }
            }
        }
    }

    if (app->ui_state_bridge.input.left_click_released) {
        if (app->route_state_bridge.dragging_start) {
            app->route_state_bridge.dragging_start = false;
            if (app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
                app_route_schedule_recompute(app, 0.0);
            }
        }
        if (app->route_state_bridge.dragging_goal) {
            app->route_state_bridge.dragging_goal = false;
            if (app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
                app_route_schedule_recompute(app, 0.0);
            }
        }
    }
    if (app->ui_state_bridge.input.right_click_released) {
        if (app->route_state_bridge.dragging_goal) {
            app->route_state_bridge.dragging_goal = false;
            if (app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
                app_route_schedule_recompute(app, 0.0);
            }
        }
    }
    app_route_poll_result(app);
    app_route_panel_model_update(app);
    double after_route = time_now_seconds();
    app_bridge_sync_to_legacy(app);

    if (out_dt) {
        *out_dt = dt;
    }
    if (out_after_update) {
        *out_after_update = after_update;
    }
    if (out_after_queue) {
        *out_after_queue = after_queue;
    }
    if (out_after_integrate) {
        *out_after_integrate = after_integrate;
    }
    if (out_after_route) {
        *out_after_route = after_route;
    }
}
