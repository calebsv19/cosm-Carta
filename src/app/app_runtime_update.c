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

    if (!app->dragging_start && !app->dragging_goal && app->input.left_click_pressed) {
        bool over_start = app->route.has_start && app_mouse_over_anchor(app, &app->start_anchor, 7.0f);
        bool over_goal = app->route.has_goal && app_mouse_over_anchor(app, &app->goal_anchor, 7.0f);
        if (over_goal && !over_start) {
            app->dragging_goal = true;
        } else if (over_start) {
            app->dragging_start = true;
        }
    }
    if (!app->dragging_goal && app->input.right_click_pressed && app->route.has_goal &&
        app_mouse_over_anchor(app, &app->goal_anchor, 7.0f)) {
        app->dragging_goal = true;
    }

    bool over_start = app->route.has_start && app_mouse_over_anchor(app, &app->start_anchor, 7.0f);
    bool over_goal = app->route.has_goal && app_mouse_over_anchor(app, &app->goal_anchor, 7.0f);
    bool allow_mouse_pan = !(app->dragging_start || app->dragging_goal) &&
        !((app->input.mouse_buttons & SDL_BUTTON_LMASK) && (over_start || over_goal));
    camera_handle_input(&app->camera, &app->input, app->width, app->height, dt, allow_mouse_pan);
    camera_update(&app->camera, dt);
    debug_overlay_update(&app->overlay, dt);

    app_update_hover(app);
    double after_update = time_now_seconds();

    app_route_poll_result(app);
    app_update_tile_queue(app);
    double after_queue = time_now_seconds();
    uint32_t integrate_budget = app->active_layer_valid
        ? app_tile_integrate_budget(app->active_layer_kind, app->active_layer_expected)
        : APP_TILE_INTEGRATE_BUDGET;
    app_drain_tile_results(app, integrate_budget);
    app_vk_poly_prep_drain(
        app,
        APP_VK_POLY_PREP_INTEGRATE_BUDGET,
        APP_VK_POLY_PREP_INTEGRATE_TIME_SLICE_SEC);
    app_process_vk_asset_queue(app, APP_VK_ASSET_BUILD_BUDGET, APP_VK_ASSET_BUILD_TIME_SLICE_SEC);
    app_refresh_layer_states(app);
    app_update_vk_line_budget(app);
    if (app->input.copy_overlay_pressed) {
        app_copy_overlay_text(app);
    }
    double after_integrate = time_now_seconds();

    bool consumed_click = app_header_layer_slider_update(app);
    if (app->input.left_click_pressed && app_header_button_hit(app, app->input.mouse_x, app->input.mouse_y)) {
        app->route.mode = (app->route.mode == ROUTE_MODE_CAR) ? ROUTE_MODE_WALK : ROUTE_MODE_CAR;
        if (app->route.has_start && app->route.has_goal) {
            app_route_schedule_recompute(app, 0.0);
        }
        consumed_click = true;
    } else if (app->input.left_click_pressed && app_header_layer_toggle_click(app, app->input.mouse_x, app->input.mouse_y)) {
        consumed_click = true;
    } else if ((app->input.left_click_pressed || app->input.right_click_pressed || app->input.middle_click_pressed) &&
               app->input.mouse_y <= (int)APP_HEADER_HEIGHT) {
        consumed_click = true;
    }
    if (!consumed_click && app_handle_hud_clicks(app)) {
        consumed_click = true;
    }

    if (!consumed_click && (app->input.left_click_pressed || app->input.right_click_pressed || app->input.middle_click_pressed)) {
        if (app->input.middle_click_pressed) {
            route_state_clear(&app->route);
            app_playback_reset(app);
            app->dragging_start = false;
            app->dragging_goal = false;
            memset(&app->start_anchor, 0, sizeof(app->start_anchor));
            memset(&app->goal_anchor, 0, sizeof(app->goal_anchor));
        } else if (app->route.loaded) {
            float world_x = 0.0f;
            float world_y = 0.0f;
            camera_screen_to_world(&app->camera, (float)app->input.mouse_x, (float)app->input.mouse_y, app->width, app->height, &world_x, &world_y);
            if (app->input.left_click_pressed) {
                if (app->route.has_start && app_mouse_over_anchor(app, &app->start_anchor, 7.0f)) {
                    app->dragging_start = true;
                } else if (app->input.shift_down) {
                    RouteEndpointAnchor anchor = {0};
                    if (app_pick_route_anchor_unbounded(app, world_x, world_y, &anchor)) {
                        app->route.start_node = anchor.node;
                        app->route.has_start = true;
                        app->start_anchor = anchor;
                    }
                }
            }
            if (app->input.right_click_pressed) {
                if (app->route.has_goal && app_mouse_over_anchor(app, &app->goal_anchor, 7.0f)) {
                    app->dragging_goal = true;
                } else {
                    RouteEndpointAnchor anchor = {0};
                    if (app_pick_route_anchor_unbounded(app, world_x, world_y, &anchor)) {
                        app->route.goal_node = anchor.node;
                        app->route.has_goal = true;
                        app->goal_anchor = anchor;
                    }
                }
            }
        }
    }
    if (app->input.enter_pressed && app->route.has_start && app->route.has_goal) {
        app_route_schedule_recompute(app, 0.0);
    }

    if (app->dragging_start || app->dragging_goal) {
        float world_x = 0.0f;
        float world_y = 0.0f;
        camera_screen_to_world(&app->camera, (float)app->input.mouse_x, (float)app->input.mouse_y, app->width, app->height, &world_x, &world_y);
        RouteEndpointAnchor anchor = {0};
        if (app_pick_route_anchor_unbounded(app, world_x, world_y, &anchor)) {
            bool changed = false;
            if (app->dragging_start &&
                (anchor.node != app->route.start_node || fabsf(anchor.world_x - app->start_anchor.world_x) > 0.01f ||
                 fabsf(anchor.world_y - app->start_anchor.world_y) > 0.01f)) {
                app->route.start_node = anchor.node;
                app->route.has_start = true;
                app->start_anchor = anchor;
                changed = true;
            }
            if (app->dragging_goal &&
                (anchor.node != app->route.goal_node || fabsf(anchor.world_x - app->goal_anchor.world_x) > 0.01f ||
                 fabsf(anchor.world_y - app->goal_anchor.world_y) > 0.01f)) {
                app->route.goal_node = anchor.node;
                app->route.has_goal = true;
                app->goal_anchor = anchor;
                changed = true;
            }
            if (changed && app->route.has_start && app->route.has_goal) {
                app_route_schedule_recompute(app, APP_ROUTE_DRAG_DEBOUNCE_SEC);
            }
        }
    }

    if (app->input.left_click_released) {
        if (app->dragging_start) {
            app->dragging_start = false;
            if (app->route.has_start && app->route.has_goal) {
                app_route_schedule_recompute(app, 0.0);
            }
        }
        if (app->dragging_goal) {
            app->dragging_goal = false;
            if (app->route.has_start && app->route.has_goal) {
                app_route_schedule_recompute(app, 0.0);
            }
        }
    }
    if (app->input.right_click_released) {
        if (app->dragging_goal) {
            app->dragging_goal = false;
            if (app->route.has_start && app->route.has_goal) {
                app_route_schedule_recompute(app, 0.0);
            }
        }
    }
    app_route_poll_result(app);
    double after_route = time_now_seconds();

    app_playback_update(app, dt);

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
