#include "kit_workspace_authoring.h"

#include <stddef.h>

CorePaneRect kit_workspace_authoring_root_bounds(int width, int height) {
    CorePaneRect bounds = {0};
    bounds.x = 0.0f;
    bounds.y = 0.0f;
    bounds.width = (float)width;
    bounds.height = (float)height;
    return bounds;
}

int kit_workspace_authoring_pane_overlay_active(int layout_mode,
                                                int authoring_layout_mode_value,
                                                int overlay_mode,
                                                int pane_overlay_mode_value) {
    if (layout_mode != authoring_layout_mode_value) {
        return 0;
    }
    return overlay_mode == pane_overlay_mode_value;
}

CoreResult kit_workspace_authoring_execute_action(void *host_context,
                                                  const KitWorkspaceAuthoringActionHooks *hooks,
                                                  const char *action_id,
                                                  uint32_t *io_selected_pane_id,
                                                  int *io_pending_apply,
                                                  int layout_mode,
                                                  int authoring_layout_mode_value,
                                                  int overlay_mode,
                                                  int pane_overlay_mode_value) {
    CoreResult result = core_result_ok();

    if (!hooks || !hooks->execute_action || !action_id || !io_selected_pane_id || !io_pending_apply) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid execute action request" };
    }

    result = hooks->execute_action(host_context, action_id, io_selected_pane_id, io_pending_apply);
    if (result.code != CORE_OK) {
        return result;
    }

    if (hooks->close_picker &&
        !kit_workspace_authoring_pane_overlay_active(layout_mode,
                                                     authoring_layout_mode_value,
                                                     overlay_mode,
                                                     pane_overlay_mode_value)) {
        hooks->close_picker(host_context);
    }

    return core_result_ok();
}

CoreResult kit_workspace_authoring_apply_text_size_step(void *host_context,
                                                        const KitWorkspaceAuthoringTextStepHooks *hooks,
                                                        int target_step,
                                                        int *out_applied_step) {
    CoreResult result = core_result_ok();
    int clamped_step = 0;

    if (!hooks || !hooks->clamp_step || !hooks->set_step) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid apply text-size request" };
    }

    clamped_step = hooks->clamp_step(host_context, target_step);
    result = hooks->set_step(host_context, clamped_step);
    if (result.code != CORE_OK) {
        return result;
    }

    if (hooks->persist_step) {
        hooks->persist_step(host_context);
    }
    if (out_applied_step) {
        *out_applied_step = clamped_step;
    }
    return core_result_ok();
}

CoreResult kit_workspace_authoring_adjust_text_size_step(void *host_context,
                                                         const KitWorkspaceAuthoringTextStepHooks *hooks,
                                                         int direction,
                                                         int *out_applied_step) {
    int step = 0;
    int increment = 1;
    if (!hooks || !hooks->current_step || !hooks->step_increment || direction == 0) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid adjust text-size request" };
    }

    step = hooks->current_step(host_context);
    increment = hooks->step_increment(host_context);
    return kit_workspace_authoring_apply_text_size_step(host_context,
                                                        hooks,
                                                        step + (increment * direction),
                                                        out_applied_step);
}

CoreResult kit_workspace_authoring_reset_text_size_step(void *host_context,
                                                        const KitWorkspaceAuthoringTextStepHooks *hooks,
                                                        int *out_applied_step) {
    int default_step = 0;
    if (!hooks || !hooks->default_step) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid reset text-size request" };
    }
    default_step = hooks->default_step(host_context);
    return kit_workspace_authoring_apply_text_size_step(host_context,
                                                        hooks,
                                                        default_step,
                                                        out_applied_step);
}

const char *kit_workspace_authoring_trigger_from_key(KitWorkspaceAuthoringKey key, uint32_t mod_bits) {
    const int pane_action_mod_held = ((mod_bits & (KIT_WORKSPACE_AUTHORING_MOD_SHIFT |
                                                   KIT_WORKSPACE_AUTHORING_MOD_ALT)) != 0u);
    switch (key) {
        case KIT_WORKSPACE_AUTHORING_KEY_TAB:
            return "tab";
        case KIT_WORKSPACE_AUTHORING_KEY_ENTER:
            return "enter";
        case KIT_WORKSPACE_AUTHORING_KEY_ESCAPE:
            return "esc";
        case KIT_WORKSPACE_AUTHORING_KEY_H:
            if (pane_action_mod_held) return NULL;
            return "h";
        case KIT_WORKSPACE_AUTHORING_KEY_V:
            if (pane_action_mod_held) return NULL;
            return "v";
        case KIT_WORKSPACE_AUTHORING_KEY_X:
            if (pane_action_mod_held) return NULL;
            return "x";
        case KIT_WORKSPACE_AUTHORING_KEY_BACKSPACE:
            return "backspace";
        case KIT_WORKSPACE_AUTHORING_KEY_R:
            if (pane_action_mod_held) return NULL;
            return "r";
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_0:
            if (pane_action_mod_held) return NULL;
            return "0";
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_1:
            if (pane_action_mod_held) return NULL;
            return "1";
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_2:
            if (pane_action_mod_held) return NULL;
            return "2";
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_3:
            if (pane_action_mod_held) return NULL;
            return "3";
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_4:
            if (pane_action_mod_held) return NULL;
            return "4";
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_5:
            if (pane_action_mod_held) return NULL;
            return "5";
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_6:
            if (pane_action_mod_held) return NULL;
            return "6";
        default:
            return NULL;
    }
}

int kit_workspace_authoring_entry_chord_pressed(KitWorkspaceAuthoringKey key,
                                                uint32_t mod_bits,
                                                int key_c_down,
                                                int key_v_down) {
    if ((mod_bits & KIT_WORKSPACE_AUTHORING_MOD_ALT) == 0u) {
        return 0;
    }
    if ((mod_bits & (KIT_WORKSPACE_AUTHORING_MOD_SHIFT |
                     KIT_WORKSPACE_AUTHORING_MOD_CTRL |
                     KIT_WORKSPACE_AUTHORING_MOD_GUI)) != 0u) {
        return 0;
    }
    if (key != KIT_WORKSPACE_AUTHORING_KEY_C && key != KIT_WORKSPACE_AUTHORING_KEY_V) {
        return 0;
    }
    return (key_c_down && key_v_down) ? 1 : 0;
}
