#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "kit_graph_timeseries.h"
#include "kit_ui.h"
#include "vk_renderer.h"

static void fill_series(float *xs, float *ys0, float *ys1, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; ++i) {
        float x = (float)i * 0.025f;
        xs[i] = x;
        ys0[i] = sinf(x) + (0.15f * sinf(x * 6.0f));
        ys1[i] = 0.65f * cosf(x * 0.75f);
    }
}

static int run_frame(KitRenderContext *render_ctx,
                     KitUiContext *ui_ctx,
                     const float *xs,
                     const float *ys0,
                     const float *ys1,
                     uint32_t point_count,
                     const KitUiInputState *input,
                     int wheel_y,
                     KitGraphTsView *view,
                     int *series_mode,
                     int *show_overlay,
                     float *detail_value) {
    KitRenderCommand commands[2048];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    KitGraphTsSeries series[2];
    KitGraphTsStyle style;
    KitGraphTsHover hover;
    KitRenderRect plot_bounds = {280.0f, 50.0f, 640.0f, 500.0f};
    KitRenderRect panel = {40.0f, 50.0f, 210.0f, 500.0f};
    CoreResult result;
    uint32_t active_series_count = *series_mode == 2 ? 2u : 1u;
    KitUiSegmentedResult segmented_result;
    KitUiCheckboxResult checkbox_result;
    KitUiSliderResult slider_result;
    int hovered_segment = -1;

    if (wheel_y != 0) {
        float factor = wheel_y > 0 ? 0.85f : 1.15f;
        result = kit_graph_ts_zoom_view(view, factor);
        if (result.code != CORE_OK) return 1;
    }

    series[0].xs = xs;
    series[0].ys = ys0;
    series[0].point_count = point_count;
    series[0].label = "SIN";
    series[0].color = (KitRenderColor){82, 150, 255, 255};
    series[1].xs = xs;
    series[1].ys = ys1;
    series[1].point_count = point_count;
    series[1].label = "COS";
    series[1].color = (KitRenderColor){90, 224, 180, 255};

    command_buffer.commands = commands;
    command_buffer.capacity = 2048;
    command_buffer.count = 0;

    result = kit_render_begin_frame(render_ctx, 960, 600, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_begin_frame failed: %d\n", (int)result.code);
        return 1;
    }

    result = kit_render_push_clear(&frame, (KitRenderColor){16, 20, 28, 255});
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_rect(
        &frame,
        &(KitRenderRectCommand){
            panel, 8.0f, {42, 48, 60, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_label(ui_ctx,
                               &frame,
                               (KitRenderRect){panel.x + 12.0f, panel.y + 10.0f, panel.width - 24.0f, 24.0f},
                               "TS VALIDATION",
                               CORE_THEME_COLOR_TEXT_PRIMARY);
    if (result.code != CORE_OK) return 1;

    if (kit_ui_point_in_rect((KitRenderRect){panel.x + 12.0f, panel.y + 46.0f, panel.width - 24.0f, 32.0f}, input->mouse_x, input->mouse_y)) {
        float w = (panel.width - 24.0f) / 3.0f;
        hovered_segment = (int)((input->mouse_x - (panel.x + 12.0f)) / w);
        if (hovered_segment < 0) hovered_segment = 0;
        if (hovered_segment > 2) hovered_segment = 2;
    }
    segmented_result = kit_ui_eval_segmented((KitRenderRect){panel.x + 12.0f, panel.y + 46.0f, panel.width - 24.0f, 32.0f},
                                             input,
                                             1,
                                             3,
                                             *series_mode);
    if (segmented_result.clicked_index >= 0) {
        *series_mode = segmented_result.selected_index;
        active_series_count = *series_mode == 2 ? 2u : 1u;
    }
    {
        const char *labels[3] = {"SIN", "COS", "BOTH"};
        result = kit_ui_draw_segmented(ui_ctx,
                                       &frame,
                                       (KitRenderRect){panel.x + 12.0f, panel.y + 46.0f, panel.width - 24.0f, 32.0f},
                                       labels,
                                       3,
                                       *series_mode,
                                       hovered_segment,
                                       1);
        if (result.code != CORE_OK) return 1;
    }

    checkbox_result = kit_ui_eval_checkbox((KitRenderRect){panel.x + 12.0f, panel.y + 92.0f, panel.width - 24.0f, 28.0f},
                                           input,
                                           1,
                                           *show_overlay);
    if (checkbox_result.toggled) {
        *show_overlay = checkbox_result.checked;
    }
    result = kit_ui_draw_checkbox(ui_ctx,
                                  &frame,
                                  (KitRenderRect){panel.x + 12.0f, panel.y + 92.0f, panel.width - 24.0f, 28.0f},
                                  "Crosshair Overlay",
                                  *show_overlay,
                                  checkbox_result.state);
    if (result.code != CORE_OK) return 1;

    slider_result = kit_ui_eval_slider((KitRenderRect){panel.x + 12.0f, panel.y + 132.0f, panel.width - 24.0f, 24.0f},
                                       input,
                                       1,
                                       *detail_value);
    if (slider_result.changed) {
        *detail_value = slider_result.value_01;
    }
    result = kit_ui_draw_slider(ui_ctx,
                                &frame,
                                (KitRenderRect){panel.x + 12.0f, panel.y + 132.0f, panel.width - 24.0f, 24.0f},
                                *detail_value,
                                slider_result.state);
    if (result.code != CORE_OK) return 1;

    kit_graph_ts_style_default(&style);
    style.max_render_points = 120u + (uint32_t)(*detail_value * 1080.0f);
    style.show_hover_crosshair = *show_overlay;
    style.show_hover_label = *show_overlay;

    result = kit_graph_ts_draw_plot(&frame, plot_bounds, series, active_series_count, view, &style);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_graph_ts_draw_plot failed: %d\n", (int)result.code);
        return 1;
    }

    result = kit_graph_ts_hover_inspect(series + (*series_mode == 1 ? 1 : 0),
                                        view,
                                        plot_bounds,
                                        input->mouse_x,
                                        input->mouse_y,
                                        &hover);
    if (result.code != CORE_OK) return 1;

    hover.series_index = (uint32_t)(*series_mode == 1 ? 1 : 0);
    if (*show_overlay) {
        result = kit_graph_ts_draw_hover_overlay(&frame, plot_bounds, view, &style, &hover);
        if (result.code != CORE_OK) return 1;
    }

    {
        char info[128];
        char detail[128];
        snprintf(info,
                 sizeof(info),
                 "mode=%s  points=%u  stride=%u",
                 *series_mode == 0 ? "SIN" : (*series_mode == 1 ? "COS" : "BOTH"),
                 point_count,
                 kit_graph_ts_recommended_stride(point_count, plot_bounds.width - (style.padding * 2.0f), style.max_render_points));
        snprintf(detail,
                 sizeof(detail),
                 "hover idx=%u  x=%.2f  y=%.2f",
                 hover.nearest_index,
                 hover.nearest_x,
                 hover.nearest_y);
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){panel.x + 12.0f, panel.y + 180.0f, panel.width - 24.0f, 24.0f},
                                   info,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){panel.x + 12.0f, panel.y + 208.0f, panel.width - 24.0f, 24.0f},
                                   detail,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
    }

    result = kit_render_end_frame(render_ctx, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_end_frame failed: %d\n", (int)result.code);
        return 1;
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
    CoreResult result;
    enum { POINT_COUNT = 1400 };
    float xs[POINT_COUNT];
    float ys0[POINT_COUNT];
    float ys1[POINT_COUNT];
    KitGraphTsView view;
    KitUiInputState input = {0};
    int wheel_y = 0;
    int series_mode = 2;
    int show_overlay = 1;
    float detail_value = 0.35f;
    int exit_code = 1;

    fill_series(xs, ys0, ys1, POINT_COUNT);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("kit_graph_timeseries Vulkan Validation",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              960,
                              600,
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

    result = kit_graph_ts_compute_view(
        (KitGraphTsSeries[]){
            {xs, ys0, POINT_COUNT, "SIN", {82, 150, 255, 255}},
            {xs, ys1, POINT_COUNT, "COS", {90, 224, 180, 255}}
        },
        2,
        &view);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_graph_ts_compute_view failed: %d\n", (int)result.code);
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
                      xs,
                      ys0,
                      ys1,
                      POINT_COUNT,
                      &input,
                      wheel_y,
                      &view,
                      &series_mode,
                      &show_overlay,
                      &detail_value) != 0) {
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
