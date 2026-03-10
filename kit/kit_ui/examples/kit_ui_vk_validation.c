#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>

#include "kit_ui.h"
#include "vk_renderer.h"

static int run_frame(KitRenderContext *render_ctx,
                     KitUiContext *ui_ctx,
                     const KitUiInputState *input,
                     int wheel_y,
                     int *button_flash,
                     int *checked,
                     float *slider_value,
                     int *mode_index,
                     float *event_scroll) {
    KitRenderCommand commands[256];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    KitUiStackLayout layout;
    KitRenderRect panel;
    KitRenderRect row;
    KitRenderRect log_panel;
    KitRenderRect log_viewport;
    CoreResult result;
    KitUiButtonResult button_result;
    KitUiCheckboxResult checkbox_result;
    KitUiSliderResult slider_result;
    KitUiSegmentedResult segmented_result;
    KitUiScrollResult scroll_result;
    const char *modes[3] = {"Bars", "Trend", "Mix"};
    int hovered_segment = -1;
    int i;

    command_buffer.commands = commands;
    command_buffer.capacity = 256;
    command_buffer.count = 0;

    result = kit_render_begin_frame(render_ctx, 1040, 620, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_begin_frame failed: %d\n", (int)result.code);
        return 1;
    }

    result = kit_render_push_clear(&frame, (KitRenderColor){18, 22, 30, 255});
    if (result.code != CORE_OK) return 1;

    panel = (KitRenderRect){40.0f, 40.0f, 360.0f, 540.0f};
    result = kit_render_push_rect(
        &frame,
        &(KitRenderRectCommand){
            panel, 10.0f, {44, 52, 65, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_ui_stack_begin(&layout,
                                KIT_UI_AXIS_VERTICAL,
                                (KitRenderRect){panel.x + 20.0f, panel.y + 20.0f, panel.width - 40.0f, 250.0f},
                                ui_ctx->style.gap);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_stack_next(&layout, 28.0f, 0.0f, &row);
    if (result.code != CORE_OK) return 1;
    result = kit_ui_draw_label(ui_ctx, &frame, row, "KIT UI VALIDATION", CORE_THEME_COLOR_TEXT_PRIMARY);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_stack_next(&layout, ui_ctx->style.control_height, 0.0f, &row);
    if (result.code != CORE_OK) return 1;
    if (kit_ui_point_in_rect(row, input->mouse_x, input->mouse_y)) {
        float segment_w = row.width / 3.0f;
        hovered_segment = (int)((input->mouse_x - row.x) / segment_w);
        if (hovered_segment < 0) hovered_segment = 0;
        if (hovered_segment > 2) hovered_segment = 2;
    }
    segmented_result = kit_ui_eval_segmented(row, input, 1, 3, *mode_index);
    if (segmented_result.clicked_index >= 0) {
        *mode_index = segmented_result.selected_index;
    }
    result = kit_ui_draw_segmented(ui_ctx, &frame, row, modes, 3, *mode_index, hovered_segment, 1);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_stack_next(&layout, ui_ctx->style.control_height, 0.0f, &row);
    if (result.code != CORE_OK) return 1;
    button_result = kit_ui_eval_button(row, input, 1);
    if (button_result.clicked) {
        *button_flash = 18;
    }
    result = kit_ui_draw_button(ui_ctx, &frame, row, "Run Action", button_result.state);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_stack_next(&layout, ui_ctx->style.control_height, 0.0f, &row);
    if (result.code != CORE_OK) return 1;
    checkbox_result = kit_ui_eval_checkbox(row, input, 1, *checked);
    if (checkbox_result.toggled) {
        *checked = checkbox_result.checked;
    }
    result = kit_ui_draw_checkbox(ui_ctx, &frame, row, "Enable Overlay", *checked, checkbox_result.state);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_stack_next(&layout, 24.0f, 0.0f, &row);
    if (result.code != CORE_OK) return 1;
    slider_result = kit_ui_eval_slider(row, input, 1, *slider_value);
    if (slider_result.changed) {
        *slider_value = slider_result.value_01;
    }
    result = kit_ui_draw_slider(ui_ctx, &frame, row, *slider_value, slider_result.state);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_stack_next(&layout, 24.0f, 0.0f, &row);
    if (result.code != CORE_OK) return 1;
    {
        char status[128];
        snprintf(status,
                 sizeof(status),
                 "Mode: %s   Slider: %d%%",
                 modes[*mode_index],
                 (int)(*slider_value * 100.0f + 0.5f));
        result = kit_ui_draw_label(ui_ctx, &frame, row, status, CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
    }

    log_panel = (KitRenderRect){panel.x + 20.0f, panel.y + 310.0f, panel.width - 40.0f, 210.0f};
    result = kit_render_push_rect(
        &frame,
        &(KitRenderRectCommand){
            log_panel, 8.0f, {36, 43, 56, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_label(ui_ctx,
                               &frame,
                               (KitRenderRect){log_panel.x + 12.0f, log_panel.y + 8.0f, log_panel.width - 24.0f, 20.0f},
                               "EVENT LOG (wheel over this panel)",
                               CORE_THEME_COLOR_TEXT_PRIMARY);
    if (result.code != CORE_OK) return 1;

    log_viewport = (KitRenderRect){log_panel.x + 12.0f, log_panel.y + 36.0f, log_panel.width - 24.0f, log_panel.height - 48.0f};
    if (kit_ui_point_in_rect(log_viewport, input->mouse_x, input->mouse_y) && wheel_y != 0) {
        scroll_result = kit_ui_eval_scroll(log_viewport, *event_scroll, 420.0f, (float)wheel_y);
        if (scroll_result.changed) {
            *event_scroll = scroll_result.offset_y;
        }
    }

    result = kit_render_push_set_clip(&frame, log_viewport);
    if (result.code != CORE_OK) return 1;
    for (i = 0; i < 14; ++i) {
        char entry[96];
        float y = log_viewport.y + 6.0f + ((float)i * 28.0f) - *event_scroll;
        snprintf(entry,
                 sizeof(entry),
                 "Entry %02d  mode=%s  overlay=%s",
                 i + 1,
                 modes[*mode_index],
                 *checked ? "on" : "off");
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){log_viewport.x, y, log_viewport.width - 14.0f, 22.0f},
                                   entry,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
    }
    result = kit_render_push_clear_clip(&frame);
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_scrollbar(ui_ctx, &frame, log_viewport, *event_scroll, 420.0f);
    if (result.code != CORE_OK) return 1;

    {
        KitRenderRect viz = {440.0f, 40.0f, 560.0f, 540.0f};
        int bar_count = 10;
        result = kit_render_push_rect(
            &frame,
            &(KitRenderRectCommand){
                viz, 8.0f, {36, 43, 56, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
            });
        if (result.code != CORE_OK) return 1;

        if (*mode_index == 0 || *mode_index == 2) {
            for (i = 0; i < bar_count; ++i) {
                float t = (float)i / (float)(bar_count - 1);
                float h = 30.0f + ((*slider_value * 220.0f) * (0.25f + (0.75f * t)));
                KitRenderRect bar = {
                    viz.x + 26.0f + (float)i * 48.0f,
                    viz.y + viz.height - 36.0f - h,
                    30.0f,
                    h
                };
                uint8_t accent = (uint8_t)(110 + (t * 100.0f));
                result = kit_render_push_rect(
                    &frame,
                    &(KitRenderRectCommand){
                        bar,
                        3.0f,
                        {(uint8_t)(40 + (*button_flash > 0 ? 100 : 0)), accent, 220, 255},
                        {0.0f, 0.0f, 1.0f, 1.0f}
                    });
                if (result.code != CORE_OK) return 1;
            }
        }

        if (*mode_index == 1 || *mode_index == 2) {
            for (i = 0; i < 7; ++i) {
                float x0 = viz.x + 40.0f + (float)i * 72.0f;
                float x1 = x0 + 72.0f;
                float y0 = viz.y + 180.0f + (float)((i % 2) ? 60.0f : -40.0f) * *slider_value;
                float y1 = viz.y + 180.0f + (float)(((i + 1) % 2) ? 60.0f : -40.0f) * *slider_value;
                result = kit_render_push_line(
                    &frame,
                    &(KitRenderLineCommand){
                        {x0, y0}, {x1, y1}, 2.0f, {110, 224, 180, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
                    });
                if (result.code != CORE_OK) return 1;
            }
        }

        if (*checked) {
            result = kit_render_push_text(
                &frame,
                &(KitRenderTextCommand){
                    {viz.x + 24.0f, viz.y + 30.0f},
                    "LIVE OVERLAY",
                    CORE_FONT_ROLE_UI_BOLD,
                    CORE_FONT_TEXT_SIZE_PARAGRAPH,
                    CORE_THEME_COLOR_TEXT_PRIMARY,
                    {0.0f, 0.0f, 1.0f, 1.0f}
                });
            if (result.code != CORE_OK) return 1;
        }
    }

    result = kit_render_end_frame(render_ctx, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_end_frame failed: %d\n", (int)result.code);
        return 1;
    }

    if (*button_flash > 0) {
        *button_flash -= 1;
    }
    return 0;
}

int main(void) {
    SDL_Window *window = 0;
    SDL_Event event;
    bool running = true;
    VkRenderer renderer;
    VkRendererConfig config;
    KitRenderContext render_ctx;
    KitUiContext ui_ctx;
    KitUiInputState input = {0};
    int checked = 1;
    float slider_value = 0.45f;
    int button_flash = 0;
    int mode_index = 0;
    float event_scroll = 0.0f;
    int wheel_y = 0;
    CoreResult result;
    int exit_code = 1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("kit_ui Vulkan Validation",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              1040,
                              620,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    vk_renderer_config_set_defaults(&config);
    config.enable_validation = VK_FALSE;
    if (vk_renderer_init(&renderer, window, &config) != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed\n");
        goto cleanup_window;
    }

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_VULKAN,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_context_init failed: %d\n", (int)result.code);
        goto cleanup_renderer;
    }

    result = kit_render_attach_external_backend(&render_ctx, &renderer);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_attach_external_backend failed: %d\n", (int)result.code);
        goto cleanup_render_ctx;
    }

    result = kit_ui_context_init(&ui_ctx, &render_ctx);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_ui_context_init failed: %d\n", (int)result.code);
        goto cleanup_render_ctx;
    }

    while (running) {
        input.mouse_pressed = 0;
        input.mouse_released = 0;
        wheel_y = 0;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    input.mouse_x = (float)event.motion.x;
                    input.mouse_y = (float)event.motion.y;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        input.mouse_x = (float)event.button.x;
                        input.mouse_y = (float)event.button.y;
                        input.mouse_down = 1;
                        input.mouse_pressed = 1;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        input.mouse_x = (float)event.button.x;
                        input.mouse_y = (float)event.button.y;
                        input.mouse_down = 0;
                        input.mouse_released = 1;
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    wheel_y = event.wheel.y;
                    break;
                default:
                    break;
            }
        }

        if (run_frame(&render_ctx,
                      &ui_ctx,
                      &input,
                      wheel_y,
                      &button_flash,
                      &checked,
                      &slider_value,
                      &mode_index,
                      &event_scroll) != 0) {
            goto cleanup_render_ctx;
        }
    }

    exit_code = 0;

cleanup_render_ctx:
    kit_render_context_shutdown(&render_ctx);
cleanup_renderer:
    vk_renderer_shutdown(&renderer);
cleanup_window:
    SDL_DestroyWindow(window);
cleanup:
    SDL_Quit();
    return exit_code;
}
