#include "app/app_internal.h"
#include "app/app_runtime_input_policy.h"

#include "core/time.h"

#include <string.h>

static uint32_t app_runtime_count_bool(bool value) {
    return value ? 1u : 0u;
}

static void app_runtime_input_intake(AppState *app, AppInputEventRaw *out_raw) {
    if (!app || !out_raw) {
        return;
    }
    input_begin_frame(&app->ui_state_bridge.input);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        out_raw->sdl_event_count += 1u;
        switch (event.type) {
            case SDL_QUIT:
                out_raw->quit_event_count += 1u;
                break;
            case SDL_WINDOWEVENT:
                out_raw->window_event_count += 1u;
                break;
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                out_raw->mouse_event_count += 1u;
                break;
            case SDL_MOUSEWHEEL:
                out_raw->wheel_event_count += 1u;
                break;
            case SDL_KEYDOWN:
                out_raw->keydown_event_count += 1u;
                break;
            case SDL_KEYUP:
                out_raw->keyup_event_count += 1u;
                break;
            default:
                out_raw->other_event_count += 1u;
                break;
        }
        input_handle_event(&app->ui_state_bridge.input, &event);
    }
    out_raw->quit_requested = app->ui_state_bridge.input.quit;
}

static void app_runtime_input_normalize(AppState *app,
                                        const AppInputEventRaw *raw,
                                        AppInputEventNormalized *out_normalized) {
    if (!app || !raw || !out_normalized) {
        return;
    }
    const InputState *input = &app->ui_state_bridge.input;
    /*
     * IR1 policy gate should only engage when app-owned text-entry focus exists.
     * Map Forge does not currently route any editable text field through input,
     * so SDL global text-input state must not suppress global shortcuts.
     */
    bool text_entry_gate_active = false;
    uint32_t blocked_shortcuts = app_runtime_apply_text_entry_shortcut_policy(
        &app->ui_state_bridge.input, text_entry_gate_active);
    uint32_t keyboard_actions = 0u;
    keyboard_actions += app_runtime_count_bool(input->quit);
    keyboard_actions += app_runtime_count_bool(input->toggle_debug_pressed);
    keyboard_actions += app_runtime_count_bool(input->toggle_single_line_pressed);
    keyboard_actions += app_runtime_count_bool(input->toggle_region_pressed);
    keyboard_actions += app_runtime_count_bool(input->toggle_profile_pressed);
    keyboard_actions += app_runtime_count_bool(input->toggle_playback_pressed);
    keyboard_actions += app_runtime_count_bool(input->toggle_landuse_pressed);
    keyboard_actions += app_runtime_count_bool(input->toggle_building_fill_pressed);
    keyboard_actions += app_runtime_count_bool(input->toggle_polygon_outline_pressed);
    keyboard_actions += app_runtime_count_bool(input->theme_cycle_next_pressed);
    keyboard_actions += app_runtime_count_bool(input->theme_cycle_prev_pressed);
    keyboard_actions += app_runtime_count_bool(input->playback_step_forward);
    keyboard_actions += app_runtime_count_bool(input->playback_step_back);
    keyboard_actions += app_runtime_count_bool(input->playback_speed_up);
    keyboard_actions += app_runtime_count_bool(input->playback_speed_down);
    keyboard_actions += app_runtime_count_bool(input->font_zoom_in_pressed);
    keyboard_actions += app_runtime_count_bool(input->font_zoom_out_pressed);
    keyboard_actions += app_runtime_count_bool(input->font_zoom_reset_pressed);
    keyboard_actions += app_runtime_count_bool(input->enter_pressed);
    keyboard_actions += app_runtime_count_bool(input->copy_overlay_pressed);
    keyboard_actions += app_runtime_count_bool(input->pan_left);
    keyboard_actions += app_runtime_count_bool(input->pan_right);
    keyboard_actions += app_runtime_count_bool(input->pan_up);
    keyboard_actions += app_runtime_count_bool(input->pan_down);

    uint32_t pointer_actions = 0u;
    pointer_actions += app_runtime_count_bool(input->mouse_dx != 0 || input->mouse_dy != 0);
    pointer_actions += app_runtime_count_bool(input->mouse_wheel_y != 0);
    pointer_actions += app_runtime_count_bool(input->left_click_pressed);
    pointer_actions += app_runtime_count_bool(input->right_click_pressed);
    pointer_actions += app_runtime_count_bool(input->middle_click_pressed);
    pointer_actions += app_runtime_count_bool(input->left_click_released);
    pointer_actions += app_runtime_count_bool(input->right_click_released);

    out_normalized->text_entry_gate_active = text_entry_gate_active;
    out_normalized->has_keyboard_actions = (keyboard_actions > 0u);
    out_normalized->has_pointer_actions = (pointer_actions > 0u);
    out_normalized->has_global_shortcut_actions =
        input->toggle_debug_pressed ||
        input->toggle_single_line_pressed ||
        input->toggle_region_pressed ||
        input->toggle_profile_pressed ||
        input->toggle_landuse_pressed ||
        input->toggle_building_fill_pressed ||
        input->toggle_polygon_outline_pressed ||
        input->theme_cycle_next_pressed ||
        input->theme_cycle_prev_pressed ||
        input->font_zoom_in_pressed ||
        input->font_zoom_out_pressed ||
        input->font_zoom_reset_pressed;
    out_normalized->action_count = keyboard_actions + pointer_actions;
    out_normalized->immediate_count = out_normalized->action_count;
    out_normalized->queued_count = 0u;
    out_normalized->ignored_count = blocked_shortcuts;
    if (raw->sdl_event_count > out_normalized->action_count) {
        uint32_t residual_ignored = raw->sdl_event_count - out_normalized->action_count;
        out_normalized->ignored_count += residual_ignored;
    }
}

static void app_runtime_input_route(const AppInputEventNormalized *normalized,
                                    AppInputRouteResult *out_route) {
    if (!normalized || !out_route) {
        return;
    }
    out_route->consumed = false;
    out_route->target_policy = APP_INPUT_ROUTE_TARGET_FALLBACK;

    if (normalized->has_global_shortcut_actions) {
        out_route->routed_global_count = normalized->immediate_count;
        out_route->consumed = true;
        out_route->target_policy = APP_INPUT_ROUTE_TARGET_GLOBAL;
        return;
    }
    if (normalized->has_pointer_actions) {
        out_route->routed_pane_count = normalized->immediate_count;
        out_route->consumed = true;
        out_route->target_policy = APP_INPUT_ROUTE_TARGET_FOCUSED_PANE;
        return;
    }
    if (normalized->immediate_count > 0u) {
        out_route->routed_fallback_count = normalized->immediate_count;
        out_route->consumed = true;
        out_route->target_policy = APP_INPUT_ROUTE_TARGET_FALLBACK;
    }
}

static void app_runtime_input_invalidate(const AppRuntimeInputFrame *input,
                                         AppInputInvalidationResult *out_invalidation) {
    if (!input || !out_invalidation) {
        return;
    }
    if (input->raw.quit_requested) {
        out_invalidation->invalidation_reason_bits |= APP_INPUT_INVALIDATE_REASON_QUIT;
        out_invalidation->full_invalidation_count += 1u;
        out_invalidation->full_invalidate = true;
    }
    if (input->raw.window_event_count > 0u) {
        out_invalidation->invalidation_reason_bits |= APP_INPUT_INVALIDATE_REASON_WINDOW;
        out_invalidation->full_invalidation_count += 1u;
        out_invalidation->full_invalidate = true;
    }
    if (input->route.routed_global_count > 0u) {
        out_invalidation->invalidation_reason_bits |= APP_INPUT_INVALIDATE_REASON_GLOBAL;
        out_invalidation->target_invalidation_count += input->route.routed_global_count;
    }
    if (input->normalized.has_keyboard_actions) {
        out_invalidation->invalidation_reason_bits |= APP_INPUT_INVALIDATE_REASON_KEYBOARD;
    }
    if (input->normalized.has_pointer_actions) {
        out_invalidation->invalidation_reason_bits |= APP_INPUT_INVALIDATE_REASON_POINTER;
    }
    if (input->raw.wheel_event_count > 0u) {
        out_invalidation->invalidation_reason_bits |= APP_INPUT_INVALIDATE_REASON_WHEEL;
    }
    out_invalidation->target_invalidation_count += input->route.routed_pane_count;
    out_invalidation->target_invalidation_count += input->route.routed_fallback_count;
}

void app_runtime_process_input_frame(AppState *app,
                                     AppRuntimeInputFrame *out_input,
                                     double *out_frame_begin,
                                     double *out_after_events) {
    AppRuntimeInputFrame local_input = {0};
    if (!out_input) {
        out_input = &local_input;
    }
    memset(out_input, 0, sizeof(*out_input));

    if (!app) {
        if (out_frame_begin) {
            *out_frame_begin = 0.0;
        }
        if (out_after_events) {
            *out_after_events = 0.0;
        }
        return;
    }

    double frame_begin = time_now_seconds();
    memset(&app->frame_timings, 0, sizeof(app->frame_timings));
    app_runtime_input_intake(app, &out_input->raw);
    app_runtime_input_normalize(app, &out_input->raw, &out_input->normalized);
    app_runtime_input_route(&out_input->normalized, &out_input->route);
    app_runtime_input_invalidate(out_input, &out_input->invalidation);
    app_bridge_sync_to_legacy(app);
    app_bridge_sync_from_legacy(app);
    double after_events = time_now_seconds();

    if (out_frame_begin) {
        *out_frame_begin = frame_begin;
    }
    if (out_after_events) {
        *out_after_events = after_events;
    }
}

void app_runtime_begin_frame(AppState *app, double *out_frame_begin, double *out_after_events) {
    app_runtime_process_input_frame(app, NULL, out_frame_begin, out_after_events);
}
