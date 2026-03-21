#include "app/app_internal.h"

#include "core/time.h"

#include <math.h>
#include <string.h>

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

    if (!app->route_state_bridge.dragging_start && !app->route_state_bridge.dragging_goal && app->ui_state_bridge.input.left_click_pressed) {
        bool over_start = app->route_state_bridge.route.has_start && app_mouse_over_anchor(app, &app->route_state_bridge.start_anchor, 7.0f);
        bool over_goal = app->route_state_bridge.route.has_goal && app_mouse_over_anchor(app, &app->route_state_bridge.goal_anchor, 7.0f);
        if (over_goal && !over_start) {
            app->route_state_bridge.dragging_goal = true;
        } else if (over_start) {
            app->route_state_bridge.dragging_start = true;
        }
    }
    if (!app->route_state_bridge.dragging_goal && app->ui_state_bridge.input.right_click_pressed && app->route_state_bridge.route.has_goal &&
        app_mouse_over_anchor(app, &app->route_state_bridge.goal_anchor, 7.0f)) {
        app->route_state_bridge.dragging_goal = true;
    }

    bool over_start = app->route_state_bridge.route.has_start && app_mouse_over_anchor(app, &app->route_state_bridge.start_anchor, 7.0f);
    bool over_goal = app->route_state_bridge.route.has_goal && app_mouse_over_anchor(app, &app->route_state_bridge.goal_anchor, 7.0f);
    bool allow_mouse_pan = !(app->route_state_bridge.dragging_start || app->route_state_bridge.dragging_goal) &&
        !((app->ui_state_bridge.input.mouse_buttons & SDL_BUTTON_LMASK) && (over_start || over_goal));
    camera_handle_input(&app->view_state_bridge.camera, &app->ui_state_bridge.input, app->width, app->height, dt, allow_mouse_pan);
    camera_update(&app->view_state_bridge.camera, dt);
    debug_overlay_update(&app->ui_state_bridge.overlay, dt);

    app_update_hover(app);
    double after_update = time_now_seconds();

    app_route_poll_result(app);
    app_update_tile_queue(app);
    double after_queue = time_now_seconds();
    uint32_t integrate_budget = app->tile_state_bridge.active_layer_valid
        ? app_tile_integrate_budget(app->tile_state_bridge.active_layer_kind, app->tile_state_bridge.active_layer_expected)
        : APP_TILE_INTEGRATE_BUDGET;
    app_drain_tile_results(app, integrate_budget);
    app_vk_poly_prep_drain(
        app,
        APP_VK_POLY_PREP_INTEGRATE_BUDGET,
        APP_VK_POLY_PREP_INTEGRATE_TIME_SLICE_SEC);
    app_process_vk_asset_queue(app, APP_VK_ASSET_BUILD_BUDGET, APP_VK_ASSET_BUILD_TIME_SLICE_SEC);
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
    if (!consumed_click && app_handle_hud_clicks(app)) {
        consumed_click = true;
    }

    if (!consumed_click && (app->ui_state_bridge.input.left_click_pressed || app->ui_state_bridge.input.right_click_pressed || app->ui_state_bridge.input.middle_click_pressed)) {
        if (app->ui_state_bridge.input.middle_click_pressed) {
            route_state_clear(&app->route_state_bridge.route);
            app_playback_reset(app);
            app->route_state_bridge.dragging_start = false;
            app->route_state_bridge.dragging_goal = false;
            memset(&app->route_state_bridge.start_anchor, 0, sizeof(app->route_state_bridge.start_anchor));
            memset(&app->route_state_bridge.goal_anchor, 0, sizeof(app->route_state_bridge.goal_anchor));
        } else if (app->route_state_bridge.route.loaded) {
            float world_x = 0.0f;
            float world_y = 0.0f;
            camera_screen_to_world(&app->view_state_bridge.camera, (float)app->ui_state_bridge.input.mouse_x, (float)app->ui_state_bridge.input.mouse_y, app->width, app->height, &world_x, &world_y);
            if (app->ui_state_bridge.input.left_click_pressed) {
                if (app->route_state_bridge.route.has_start && app_mouse_over_anchor(app, &app->route_state_bridge.start_anchor, 7.0f)) {
                    app->route_state_bridge.dragging_start = true;
                } else if (app->ui_state_bridge.input.shift_down) {
                    RouteEndpointAnchor anchor = {0};
                    if (app_pick_route_anchor_unbounded(app, world_x, world_y, &anchor)) {
                        app->route_state_bridge.route.start_node = anchor.node;
                        app->route_state_bridge.route.has_start = true;
                        app->route_state_bridge.start_anchor = anchor;
                    }
                }
            }
            if (app->ui_state_bridge.input.right_click_pressed) {
                if (app->route_state_bridge.route.has_goal && app_mouse_over_anchor(app, &app->route_state_bridge.goal_anchor, 7.0f)) {
                    app->route_state_bridge.dragging_goal = true;
                } else {
                    RouteEndpointAnchor anchor = {0};
                    if (app_pick_route_anchor_unbounded(app, world_x, world_y, &anchor)) {
                        app->route_state_bridge.route.goal_node = anchor.node;
                        app->route_state_bridge.route.has_goal = true;
                        app->route_state_bridge.goal_anchor = anchor;
                    }
                }
            }
        }
    }
    if (app->ui_state_bridge.input.enter_pressed && app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
        app_route_schedule_recompute(app, 0.0);
    }

    if (app->route_state_bridge.dragging_start || app->route_state_bridge.dragging_goal) {
        float world_x = 0.0f;
        float world_y = 0.0f;
        camera_screen_to_world(&app->view_state_bridge.camera, (float)app->ui_state_bridge.input.mouse_x, (float)app->ui_state_bridge.input.mouse_y, app->width, app->height, &world_x, &world_y);
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
    double after_route = time_now_seconds();

    app_playback_update(app, dt);
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
