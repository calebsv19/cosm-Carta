#include "app/app_runtime_input_policy.h"

#include <assert.h>
#include <stdio.h>

static void test_text_entry_blocks_global_shortcuts(void) {
    InputState input = {0};
    input.toggle_debug_pressed = true;
    input.theme_cycle_next_pressed = true;
    input.font_zoom_in_pressed = true;
    input.font_zoom_out_pressed = true;
    input.font_zoom_reset_pressed = true;
    input.copy_overlay_pressed = true;
    input.pan_left = true;
    input.left_click_pressed = true;

    uint32_t blocked = app_runtime_apply_text_entry_shortcut_policy(&input, true);
    assert(blocked == 6u);
    assert(!input.toggle_debug_pressed);
    assert(!input.theme_cycle_next_pressed);
    assert(!input.font_zoom_in_pressed);
    assert(!input.font_zoom_out_pressed);
    assert(!input.font_zoom_reset_pressed);
    assert(!input.copy_overlay_pressed);
    assert(input.pan_left);
    assert(input.left_click_pressed);
}

static void test_shortcuts_preserved_when_text_entry_inactive(void) {
    InputState input = {0};
    input.toggle_profile_pressed = true;
    input.theme_cycle_prev_pressed = true;
    input.font_zoom_out_pressed = true;

    uint32_t blocked = app_runtime_apply_text_entry_shortcut_policy(&input, false);
    assert(blocked == 0u);
    assert(input.toggle_profile_pressed);
    assert(input.theme_cycle_prev_pressed);
    assert(input.font_zoom_out_pressed);
}

int main(void) {
    test_text_entry_blocks_global_shortcuts();
    test_shortcuts_preserved_when_text_entry_inactive();
    printf("app_runtime_input_policy_test: success\n");
    return 0;
}
