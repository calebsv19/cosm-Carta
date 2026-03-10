#include "kit_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int nearf(float a, float b) {
    return fabsf(a - b) < 1e-5f;
}

static int test_theme_scaled_style(void) {
    KitRenderContext render_ctx;
    KitUiContext ui_ctx;
    CoreResult result;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_context_init(&ui_ctx, &render_ctx);
    if (result.code != CORE_OK) return 1;

    if (!nearf(ui_ctx.style.padding, 6.0f)) {
        fprintf(stderr, "expected theme-scaled padding 6.0, got %.3f\n", ui_ctx.style.padding);
        return 1;
    }
    if (!nearf(ui_ctx.style.gap, 10.0f)) {
        fprintf(stderr, "expected theme-scaled gap 10.0, got %.3f\n", ui_ctx.style.gap);
        return 1;
    }
    if (!nearf(ui_ctx.style.control_height, 32.0f)) {
        fprintf(stderr, "expected theme-scaled control_height 32.0, got %.3f\n", ui_ctx.style.control_height);
        return 1;
    }

    result = kit_render_set_theme_preset(&render_ctx, CORE_THEME_PRESET_IDE_GRAY);
    if (result.code != CORE_OK) return 1;
    result = kit_ui_style_apply_theme_scale(&ui_ctx);
    if (result.code != CORE_OK) return 1;

    if (!nearf(ui_ctx.style.padding, 6.0f)) return 1;
    if (!nearf(ui_ctx.style.checkbox_size, 18.0f)) return 1;
    if (!nearf(ui_ctx.style.slider_track_height, 6.0f)) return 1;
    return 0;
}

static int test_stack_layout(void) {
    KitUiStackLayout layout;
    KitRenderRect r0;
    KitRenderRect r1;
    CoreResult result;

    result = kit_ui_stack_begin(&layout,
                                KIT_UI_AXIS_VERTICAL,
                                (KitRenderRect){10.0f, 20.0f, 200.0f, 400.0f},
                                6.0f);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_stack_next(&layout, 30.0f, 0.0f, &r0);
    if (result.code != CORE_OK) return 1;
    result = kit_ui_stack_next(&layout, 40.0f, 120.0f, &r1);
    if (result.code != CORE_OK) return 1;

    if (!nearf(r0.x, 10.0f) || !nearf(r0.y, 20.0f) || !nearf(r0.width, 200.0f) || !nearf(r0.height, 30.0f)) {
        fprintf(stderr, "unexpected r0 rect: %.3f %.3f %.3f %.3f\n", r0.x, r0.y, r0.width, r0.height);
        return 1;
    }
    if (!nearf(r1.x, 10.0f) || !nearf(r1.y, 56.0f) || !nearf(r1.width, 120.0f) || !nearf(r1.height, 40.0f)) {
        fprintf(stderr, "unexpected r1 rect: %.3f %.3f %.3f %.3f\n", r1.x, r1.y, r1.width, r1.height);
        return 1;
    }
    return 0;
}

static int test_widget_rendering(void) {
    KitRenderContext render_ctx;
    KitUiContext ui_ctx;
    KitRenderCommand storage[64];
    KitRenderCommandBuffer buffer;
    KitRenderFrame frame;
    const char *labels[3] = {"Bars", "Trend", "Mix"};
    CoreResult result;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_context_init(&ui_ctx, &render_ctx);
    if (result.code != CORE_OK) return 1;

    buffer.commands = storage;
    buffer.capacity = 64;
    buffer.count = 0;
    result = kit_render_begin_frame(&render_ctx, 800, 600, &buffer, &frame);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_button(&ui_ctx,
                                &frame,
                                (KitRenderRect){10.0f, 10.0f, 160.0f, 32.0f},
                                "Apply",
                                KIT_UI_STATE_HOVERED);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_checkbox(&ui_ctx,
                                  &frame,
                                  (KitRenderRect){10.0f, 52.0f, 220.0f, 32.0f},
                                  "Enabled",
                                  1,
                                  KIT_UI_STATE_NORMAL);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_slider(&ui_ctx,
                                &frame,
                                (KitRenderRect){10.0f, 94.0f, 240.0f, 24.0f},
                                0.25f,
                                KIT_UI_STATE_ACTIVE);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_scrollbar(&ui_ctx,
                                   &frame,
                                   (KitRenderRect){260.0f, 20.0f, 120.0f, 120.0f},
                                   24.0f,
                                   260.0f);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_segmented(&ui_ctx,
                                   &frame,
                                   (KitRenderRect){10.0f, 132.0f, 300.0f, 32.0f},
                                   labels,
                                   3,
                                   1,
                                   2,
                                   1);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_label_custom(&ui_ctx,
                                      &frame,
                                      (KitRenderRect){10.0f, 180.0f, 220.0f, 28.0f},
                                      "Section",
                                      CORE_THEME_COLOR_TEXT_PRIMARY,
                                      CORE_FONT_ROLE_UI_BOLD,
                                      CORE_FONT_TEXT_SIZE_TITLE);
    if (result.code != CORE_OK) return 1;

    if (buffer.count < 14) {
        fprintf(stderr, "expected at least 14 commands, got %zu\n", buffer.count);
        return 1;
    }
    if (buffer.commands[0].kind != KIT_RENDER_CMD_RECT) return 1;
    if (buffer.commands[1].kind != KIT_RENDER_CMD_TEXT) return 1;
    if (buffer.commands[3].kind != KIT_RENDER_CMD_LINE) return 1;
    if (buffer.commands[7].kind != KIT_RENDER_CMD_RECT) return 1;
    if (buffer.commands[buffer.count - 1].kind != KIT_RENDER_CMD_TEXT) return 1;
    if (buffer.commands[buffer.count - 1].data.text.font_role != CORE_FONT_ROLE_UI_BOLD) return 1;
    if (buffer.commands[buffer.count - 1].data.text.text_tier != CORE_FONT_TEXT_SIZE_TITLE) return 1;

    result = kit_render_end_frame(&render_ctx, &frame);
    if (result.code != CORE_OK) return 1;
    return 0;
}

static int test_input_and_behaviors(void) {
    KitUiInputState input;
    KitRenderRect bounds = {10.0f, 20.0f, 100.0f, 30.0f};
    KitUiButtonResult button_result;
    KitUiCheckboxResult checkbox_result;
    KitUiSliderResult slider_result;
    KitUiScrollResult scroll_result;
    KitUiSegmentedResult segmented_result;

    input.mouse_x = 40.0f;
    input.mouse_y = 35.0f;
    input.mouse_down = 0;
    input.mouse_pressed = 0;
    input.mouse_released = 0;

    if (!kit_ui_point_in_rect(bounds, input.mouse_x, input.mouse_y)) return 1;
    if (kit_ui_state_for_input(bounds, &input, 1) != KIT_UI_STATE_HOVERED) return 1;

    input.mouse_down = 1;
    if (kit_ui_state_for_input(bounds, &input, 1) != KIT_UI_STATE_ACTIVE) return 1;

    input.mouse_down = 0;
    input.mouse_released = 1;
    button_result = kit_ui_eval_button(bounds, &input, 1);
    if (!button_result.clicked) return 1;
    if (button_result.state != KIT_UI_STATE_HOVERED) return 1;

    checkbox_result = kit_ui_eval_checkbox(bounds, &input, 1, 0);
    if (!checkbox_result.toggled || !checkbox_result.checked) return 1;

    input.mouse_released = 0;
    input.mouse_down = 1;
    input.mouse_x = 85.0f;
    slider_result = kit_ui_eval_slider(bounds, &input, 1, 0.1f);
    if (!slider_result.changed) return 1;
    if (!nearf(slider_result.value_01, 0.75f)) return 1;

    scroll_result = kit_ui_eval_scroll((KitRenderRect){0.0f, 0.0f, 120.0f, 80.0f}, 12.0f, 220.0f, -2.0f);
    if (!scroll_result.changed) return 1;
    if (!nearf(scroll_result.offset_y, 60.0f)) return 1;

    input.mouse_down = 0;
    input.mouse_released = 1;
    input.mouse_x = 88.0f;
    input.mouse_y = 18.0f;
    segmented_result = kit_ui_eval_segmented((KitRenderRect){0.0f, 0.0f, 120.0f, 30.0f}, &input, 1, 3, 0);
    if (!segmented_result.changed) return 1;
    if (segmented_result.selected_index != 2) return 1;
    if (segmented_result.clicked_index != 2) return 1;

    if (kit_ui_state_for_input(bounds, &input, 0) != KIT_UI_STATE_DISABLED) return 1;
    return 0;
}

static int test_scroll_top_anchor_content_height(void) {
    float content_height;
    float max_offset;

    content_height = kit_ui_scroll_content_height_top_anchor(4, 44.0f, 200.0f);
    if (!nearf(content_height, 332.0f)) return 1;
    max_offset = content_height - 200.0f;
    if (!nearf(max_offset, 132.0f)) return 1;

    content_height = kit_ui_scroll_content_height_top_anchor(1, 44.0f, 200.0f);
    if (!nearf(content_height, 200.0f)) return 1;

    content_height = kit_ui_scroll_content_height_top_anchor(0, 44.0f, 200.0f);
    if (!nearf(content_height, 0.0f)) return 1;

    return 0;
}

static int test_clip_stack_push_pop(void) {
    KitRenderContext render_ctx;
    KitUiContext ui_ctx;
    KitRenderCommand storage[32];
    KitRenderCommandBuffer buffer;
    KitRenderFrame frame;
    CoreResult result;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_context_init(&ui_ctx, &render_ctx);
    if (result.code != CORE_OK) return 1;

    buffer.commands = storage;
    buffer.capacity = 32;
    buffer.count = 0;
    result = kit_render_begin_frame(&render_ctx, 800, 600, &buffer, &frame);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_clip_push(&ui_ctx,
                              &frame,
                              (KitRenderRect){10.2f, 20.2f, 90.4f, 30.4f});
    if (result.code != CORE_OK) return 1;

    result = kit_ui_clip_push(&ui_ctx,
                              &frame,
                              (KitRenderRect){50.9f, 25.1f, 100.0f, 100.0f});
    if (result.code != CORE_OK) return 1;

    result = kit_ui_clip_pop(&ui_ctx, &frame);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_clip_pop(&ui_ctx, &frame);
    if (result.code != CORE_OK) return 1;

    if (buffer.count != 4) {
        fprintf(stderr, "expected 4 clip commands, got %zu\n", buffer.count);
        return 1;
    }

    if (buffer.commands[0].kind != KIT_RENDER_CMD_SET_CLIP) return 1;
    if (buffer.commands[1].kind != KIT_RENDER_CMD_SET_CLIP) return 1;
    if (buffer.commands[2].kind != KIT_RENDER_CMD_SET_CLIP) return 1;
    if (buffer.commands[3].kind != KIT_RENDER_CMD_CLEAR_CLIP) return 1;

    if (!nearf(buffer.commands[0].data.clip.rect.x, 10.0f)) return 1;
    if (!nearf(buffer.commands[0].data.clip.rect.y, 20.0f)) return 1;
    if (!nearf(buffer.commands[0].data.clip.rect.width, 91.0f)) return 1;
    if (!nearf(buffer.commands[0].data.clip.rect.height, 31.0f)) return 1;

    if (!nearf(buffer.commands[1].data.clip.rect.x, 50.0f)) return 1;
    if (!nearf(buffer.commands[1].data.clip.rect.y, 25.0f)) return 1;
    if (!nearf(buffer.commands[1].data.clip.rect.width, 51.0f)) return 1;
    if (!nearf(buffer.commands[1].data.clip.rect.height, 26.0f)) return 1;

    result = kit_ui_clip_pop(&ui_ctx, &frame);
    if (result.code != CORE_ERR_INVALID_ARG) {
        fprintf(stderr, "expected clip underflow error\n");
        return 1;
    }

    result = kit_render_end_frame(&render_ctx, &frame);
    if (result.code != CORE_OK) return 1;
    return 0;
}

static int test_text_fit_helpers(void) {
    KitRenderContext render_ctx;
    KitUiContext ui_ctx;
    CoreFontTextSizeTier tiers[4] = {
        CORE_FONT_TEXT_SIZE_TITLE,
        CORE_FONT_TEXT_SIZE_PARAGRAPH,
        CORE_FONT_TEXT_SIZE_BASIC,
        CORE_FONT_TEXT_SIZE_CAPTION
    };
    KitUiTextFitResult fit;
    KitRenderTextMetrics metrics;
    char fitted[64];
    CoreResult result;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_context_init(&ui_ctx, &render_ctx);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_measure_text(&ui_ctx,
                                 CORE_FONT_ROLE_UI_REGULAR,
                                 CORE_FONT_TEXT_SIZE_BASIC,
                                 "Node",
                                 &metrics);
    if (result.code != CORE_OK) return 1;
    if (!(metrics.width_px > 0.0f && metrics.height_px > 0.0f)) return 1;

    result = kit_ui_fit_text_to_rect(&ui_ctx,
                                     "Node",
                                     CORE_FONT_ROLE_UI_REGULAR,
                                     tiers,
                                     4u,
                                     160.0f,
                                     28.0f,
                                     fitted,
                                     sizeof(fitted),
                                     &fit);
    if (result.code != CORE_OK) return 1;
    if (strcmp(fitted, "Node") != 0) return 1;
    if (fit.truncated) return 1;
    if (fit.text_tier != CORE_FONT_TEXT_SIZE_TITLE) return 1;

    result = kit_ui_fit_text_to_rect(&ui_ctx,
                                     "VeryLongGraphNodeNameThatNeedsTruncation",
                                     CORE_FONT_ROLE_UI_REGULAR,
                                     tiers,
                                     4u,
                                     72.0f,
                                     17.0f,
                                     fitted,
                                     sizeof(fitted),
                                     &fit);
    if (result.code != CORE_OK) return 1;
    if (!fit.truncated) return 1;
    if (fit.text_tier != CORE_FONT_TEXT_SIZE_CAPTION) return 1;
    if (strstr(fitted, "...") == 0) return 1;

    /* With generous height but narrow width, truncated fallback should still choose
       the smallest tier to avoid zoom-out size jumps. */
    result = kit_ui_fit_text_to_rect(&ui_ctx,
                                     "VeryLongGraphNodeNameThatNeedsTruncation",
                                     CORE_FONT_ROLE_UI_REGULAR,
                                     tiers,
                                     4u,
                                     72.0f,
                                     40.0f,
                                     fitted,
                                     sizeof(fitted),
                                     &fit);
    if (result.code != CORE_OK) return 1;
    if (!fit.truncated) return 1;
    if (fit.text_tier != CORE_FONT_TEXT_SIZE_CAPTION) return 1;

    return 0;
}

int main(void) {
    if (test_theme_scaled_style() != 0) return 1;
    if (test_stack_layout() != 0) return 1;
    if (test_widget_rendering() != 0) return 1;
    if (test_input_and_behaviors() != 0) return 1;
    if (test_scroll_top_anchor_content_height() != 0) return 1;
    if (test_clip_stack_push_pop() != 0) return 1;
    if (test_text_fit_helpers() != 0) return 1;

    puts("kit_ui tests passed");
    return 0;
}
