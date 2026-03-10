#include "kit_graph_timeseries.h"

#include <math.h>
#include <stdio.h>

static int nearf(float a, float b) {
    return fabsf(a - b) < 1e-5f;
}

static int test_compute_view(void) {
    const float xs0[3] = {0.0f, 1.0f, 2.0f};
    const float ys0[3] = {1.0f, -1.0f, 3.0f};
    const float xs1[2] = {-1.0f, 4.0f};
    const float ys1[2] = {2.0f, 0.0f};
    KitGraphTsSeries series[2];
    KitGraphTsView view;
    CoreResult result;

    series[0].xs = xs0;
    series[0].ys = ys0;
    series[0].point_count = 3;
    series[0].label = "A";
    series[0].color = (KitRenderColor){255, 0, 0, 255};
    series[1].xs = xs1;
    series[1].ys = ys1;
    series[1].point_count = 2;
    series[1].label = "B";
    series[1].color = (KitRenderColor){0, 255, 0, 255};

    result = kit_graph_ts_compute_view(series, 2, &view);
    if (result.code != CORE_OK) return 1;
    if (!nearf(view.x_min, -1.0f) || !nearf(view.x_max, 4.0f)) return 1;
    if (!nearf(view.y_min, -1.0f) || !nearf(view.y_max, 3.0f)) return 1;
    return 0;
}

static int test_stride_zoom_and_hover(void) {
    const float xs[4] = {0.0f, 1.0f, 2.0f, 3.0f};
    const float ys[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    KitGraphTsSeries series;
    KitGraphTsView view = {0.0f, 3.0f, 0.0f, 1.0f};
    KitGraphTsHover hover;
    CoreResult result;

    if (kit_graph_ts_recommended_stride(1200u, 300.0f, 0u) <= 1u) return 1;
    if (kit_graph_ts_recommended_stride(100u, 300.0f, 0u) != 1u) return 1;

    result = kit_graph_ts_zoom_view(&view, 0.5f);
    if (result.code != CORE_OK) return 1;
    if (!nearf(view.x_min, 0.75f) || !nearf(view.x_max, 2.25f)) return 1;

    series.xs = xs;
    series.ys = ys;
    series.point_count = 4;
    series.label = "Wave";
    series.color = (KitRenderColor){0, 0, 255, 255};
    result = kit_graph_ts_hover_inspect(&series,
                                        &(KitGraphTsView){0.0f, 3.0f, 0.0f, 1.0f},
                                        (KitRenderRect){0.0f, 0.0f, 300.0f, 200.0f},
                                        100.0f,
                                        36.0f,
                                        &hover);
    if (result.code != CORE_OK) return 1;
    if (!hover.active) return 1;
    if (hover.nearest_index != 1u) return 1;
    if (!(hover.plot_x > 0.0f)) return 1;
    if (!(hover.plot_y > 0.0f)) return 1;
    return 0;
}

static int test_draw_plot_commands(void) {
    float xs[1024];
    float ys[1024];
    KitGraphTsSeries series;
    KitGraphTsView view = {0.0f, 1023.0f, -1.0f, 1.0f};
    KitGraphTsStyle style;
    KitGraphTsHover hover = {1, 0, 64, 64.0f, 0.5f, 80.0f, 64.0f};
    KitRenderContext render_ctx;
    KitRenderCommand commands[512];
    KitRenderCommandBuffer buffer;
    KitRenderFrame frame;
    CoreResult result;
    int i;
    size_t line_count = 0;

    for (i = 0; i < 1024; ++i) {
        xs[i] = (float)i;
        ys[i] = (float)((i % 32) - 16) / 16.0f;
    }

    series.xs = xs;
    series.ys = ys;
    series.point_count = 1024;
    series.label = "Series";
    series.color = (KitRenderColor){255, 128, 0, 255};

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;

    buffer.commands = commands;
    buffer.capacity = 512;
    buffer.count = 0;
    result = kit_render_begin_frame(&render_ctx, 640, 480, &buffer, &frame);
    if (result.code != CORE_OK) return 1;

    kit_graph_ts_style_default(&style);
    style.show_grid = 0;
    style.max_render_points = 120;
    result = kit_graph_ts_draw_plot(&frame,
                                    (KitRenderRect){20.0f, 20.0f, 300.0f, 180.0f},
                                    &series,
                                    1,
                                    &view,
                                    &style);
    if (result.code != CORE_OK) return 1;

    result = kit_graph_ts_draw_hover_overlay(&frame,
                                             (KitRenderRect){20.0f, 20.0f, 300.0f, 180.0f},
                                             &view,
                                             &style,
                                             &hover);
    if (result.code != CORE_OK) return 1;

    for (i = 0; i < (int)buffer.count; ++i) {
        if (buffer.commands[i].kind == KIT_RENDER_CMD_LINE) {
            line_count += 1;
        }
    }

    if (buffer.count < 10) {
        fprintf(stderr, "expected at least 10 commands, got %zu\n", buffer.count);
        return 1;
    }
    if (line_count >= 300) {
        fprintf(stderr, "expected decimated draw path, got too many lines: %zu\n", line_count);
        return 1;
    }

    result = kit_render_end_frame(&render_ctx, &frame);
    if (result.code != CORE_OK) return 1;
    return 0;
}

int main(void) {
    if (test_compute_view() != 0) return 1;
    if (test_stride_zoom_and_hover() != 0) return 1;
    if (test_draw_plot_commands() != 0) return 1;

    puts("kit_graph_timeseries tests passed");
    return 0;
}
