#include "kit_pane.h"

#include <stdio.h>

static int test_chrome_draw_emits_expected_commands(void) {
    KitRenderContext render_ctx;
    KitRenderCommand commands[16];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    KitPaneStyle style;
    KitPaneChrome chrome;
    CoreResult result;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) {
        return 1;
    }

    command_buffer.commands = commands;
    command_buffer.capacity = 16;
    command_buffer.count = 0;

    result = kit_render_begin_frame(&render_ctx, 800, 600, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    kit_pane_style_default(&style);
    chrome.pane_id = 42u;
    chrome.title = "Data View";
    chrome.bounds = (KitRenderRect){ 20.0f, 40.0f, 320.0f, 240.0f };
    chrome.state = KIT_PANE_STATE_FOCUSED;
    chrome.show_header = 1;
    chrome.show_id = 1;
    chrome.authoring_selected = 1;

    result = kit_pane_draw_chrome(&render_ctx, &frame, &style, &chrome);
    if (result.code != CORE_OK) {
        return 1;
    }

    if (command_buffer.count < 5u) {
        fprintf(stderr, "expected at least 5 commands, got %zu\n", command_buffer.count);
        return 1;
    }
    if (command_buffer.commands[0].kind != KIT_RENDER_CMD_RECT) {
        return 1;
    }

    result = kit_render_end_frame(&render_ctx, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    return 0;
}

static int test_splitter_draw_state_changes(void) {
    KitRenderContext render_ctx;
    KitRenderCommand commands[8];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    CoreResult result;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_IDE_GRAY,
                                     CORE_FONT_PRESET_IDE);
    if (result.code != CORE_OK) {
        return 1;
    }

    command_buffer.commands = commands;
    command_buffer.capacity = 8;
    command_buffer.count = 0;

    result = kit_render_begin_frame(&render_ctx, 640, 480, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    result = kit_pane_draw_splitter(&render_ctx,
                                    &frame,
                                    (KitRenderRect){ 100.0f, 20.0f, 6.0f, 200.0f },
                                    0,
                                    0);
    if (result.code != CORE_OK) {
        return 1;
    }

    result = kit_pane_draw_splitter(&render_ctx,
                                    &frame,
                                    (KitRenderRect){ 110.0f, 20.0f, 6.0f, 200.0f },
                                    1,
                                    0);
    if (result.code != CORE_OK) {
        return 1;
    }

    result = kit_pane_draw_splitter(&render_ctx,
                                    &frame,
                                    (KitRenderRect){ 120.0f, 20.0f, 6.0f, 200.0f },
                                    1,
                                    1);
    if (result.code != CORE_OK) {
        return 1;
    }

    if (command_buffer.count != 3u) {
        fprintf(stderr, "expected 3 splitter commands, got %zu\n", command_buffer.count);
        return 1;
    }

    result = kit_render_end_frame(&render_ctx, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    return 0;
}

int main(void) {
    if (test_chrome_draw_emits_expected_commands() != 0) {
        return 1;
    }
    if (test_splitter_draw_state_changes() != 0) {
        return 1;
    }

    puts("kit_pane tests passed");
    return 0;
}
