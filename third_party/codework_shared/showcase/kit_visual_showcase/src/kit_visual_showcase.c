/*
 * kit_visual_showcase.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "kit_graph_struct.h"
#include "kit_graph_timeseries.h"
#include "kit_ui.h"
#include "vk_renderer.h"

typedef enum ShowcaseView {
    SHOWCASE_VIEW_RENDER = 0,
    SHOWCASE_VIEW_UI = 1,
    SHOWCASE_VIEW_TIMESERIES = 2,
    SHOWCASE_VIEW_STRUCT = 3
} ShowcaseView;

typedef struct UiDemoState {
    int selected_panel;
    int overlay_enabled;
    float slider_value;
    int flash;
} UiDemoState;

typedef struct StructDemoState {
    KitGraphStructViewport viewport;
    uint32_t selected_node_id;
    int has_selected;
    int layout_mode;
} StructDemoState;

static void fill_series(float *xs, float *ys0, float *ys1, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; ++i) {
        float x = (float)i * 0.03f;
        xs[i] = x;
        ys0[i] = sinf(x) + (0.1f * sinf(x * 8.0f));
        ys1[i] = 0.7f * cosf(x * 0.8f);
    }
}

static int hovered_segment_index(KitRenderRect bounds,
                                 const KitUiInputState *input,
                                 int segment_count) {
    float seg_width;
    int index;

    if (!input || segment_count <= 0) {
        return -1;
    }
    if (!kit_ui_point_in_rect(bounds, input->mouse_x, input->mouse_y)) {
        return -1;
    }

    seg_width = bounds.width / (float)segment_count;
    if (seg_width <= 0.0f) {
        return -1;
    }

    index = (int)((input->mouse_x - bounds.x) / seg_width);
    if (index < 0) index = 0;
    if (index >= segment_count) index = segment_count - 1;
    return index;
}

static CoreResult draw_info_lines(KitUiContext *ui_ctx,
                                  KitRenderFrame *frame,
                                  float x,
                                  float y,
                                  float width,
                                  const char *const *lines,
                                  int line_count) {
    int i;
    for (i = 0; i < line_count; ++i) {
        CoreResult result = kit_ui_draw_label(ui_ctx,
                                              frame,
                                              (KitRenderRect){x, y + ((float)i * 24.0f), width, 20.0f},
                                              lines[i],
                                              i == 0 ? CORE_THEME_COLOR_TEXT_PRIMARY : CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) {
            return result;
        }
    }
    return core_result_ok();
}

static int draw_nav_button(KitUiContext *ui_ctx,
                           KitRenderFrame *frame,
                           KitRenderRect bounds,
                           const KitUiInputState *input,
                           const char *label,
                           int selected,
                           int *clicked) {
    KitUiButtonResult button_result;
    CoreResult result;

    *clicked = 0;
    button_result = kit_ui_eval_button(bounds, input, 1);
    if (button_result.clicked) {
        *clicked = 1;
    }
    if (selected) {
        button_result.state = KIT_UI_STATE_ACTIVE;
    }

    result = kit_ui_draw_button(ui_ctx, frame, bounds, label, button_result.state);
    return result.code == CORE_OK ? 0 : 1;
}

static int draw_render_view(KitRenderFrame *frame,
                            KitUiContext *ui_ctx,
                            KitRenderRect content,
                            int overlay_enabled) {
    CoreResult result;
    KitRenderRect info_panel;
    KitRenderRect canvas;
    KitRenderRect clip_rect;
    const char *info_lines[] = {
        "kit_render API",
        "begin_frame / end_frame",
        "push_clear",
        "push_set_clip / push_clear_clip",
        "push_rect / push_line / push_polyline",
        "push_text"
    };

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            content, 12.0f, {22, 27, 38, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    info_panel = (KitRenderRect){content.x + 22.0f, content.y + 22.0f, 300.0f, content.height - 44.0f};
    canvas = (KitRenderRect){info_panel.x + info_panel.width + 22.0f,
                             content.y + 22.0f,
                             content.width - (info_panel.width + 66.0f),
                             content.height - 44.0f};

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            info_panel, 10.0f, {32, 38, 52, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;
    result = draw_info_lines(ui_ctx, frame, info_panel.x + 10.0f, info_panel.y + 12.0f, info_panel.width - 20.0f, info_lines, 6);
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            canvas, 10.0f, {18, 22, 32, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    clip_rect = (KitRenderRect){canvas.x + 26.0f, canvas.y + 26.0f, canvas.width - 180.0f, canvas.height - 52.0f};
    result = kit_render_push_set_clip(frame, clip_rect);
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            {clip_rect.x + 12.0f, clip_rect.y + 18.0f, clip_rect.width * 0.32f, clip_rect.height * 0.22f},
            10.0f, {78, 118, 210, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;
    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            {clip_rect.x + 80.0f, clip_rect.y + 90.0f, clip_rect.width * 0.38f, clip_rect.height * 0.19f},
            10.0f, {214, 104, 62, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;
    result = kit_render_push_line(
        frame,
        &(KitRenderLineCommand){
            {clip_rect.x + 24.0f, clip_rect.y + clip_rect.height * 0.64f},
            {clip_rect.x + clip_rect.width * 0.55f, clip_rect.y + clip_rect.height * 0.90f},
            2.0f, {230, 236, 245, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;
    result = kit_render_push_polyline(
        frame,
        &(KitRenderPolylineCommand){
            (KitRenderVec2[]){
                {clip_rect.x + clip_rect.width * 0.58f, clip_rect.y + clip_rect.height * 0.26f},
                {clip_rect.x + clip_rect.width * 0.71f, clip_rect.y + clip_rect.height * 0.13f},
                {clip_rect.x + clip_rect.width * 0.84f, clip_rect.y + clip_rect.height * 0.34f},
                {clip_rect.x + clip_rect.width * 0.97f, clip_rect.y + clip_rect.height * 0.18f}
            },
            4u,
            2.0f,
            {100, 218, 184, 255},
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;
    result = kit_render_push_clear_clip(frame);
    if (result.code != CORE_OK) return 1;

    if (overlay_enabled) {
        result = kit_render_push_text(
            frame,
            &(KitRenderTextCommand){
                {canvas.x + 18.0f, canvas.y + canvas.height - 18.0f},
                "RENDER PRIMITIVE SHOWCASE",
                CORE_FONT_ROLE_UI_BOLD,
                CORE_FONT_TEXT_SIZE_PARAGRAPH,
                CORE_THEME_COLOR_TEXT_PRIMARY,
                {0.0f, 0.0f, 1.0f, 1.0f}
            });
        if (result.code != CORE_OK) return 1;
    }

    return 0;
}

static int draw_ui_view(KitRenderFrame *frame,
                        KitUiContext *ui_ctx,
                        KitRenderRect content,
                        const KitUiInputState *input,
                        UiDemoState *ui_demo) {
    CoreResult result;
    KitRenderRect info_panel;
    KitRenderRect demo_panel;
    KitRenderRect row;
    KitUiButtonResult button_result;
    KitUiCheckboxResult checkbox_result;
    KitUiSliderResult slider_result;
    KitUiSegmentedResult segmented_result;
    int hovered_segment;
    const char *panels[3] = {"Input", "Layout", "Theme"};
    const char *info_lines[] = {
        "kit_ui API",
        "draw_button / eval_button",
        "draw_checkbox / eval_checkbox",
        "draw_slider / eval_slider",
        "draw_segmented / eval_segmented",
        "draw_label"
    };
    int i;

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            content, 12.0f, {24, 30, 42, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    info_panel = (KitRenderRect){content.x + 22.0f, content.y + 22.0f, 320.0f, content.height - 44.0f};
    demo_panel = (KitRenderRect){info_panel.x + info_panel.width + 22.0f,
                                 content.y + 22.0f,
                                 content.width - (info_panel.width + 66.0f),
                                 content.height - 44.0f};

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            info_panel, 10.0f, {32, 38, 52, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;
    result = draw_info_lines(ui_ctx, frame, info_panel.x + 10.0f, info_panel.y + 12.0f, info_panel.width - 20.0f, info_lines, 6);
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            demo_panel, 10.0f, {31, 37, 49, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    row = (KitRenderRect){demo_panel.x + 22.0f, demo_panel.y + 26.0f, 360.0f, 42.0f};
    button_result = kit_ui_eval_button(row, input, 1);
    if (button_result.clicked) {
        ui_demo->flash = 18;
    }
    result = kit_ui_draw_button(ui_ctx, frame, row, "Shared Action", button_result.state);
    if (result.code != CORE_OK) return 1;

    row.y += 58.0f;
    checkbox_result = kit_ui_eval_checkbox(row, input, 1, ui_demo->overlay_enabled);
    if (checkbox_result.toggled) {
        ui_demo->overlay_enabled = checkbox_result.checked;
    }
    result = kit_ui_draw_checkbox(ui_ctx, frame, row, "Overlay Enabled", ui_demo->overlay_enabled, checkbox_result.state);
    if (result.code != CORE_OK) return 1;

    row.y += 58.0f;
    slider_result = kit_ui_eval_slider(row, input, 1, ui_demo->slider_value);
    if (slider_result.changed) {
        ui_demo->slider_value = slider_result.value_01;
    }
    result = kit_ui_draw_slider(ui_ctx, frame, row, ui_demo->slider_value, slider_result.state);
    if (result.code != CORE_OK) return 1;

    row.y += 56.0f;
    hovered_segment = hovered_segment_index((KitRenderRect){row.x, row.y, row.width, 36.0f}, input, 3);
    segmented_result = kit_ui_eval_segmented((KitRenderRect){row.x, row.y, row.width, 36.0f}, input, 1, 3, ui_demo->selected_panel);
    if (segmented_result.clicked_index >= 0) {
        ui_demo->selected_panel = segmented_result.selected_index;
    }
    result = kit_ui_draw_segmented(ui_ctx,
                                   frame,
                                   (KitRenderRect){row.x, row.y, row.width, 36.0f},
                                   panels,
                                   3,
                                   ui_demo->selected_panel,
                                   hovered_segment,
                                   1);
    if (result.code != CORE_OK) return 1;

    for (i = 0; i < 4; ++i) {
        float width_scale = 0.35f + ((float)(i + 1) * 0.14f) + (ui_demo->slider_value * 0.18f);
        if (width_scale > 0.92f) width_scale = 0.92f;
        result = kit_render_push_rect(
            frame,
            &(KitRenderRectCommand){
                {
                    demo_panel.x + 430.0f,
                    demo_panel.y + 42.0f + ((float)i * 82.0f),
                    (demo_panel.width - 460.0f) * width_scale,
                    46.0f
                },
                8.0f,
                i == ui_demo->selected_panel ? (KitRenderColor){88, 136, 232, 255} : (KitRenderColor){58, 66, 84, 255},
                {0.0f, 0.0f, 1.0f, 1.0f}
            });
        if (result.code != CORE_OK) return 1;
    }

    if (ui_demo->overlay_enabled) {
        result = kit_render_push_text(
            frame,
            &(KitRenderTextCommand){
                {demo_panel.x + 22.0f, demo_panel.y + demo_panel.height - 18.0f},
                "INTERACTIVE UI DEMO",
                CORE_FONT_ROLE_UI_BOLD,
                CORE_FONT_TEXT_SIZE_BASIC,
                CORE_THEME_COLOR_TEXT_PRIMARY,
                {0.0f, 0.0f, 1.0f, 1.0f}
            });
        if (result.code != CORE_OK) return 1;
    }

    if (ui_demo->flash > 0) {
        result = kit_render_push_rect(
            frame,
            &(KitRenderRectCommand){
                {demo_panel.x + demo_panel.width - 40.0f, demo_panel.y + 18.0f, 22.0f, 22.0f},
                6.0f,
                {(uint8_t)(130 + (ui_demo->flash * 4)), 138, 224, 255},
                {0.0f, 0.0f, 1.0f, 1.0f}
            });
        if (result.code != CORE_OK) return 1;
        ui_demo->flash -= 1;
    }

    return 0;
}

static int draw_timeseries_view(KitRenderFrame *frame,
                                KitUiContext *ui_ctx,
                                KitRenderRect content,
                                const float *xs,
                                const float *ys0,
                                const float *ys1,
                                uint32_t point_count,
                                const KitUiInputState *input,
                                int wheel_y,
                                KitGraphTsView *graph_view,
                                int overlay_enabled) {
    KitGraphTsSeries series[2];
    KitGraphTsStyle style;
    KitGraphTsHover hover;
    KitRenderRect info_panel;
    KitRenderRect plot_bounds;
    CoreResult result;
    const char *info_lines[] = {
        "kit_graph_timeseries API",
        "compute_view / zoom_view",
        "draw_plot",
        "hover_inspect",
        "draw_hover_overlay",
        "recommended_stride"
    };

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            content, 12.0f, {22, 28, 40, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    info_panel = (KitRenderRect){content.x + 22.0f, content.y + 22.0f, 320.0f, content.height - 44.0f};
    plot_bounds = (KitRenderRect){info_panel.x + info_panel.width + 22.0f,
                                  content.y + 22.0f,
                                  content.width - (info_panel.width + 66.0f),
                                  content.height - 44.0f};

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            info_panel, 10.0f, {32, 38, 52, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;
    result = draw_info_lines(ui_ctx, frame, info_panel.x + 10.0f, info_panel.y + 12.0f, info_panel.width - 20.0f, info_lines, 6);
    if (result.code != CORE_OK) return 1;

    if (wheel_y != 0 && kit_ui_point_in_rect(plot_bounds, input->mouse_x, input->mouse_y)) {
        result = kit_graph_ts_zoom_view(graph_view, wheel_y > 0 ? 0.85f : 1.15f);
        if (result.code != CORE_OK) return 1;
    }

    series[0] = (KitGraphTsSeries){xs, ys0, point_count, "SIN", {82, 150, 255, 255}};
    series[1] = (KitGraphTsSeries){xs, ys1, point_count, "COS", {90, 224, 180, 255}};

    kit_graph_ts_style_default(&style);
    style.max_render_points = 720u;
    style.show_hover_crosshair = overlay_enabled;
    style.show_hover_label = overlay_enabled;

    result = kit_graph_ts_draw_plot(frame, plot_bounds, series, 2u, graph_view, &style);
    if (result.code != CORE_OK) return 1;
    result = kit_graph_ts_hover_inspect(&series[0], graph_view, plot_bounds, input->mouse_x, input->mouse_y, &hover);
    if (result.code != CORE_OK) return 1;
    if (overlay_enabled) {
        result = kit_graph_ts_draw_hover_overlay(frame, plot_bounds, graph_view, &style, &hover);
        if (result.code != CORE_OK) return 1;
    }

    {
        char line0[128];
        char line1[128];
        snprintf(line0,
                 sizeof(line0),
                 "Stride %u  Hover X %.2f",
                 kit_graph_ts_recommended_stride(point_count,
                                                 plot_bounds.width - (style.padding * 2.0f),
                                                 style.max_render_points),
                 hover.nearest_x);
        snprintf(line1, sizeof(line1), "Hover Y %.2f", hover.nearest_y);
        result = kit_ui_draw_label(ui_ctx,
                                   frame,
                                   (KitRenderRect){info_panel.x + 10.0f, info_panel.y + 186.0f, info_panel.width - 20.0f, 22.0f},
                                   line0,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   frame,
                                   (KitRenderRect){info_panel.x + 10.0f, info_panel.y + 210.0f, info_panel.width - 20.0f, 22.0f},
                                   line1,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
    }

    return 0;
}

static const char *struct_node_label(uint32_t node_id) {
    switch (node_id) {
        case 1u: return "Root";
        case 2u: return "Alpha";
        case 3u: return "Beta";
        case 4u: return "Gamma";
        case 5u: return "Delta";
        case 6u: return "Epsilon";
        case 7u: return "Zeta";
        default: return "None";
    }
}

static int draw_struct_view(KitRenderFrame *frame,
                            KitUiContext *ui_ctx,
                            KitRenderRect content,
                            const KitUiInputState *input,
                            int wheel_y,
                            int drag_dx,
                            int drag_dy,
                            StructDemoState *struct_demo) {
    KitGraphStructNode nodes[7] = {
        {1u, "Root"}, {2u, "Alpha"}, {3u, "Beta"}, {4u, "Gamma"},
        {5u, "Delta"}, {6u, "Epsilon"}, {7u, "Zeta"}
    };
    KitGraphStructEdge tree_edges[6] = {
        {1u, 2u}, {1u, 3u}, {2u, 4u}, {2u, 5u}, {3u, 6u}, {3u, 7u}
    };
    KitGraphStructEdge dag_edges[8] = {
        {1u, 2u}, {1u, 3u}, {2u, 4u}, {2u, 5u},
        {3u, 5u}, {3u, 6u}, {4u, 7u}, {5u, 7u}
    };
    KitGraphStructNodeLayout layouts[7];
    KitGraphStructHit hit;
    KitRenderRect info_panel;
    KitRenderRect canvas;
    CoreResult result;
    KitUiSegmentedResult mode_result;
    KitUiButtonResult focus_result;
    int hovered_mode;
    const char *mode_labels[2] = {"Tree", "DAG"};
    const char *info_lines[] = {
        "kit_graph_struct API",
        "compute_layered_tree_layout",
        "compute_layered_dag_layout",
        "focus_on_node",
        "hit_test",
        "draw"
    };

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            content, 12.0f, {23, 28, 39, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    info_panel = (KitRenderRect){content.x + 22.0f, content.y + 22.0f, 320.0f, content.height - 44.0f};
    canvas = (KitRenderRect){info_panel.x + info_panel.width + 22.0f,
                             content.y + 22.0f,
                             content.width - (info_panel.width + 66.0f),
                             content.height - 44.0f};

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            info_panel, 10.0f, {32, 38, 52, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;
    result = draw_info_lines(ui_ctx, frame, info_panel.x + 10.0f, info_panel.y + 12.0f, info_panel.width - 20.0f, info_lines, 6);
    if (result.code != CORE_OK) return 1;

    hovered_mode = hovered_segment_index((KitRenderRect){info_panel.x + 10.0f, info_panel.y + 170.0f, info_panel.width - 20.0f, 34.0f},
                                         input,
                                         2);
    mode_result = kit_ui_eval_segmented((KitRenderRect){info_panel.x + 10.0f, info_panel.y + 170.0f, info_panel.width - 20.0f, 34.0f},
                                        input,
                                        1,
                                        2,
                                        struct_demo->layout_mode);
    if (mode_result.clicked_index >= 0) {
        struct_demo->layout_mode = mode_result.selected_index;
    }
    result = kit_ui_draw_segmented(ui_ctx,
                                   frame,
                                   (KitRenderRect){info_panel.x + 10.0f, info_panel.y + 170.0f, info_panel.width - 20.0f, 34.0f},
                                   mode_labels,
                                   2,
                                   struct_demo->layout_mode,
                                   hovered_mode,
                                   1);
    if (result.code != CORE_OK) return 1;

    if (wheel_y != 0 && kit_ui_point_in_rect(canvas, input->mouse_x, input->mouse_y)) {
        struct_demo->viewport.zoom *= wheel_y > 0 ? 1.08f : 0.92f;
        if (struct_demo->viewport.zoom < 0.45f) struct_demo->viewport.zoom = 0.45f;
        if (struct_demo->viewport.zoom > 2.2f) struct_demo->viewport.zoom = 2.2f;
    }
    if (drag_dx != 0 || drag_dy != 0) {
        struct_demo->viewport.pan_x += (float)drag_dx;
        struct_demo->viewport.pan_y += (float)drag_dy;
    }

    if (struct_demo->layout_mode == KIT_GRAPH_STRUCT_LAYOUT_LAYERED_DAG) {
        result = kit_graph_struct_compute_layered_dag_layout(nodes,
                                                             7u,
                                                             dag_edges,
                                                             8u,
                                                             canvas,
                                                             &struct_demo->viewport,
                                                             0,
                                                             layouts);
    } else {
        result = kit_graph_struct_compute_layered_tree_layout(nodes,
                                                              7u,
                                                              tree_edges,
                                                              6u,
                                                              canvas,
                                                              &struct_demo->viewport,
                                                              0,
                                                              layouts);
    }
    if (result.code != CORE_OK) return 1;
    result = kit_graph_struct_hit_test(layouts, 7u, input->mouse_x, input->mouse_y, &hit);
    if (result.code != CORE_OK) return 1;
    if (input->mouse_released && hit.active) {
        struct_demo->selected_node_id = hit.node_id;
        struct_demo->has_selected = 1;
    }

    focus_result = kit_ui_eval_button((KitRenderRect){info_panel.x + 10.0f, info_panel.y + 216.0f, info_panel.width - 20.0f, 32.0f},
                                      input,
                                      struct_demo->has_selected);
    if (focus_result.clicked && struct_demo->has_selected) {
        result = kit_graph_struct_focus_on_node(layouts,
                                                7u,
                                                struct_demo->selected_node_id,
                                                canvas,
                                                26.0f,
                                                &struct_demo->viewport);
        if (result.code != CORE_OK) return 1;
        if (struct_demo->layout_mode == KIT_GRAPH_STRUCT_LAYOUT_LAYERED_DAG) {
            result = kit_graph_struct_compute_layered_dag_layout(nodes,
                                                                 7u,
                                                                 dag_edges,
                                                                 8u,
                                                                 canvas,
                                                                 &struct_demo->viewport,
                                                                 0,
                                                                 layouts);
        } else {
            result = kit_graph_struct_compute_layered_tree_layout(nodes,
                                                                  7u,
                                                                  tree_edges,
                                                                  6u,
                                                                  canvas,
                                                                  &struct_demo->viewport,
                                                                  0,
                                                                  layouts);
        }
        if (result.code != CORE_OK) return 1;
    }
    result = kit_ui_draw_button(ui_ctx,
                                frame,
                                (KitRenderRect){info_panel.x + 10.0f, info_panel.y + 216.0f, info_panel.width - 20.0f, 32.0f},
                                "Focus Selected",
                                focus_result.state);
    if (result.code != CORE_OK) return 1;

    result = kit_graph_struct_draw(frame,
                                   canvas,
                                   nodes,
                                   7u,
                                   struct_demo->layout_mode == KIT_GRAPH_STRUCT_LAYOUT_LAYERED_DAG ? dag_edges : tree_edges,
                                   struct_demo->layout_mode == KIT_GRAPH_STRUCT_LAYOUT_LAYERED_DAG ? 8u : 6u,
                                   layouts,
                                   struct_demo->selected_node_id,
                                   struct_demo->has_selected,
                                   hit.node_id,
                                   hit.active);
    if (result.code != CORE_OK) return 1;

    {
        char line0[96];
        char line1[96];
        char line2[96];
        snprintf(line0,
                 sizeof(line0),
                 "Mode %s",
                 struct_demo->layout_mode == KIT_GRAPH_STRUCT_LAYOUT_LAYERED_DAG ? "DAG" : "Tree");
        snprintf(line1, sizeof(line1), "Zoom %.2fx", struct_demo->viewport.zoom);
        snprintf(line2,
                 sizeof(line2),
                 "Selected %s",
                 struct_demo->has_selected ? struct_node_label(struct_demo->selected_node_id) : "None");
        result = kit_ui_draw_label(ui_ctx,
                                   frame,
                                   (KitRenderRect){info_panel.x + 10.0f, info_panel.y + 268.0f, info_panel.width - 20.0f, 22.0f},
                                   line0,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   frame,
                                   (KitRenderRect){info_panel.x + 10.0f, info_panel.y + 292.0f, info_panel.width - 20.0f, 22.0f},
                                   line1,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   frame,
                                   (KitRenderRect){info_panel.x + 10.0f, info_panel.y + 316.0f, info_panel.width - 20.0f, 22.0f},
                                   line2,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
    }

    return 0;
}

static int run_frame(KitRenderContext *render_ctx,
                     KitUiContext *ui_ctx,
                     int frame_width,
                     int frame_height,
                     const KitUiInputState *input,
                     int wheel_y,
                     int drag_dx,
                     int drag_dy,
                     int *view_index,
                     int *overlay_enabled,
                     UiDemoState *ui_demo,
                     StructDemoState *struct_demo,
                     const float *xs,
                     const float *ys0,
                     const float *ys1,
                     uint32_t point_count,
                     KitGraphTsView *graph_view) {
    KitRenderCommand commands[4096];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    KitRenderRect sidebar;
    KitRenderRect content;
    KitRenderRect row;
    CoreResult result;
    int clicked;
    int i;
    const char *view_labels[4] = {"Render", "UI", "Time Series", "Struct Graph"};

    if (frame_width < 1100) frame_width = 1100;
    if (frame_height < 760) frame_height = 760;

    command_buffer.commands = commands;
    command_buffer.capacity = 4096;
    command_buffer.count = 0;

    result = kit_render_begin_frame(render_ctx, (uint32_t)frame_width, (uint32_t)frame_height, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_begin_frame failed: %d\n", (int)result.code);
        return 1;
    }

    sidebar = (KitRenderRect){24.0f, 24.0f, 280.0f, (float)frame_height - 48.0f};
    content = (KitRenderRect){sidebar.x + sidebar.width + 24.0f,
                              24.0f,
                              (float)frame_width - (sidebar.x + sidebar.width + 48.0f),
                              (float)frame_height - 48.0f};

    result = kit_render_push_clear(&frame, (KitRenderColor){14, 18, 24, 255});
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_rect(
        &frame,
        &(KitRenderRectCommand){
            sidebar, 12.0f, {34, 40, 52, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_label(ui_ctx,
                               &frame,
                               (KitRenderRect){sidebar.x + 14.0f, sidebar.y + 14.0f, sidebar.width - 28.0f, 24.0f},
                               "KIT VISUAL SHOWCASE",
                               CORE_THEME_COLOR_TEXT_PRIMARY);
    if (result.code != CORE_OK) return 1;

    row = (KitRenderRect){sidebar.x + 14.0f, sidebar.y + 58.0f, sidebar.width - 28.0f, 40.0f};
    for (i = 0; i < 4; ++i) {
        if (draw_nav_button(ui_ctx, &frame, row, input, view_labels[i], *view_index == i, &clicked) != 0) return 1;
        if (clicked) {
            *view_index = i;
        }
        row.y += 50.0f;
    }

    {
        KitUiCheckboxResult overlay_result = kit_ui_eval_checkbox((KitRenderRect){sidebar.x + 14.0f, sidebar.y + 286.0f, sidebar.width - 28.0f, 28.0f},
                                                                  input,
                                                                  1,
                                                                  *overlay_enabled);
        if (overlay_result.toggled) {
            *overlay_enabled = overlay_result.checked;
        }
        result = kit_ui_draw_checkbox(ui_ctx,
                                      &frame,
                                      (KitRenderRect){sidebar.x + 14.0f, sidebar.y + 286.0f, sidebar.width - 28.0f, 28.0f},
                                      "Overlay Labels",
                                      *overlay_enabled,
                                      overlay_result.state);
        if (result.code != CORE_OK) return 1;
    }

    {
        const char *sidebar_lines[] = {
            "Left rail switches kits.",
            "Active view shows",
            "API-driven output.",
            "Use controls inside",
            "the active view."
        };
        result = draw_info_lines(ui_ctx,
                                 &frame,
                                 sidebar.x + 14.0f,
                                 sidebar.y + 338.0f,
                                 sidebar.width - 28.0f,
                                 sidebar_lines,
                                 5);
        if (result.code != CORE_OK) return 1;
    }

    switch ((ShowcaseView)(*view_index)) {
        case SHOWCASE_VIEW_RENDER:
            if (draw_render_view(&frame, ui_ctx, content, *overlay_enabled) != 0) return 1;
            break;
        case SHOWCASE_VIEW_UI:
            if (draw_ui_view(&frame, ui_ctx, content, input, ui_demo) != 0) return 1;
            break;
        case SHOWCASE_VIEW_TIMESERIES:
            if (draw_timeseries_view(&frame,
                                     ui_ctx,
                                     content,
                                     xs,
                                     ys0,
                                     ys1,
                                     point_count,
                                     input,
                                     wheel_y,
                                     graph_view,
                                     *overlay_enabled) != 0) return 1;
            break;
        case SHOWCASE_VIEW_STRUCT:
            if (draw_struct_view(&frame,
                                 ui_ctx,
                                 content,
                                 input,
                                 wheel_y,
                                 drag_dx,
                                 drag_dy,
                                 struct_demo) != 0) return 1;
            break;
        default:
            break;
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
    KitUiInputState input = {0};
    UiDemoState ui_demo = {0, 1, 0.35f, 0};
    StructDemoState struct_demo;
    CoreResult result;
    enum { POINT_COUNT = 1600 };
    float xs[POINT_COUNT];
    float ys0[POINT_COUNT];
    float ys1[POINT_COUNT];
    KitGraphTsView graph_view;
    int view_index = SHOWCASE_VIEW_RENDER;
    int overlay_enabled = 1;
    int wheel_y = 0;
    int drag_dx = 0;
    int drag_dy = 0;
    int last_x = 0;
    int last_y = 0;
    int frame_width = 0;
    int frame_height = 0;
    int exit_code = 1;

    fill_series(xs, ys0, ys1, POINT_COUNT);
    kit_graph_struct_viewport_default(&struct_demo.viewport);
    struct_demo.selected_node_id = 0u;
    struct_demo.has_selected = 0;
    struct_demo.layout_mode = KIT_GRAPH_STRUCT_LAYOUT_LAYERED_TREE;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("kit_visual_showcase",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              1440,
                              900,
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
        2u,
        &graph_view);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_graph_ts_compute_view failed: %d\n", (int)result.code);
        goto cleanup_render_ctx;
    }

    while (running) {
        input.mouse_pressed = 0;
        input.mouse_released = 0;
        wheel_y = 0;
        drag_dx = 0;
        drag_dy = 0;
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
                    if (input.mouse_down) {
                        drag_dx += event.motion.x - last_x;
                        drag_dy += event.motion.y - last_y;
                    }
                    last_x = event.motion.x;
                    last_y = event.motion.y;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        input.mouse_x = (float)event.button.x;
                        input.mouse_y = (float)event.button.y;
                        input.mouse_down = 1;
                        input.mouse_pressed = 1;
                        last_x = event.button.x;
                        last_y = event.button.y;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        input.mouse_x = (float)event.button.x;
                        input.mouse_y = (float)event.button.y;
                        input.mouse_down = 0;
                        input.mouse_released = 1;
                        last_x = event.button.x;
                        last_y = event.button.y;
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    wheel_y = event.wheel.y;
                    break;
                default:
                    break;
            }
        }

        SDL_GetWindowSize(window, &frame_width, &frame_height);
        if (run_frame(&render_ctx,
                      &ui_ctx,
                      frame_width,
                      frame_height,
                      &input,
                      wheel_y,
                      drag_dx,
                      drag_dy,
                      &view_index,
                      &overlay_enabled,
                      &ui_demo,
                      &struct_demo,
                      xs,
                      ys0,
                      ys1,
                      POINT_COUNT,
                      &graph_view) != 0) {
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
