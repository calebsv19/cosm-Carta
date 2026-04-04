#include "app/app_runtime_input_policy.h"

static uint32_t app_runtime_clear_shortcut_flag(bool *flag) {
    if (!flag || !(*flag)) {
        return 0u;
    }
    *flag = false;
    return 1u;
}

uint32_t app_runtime_apply_text_entry_shortcut_policy(InputState *input,
                                                      bool text_entry_active) {
    if (!input || !text_entry_active) {
        return 0u;
    }

    uint32_t blocked_count = 0u;
    blocked_count += app_runtime_clear_shortcut_flag(&input->toggle_debug_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->toggle_single_line_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->toggle_region_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->toggle_profile_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->toggle_landuse_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->toggle_building_fill_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->toggle_polygon_outline_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->theme_cycle_next_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->theme_cycle_prev_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->copy_overlay_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->font_zoom_in_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->font_zoom_out_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->font_zoom_reset_pressed);
    return blocked_count;
}
