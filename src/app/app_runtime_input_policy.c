#include "app/app_runtime_input_policy.h"

static uint32_t app_runtime_clear_shortcut_flag(bool *flag) {
    if (!flag || !(*flag)) {
        return 0u;
    }
    *flag = false;
    return 1u;
}

uint32_t app_runtime_apply_text_entry_shortcut_policy(InputState *input,
                                                      bool text_entry_active,
                                                      bool preserve_horizontal_caret_keys) {
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
    blocked_count += app_runtime_clear_shortcut_flag(&input->toggle_follow_preview_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->toggle_follow_heading_mode_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->theme_cycle_next_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->theme_cycle_prev_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->zoom_step_in_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->zoom_step_out_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->copy_overlay_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->font_zoom_in_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->font_zoom_out_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->font_zoom_reset_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->ingest_panel_toggle_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->ingest_tab_toggle_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->ingest_select_prev_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->ingest_select_next_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->ingest_import_all_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->ingest_edit_toggle_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->ingest_folder_dialog_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->pin_panel_toggle_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->toggle_playback_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->playback_step_forward);
    blocked_count += app_runtime_clear_shortcut_flag(&input->playback_step_back);
    blocked_count += app_runtime_clear_shortcut_flag(&input->playback_speed_up);
    blocked_count += app_runtime_clear_shortcut_flag(&input->playback_speed_down);
    blocked_count += app_runtime_clear_shortcut_flag(&input->rotate_heading_left_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->rotate_heading_right_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->rotate_heading_reset_pressed);
    blocked_count += app_runtime_clear_shortcut_flag(&input->pan_up);
    blocked_count += app_runtime_clear_shortcut_flag(&input->pan_down);
    blocked_count += app_runtime_clear_shortcut_flag(&input->quit);
    if (!preserve_horizontal_caret_keys) {
        blocked_count += app_runtime_clear_shortcut_flag(&input->cursor_left_pressed);
        blocked_count += app_runtime_clear_shortcut_flag(&input->cursor_right_pressed);
    }
    return blocked_count;
}
