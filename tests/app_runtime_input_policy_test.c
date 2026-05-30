#include "app/app_runtime_input_policy.h"

#include <assert.h>
#include <stdio.h>

static void test_text_entry_blocks_global_shortcuts(void) {
    InputState input = {0};
    input.toggle_debug_pressed = true;
    input.theme_cycle_next_pressed = true;
    input.zoom_step_in_pressed = true;
    input.zoom_step_out_pressed = true;
    input.font_zoom_in_pressed = true;
    input.font_zoom_out_pressed = true;
    input.font_zoom_reset_pressed = true;
    input.toggle_follow_preview_pressed = true;
    input.toggle_follow_heading_mode_pressed = true;
    input.copy_overlay_pressed = true;
    input.pin_panel_toggle_pressed = true;
    input.ingest_panel_toggle_pressed = true;
    input.pan_left = true;
    input.pan_up = true;
    input.left_click_pressed = true;

    uint32_t blocked = app_runtime_apply_text_entry_shortcut_policy(&input, true, false);
    assert(blocked == 13u);
    assert(!input.toggle_debug_pressed);
    assert(!input.theme_cycle_next_pressed);
    assert(!input.zoom_step_in_pressed);
    assert(!input.zoom_step_out_pressed);
    assert(!input.font_zoom_in_pressed);
    assert(!input.font_zoom_out_pressed);
    assert(!input.font_zoom_reset_pressed);
    assert(!input.toggle_follow_preview_pressed);
    assert(!input.toggle_follow_heading_mode_pressed);
    assert(!input.copy_overlay_pressed);
    assert(!input.pin_panel_toggle_pressed);
    assert(!input.ingest_panel_toggle_pressed);
    assert(input.pan_left);
    assert(!input.pan_up);
    assert(input.left_click_pressed);
}

static void test_pin_name_focus_preserves_caret_keys(void) {
    InputState input = {0};
    input.cursor_left_pressed = true;
    input.cursor_right_pressed = true;
    input.pan_left = true;
    input.pan_right = true;
    input.pin_panel_toggle_pressed = true;

    uint32_t blocked = app_runtime_apply_text_entry_shortcut_policy(&input, true, true);
    assert(blocked == 1u);
    assert(input.cursor_left_pressed);
    assert(input.cursor_right_pressed);
    assert(input.pan_left);
    assert(input.pan_right);
    assert(!input.pin_panel_toggle_pressed);
}

static void test_shortcuts_preserved_when_text_entry_inactive(void) {
    InputState input = {0};
    input.toggle_profile_pressed = true;
    input.theme_cycle_prev_pressed = true;
    input.zoom_step_out_pressed = true;
    input.font_zoom_out_pressed = true;

    uint32_t blocked = app_runtime_apply_text_entry_shortcut_policy(&input, false, false);
    assert(blocked == 0u);
    assert(input.toggle_profile_pressed);
    assert(input.theme_cycle_prev_pressed);
    assert(input.zoom_step_out_pressed);
    assert(input.font_zoom_out_pressed);
}

int main(void) {
    test_text_entry_blocks_global_shortcuts();
    test_pin_name_focus_preserves_caret_keys();
    test_shortcuts_preserved_when_text_entry_inactive();
    printf("app_runtime_input_policy_test: success\n");
    return 0;
}
