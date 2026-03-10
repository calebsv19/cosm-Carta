/*
 * kit_graph_timeseries_draw.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "kit_graph_timeseries.h"

#include <stdio.h>

static CoreResult kit_graph_ts_invalid(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static float ts_clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static int ts_has_valid_view(const KitGraphTsView *view) {
    if (!view) return 0;
    if (!(view->x_max > view->x_min)) return 0;
    if (!(view->y_max > view->y_min)) return 0;
    return 1;
}

static void ts_plot_inner_rect(KitRenderRect bounds,
                               const KitGraphTsStyle *style,
                               KitRenderRect *out_inner) {
    float pad = style ? style->padding : 16.0f;
    out_inner->x = bounds.x + pad;
    out_inner->y = bounds.y + pad;
    out_inner->width = bounds.width - (pad * 2.0f);
    out_inner->height = bounds.height - (pad * 2.0f);
}

static void ts_map_point(KitRenderRect inner,
                         const KitGraphTsView *view,
                         float x,
                         float y,
                         float *out_px,
                         float *out_py) {
    float tx = (x - view->x_min) / (view->x_max - view->x_min);
    float ty = (y - view->y_min) / (view->y_max - view->y_min);
    tx = ts_clamp01(tx);
    ty = ts_clamp01(ty);
    *out_px = inner.x + (tx * inner.width);
    *out_py = inner.y + inner.height - (ty * inner.height);
}

CoreResult kit_graph_ts_draw_hover_overlay(KitRenderFrame *frame,
                                           KitRenderRect bounds,
                                           const KitGraphTsView *view,
                                           const KitGraphTsStyle *style,
                                           const KitGraphTsHover *hover) {
    KitGraphTsStyle local_style;
    KitRenderRect inner;
    CoreResult result;

    if (!frame || !hover || !ts_has_valid_view(view)) {
        return kit_graph_ts_invalid("invalid hover overlay");
    }
    if (!hover->active) {
        return core_result_ok();
    }
    if (!style) {
        kit_graph_ts_style_default(&local_style);
        style = &local_style;
    }

    ts_plot_inner_rect(bounds, style, &inner);
    if (inner.width <= 0.0f || inner.height <= 0.0f) {
        return kit_graph_ts_invalid("invalid hover bounds");
    }

    if (style->show_hover_crosshair) {
        result = kit_render_push_set_clip(frame, inner);
        if (result.code != CORE_OK) return result;
        result = kit_render_push_line(frame,
                                      &(KitRenderLineCommand){
                                          {hover->plot_x, inner.y},
                                          {hover->plot_x, inner.y + inner.height},
                                          1.0f,
                                          {82, 92, 116, 255},
                                          {0.0f, 0.0f, 1.0f, 1.0f}
                                      });
        if (result.code != CORE_OK) return result;
        result = kit_render_push_line(frame,
                                      &(KitRenderLineCommand){
                                          {inner.x, hover->plot_y},
                                          {inner.x + inner.width, hover->plot_y},
                                          1.0f,
                                          {82, 92, 116, 255},
                                          {0.0f, 0.0f, 1.0f, 1.0f}
                                      });
        if (result.code != CORE_OK) return result;
        result = kit_render_push_clear_clip(frame);
        if (result.code != CORE_OK) return result;
    }

    result = kit_render_push_rect(frame,
                                  &(KitRenderRectCommand){
                                      {
                                          hover->plot_x - style->hover_radius,
                                          hover->plot_y - style->hover_radius,
                                          style->hover_radius * 2.0f,
                                          style->hover_radius * 2.0f
                                      },
                                      style->hover_radius,
                                      {245, 248, 255, 255},
                                      {0.0f, 0.0f, 1.0f, 1.0f}
                                  });
    if (result.code != CORE_OK) return result;

    if (style->show_hover_label) {
        char label[96];
        snprintf(label, sizeof(label), "x %.2f  y %.2f", hover->nearest_x, hover->nearest_y);
        result = kit_render_push_text(frame,
                                      &(KitRenderTextCommand){
                                          {bounds.x + style->padding, bounds.y + bounds.height - 12.0f},
                                          label,
                                          CORE_FONT_ROLE_UI_MEDIUM,
                                          CORE_FONT_TEXT_SIZE_CAPTION,
                                          CORE_THEME_COLOR_TEXT_MUTED,
                                          {0.0f, 0.0f, 1.0f, 1.0f}
                                      });
        if (result.code != CORE_OK) return result;
    }

    return core_result_ok();
}

CoreResult kit_graph_ts_draw_plot(KitRenderFrame *frame,
                                  KitRenderRect bounds,
                                  const KitGraphTsSeries *series,
                                  uint32_t series_count,
                                  const KitGraphTsView *view,
                                  const KitGraphTsStyle *style) {
    KitGraphTsStyle local_style;
    KitRenderRect inner;
    CoreResult result;
    uint32_t i;

    if (!frame || !series || series_count == 0 || !ts_has_valid_view(view)) {
        return kit_graph_ts_invalid("invalid plot draw");
    }
    if (!style) {
        kit_graph_ts_style_default(&local_style);
        style = &local_style;
    }

    ts_plot_inner_rect(bounds, style, &inner);
    if (inner.width <= 0.0f || inner.height <= 0.0f) {
        return kit_graph_ts_invalid("invalid plot bounds");
    }

    result = kit_render_push_rect(frame,
                                  &(KitRenderRectCommand){
                                      bounds,
                                      8.0f,
                                      {26, 32, 42, 255},
                                      {0.0f, 0.0f, 1.0f, 1.0f}
                                  });
    if (result.code != CORE_OK) return result;

    result = kit_render_push_set_clip(frame, inner);
    if (result.code != CORE_OK) return result;

    if (style->show_grid) {
        uint32_t gx = style->grid_x_divisions == 0 ? 1 : style->grid_x_divisions;
        uint32_t gy = style->grid_y_divisions == 0 ? 1 : style->grid_y_divisions;
        for (i = 1; i < gx; ++i) {
            float x = inner.x + (inner.width * ((float)i / (float)gx));
            result = kit_render_push_line(frame,
                                          &(KitRenderLineCommand){
                                              {x, inner.y},
                                              {x, inner.y + inner.height},
                                              1.0f,
                                              {58, 66, 82, 255},
                                              {0.0f, 0.0f, 1.0f, 1.0f}
                                          });
            if (result.code != CORE_OK) return result;
        }
        for (i = 1; i < gy; ++i) {
            float y = inner.y + (inner.height * ((float)i / (float)gy));
            result = kit_render_push_line(frame,
                                          &(KitRenderLineCommand){
                                              {inner.x, y},
                                              {inner.x + inner.width, y},
                                              1.0f,
                                              {58, 66, 82, 255},
                                              {0.0f, 0.0f, 1.0f, 1.0f}
                                          });
            if (result.code != CORE_OK) return result;
        }
    }

    if (style->show_axes) {
        float axis_y = inner.y + inner.height;
        if (view->y_min < 0.0f && view->y_max > 0.0f) {
            axis_y = inner.y + inner.height - (((0.0f - view->y_min) / (view->y_max - view->y_min)) * inner.height);
        }
        result = kit_render_push_line(frame,
                                      &(KitRenderLineCommand){
                                          {inner.x, axis_y},
                                          {inner.x + inner.width, axis_y},
                                          1.5f,
                                          {120, 132, 160, 255},
                                          {0.0f, 0.0f, 1.0f, 1.0f}
                                      });
        if (result.code != CORE_OK) return result;
        result = kit_render_push_line(frame,
                                      &(KitRenderLineCommand){
                                          {inner.x, inner.y},
                                          {inner.x, inner.y + inner.height},
                                          1.5f,
                                          {120, 132, 160, 255},
                                          {0.0f, 0.0f, 1.0f, 1.0f}
                                      });
        if (result.code != CORE_OK) return result;
    }

    for (i = 0; i < series_count; ++i) {
        const KitGraphTsSeries *s = &series[i];
        uint32_t j;
        uint32_t stride;
        if (!s->xs || !s->ys || s->point_count < 2) {
            continue;
        }
        stride = kit_graph_ts_recommended_stride(s->point_count, inner.width, style->max_render_points);
        for (j = 0; j + stride < s->point_count; j += stride) {
            float x0, y0, x1, y1;
            ts_map_point(inner, view, s->xs[j], s->ys[j], &x0, &y0);
            ts_map_point(inner, view, s->xs[j + stride], s->ys[j + stride], &x1, &y1);
            result = kit_render_push_line(frame,
                                          &(KitRenderLineCommand){
                                              {x0, y0}, {x1, y1}, style->line_thickness, s->color,
                                              {0.0f, 0.0f, 1.0f, 1.0f}
                                          });
            if (result.code != CORE_OK) return result;
        }
        if (((s->point_count - 1u) % stride) != 0u) {
            float x0, y0, x1, y1;
            uint32_t last = s->point_count - 1u;
            uint32_t prev = last > stride ? last - stride : 0u;
            ts_map_point(inner, view, s->xs[prev], s->ys[prev], &x0, &y0);
            ts_map_point(inner, view, s->xs[last], s->ys[last], &x1, &y1);
            result = kit_render_push_line(frame,
                                          &(KitRenderLineCommand){
                                              {x0, y0}, {x1, y1}, style->line_thickness, s->color,
                                              {0.0f, 0.0f, 1.0f, 1.0f}
                                          });
            if (result.code != CORE_OK) return result;
        }
    }

    result = kit_render_push_clear_clip(frame);
    if (result.code != CORE_OK) return result;

    result = kit_render_push_rect(frame,
                                  &(KitRenderRectCommand){
                                      inner, 0.0f, {0, 0, 0, 0}, {0.0f, 0.0f, 1.0f, 1.0f}
                                  });
    (void)result;

    if (style->show_legend) {
        float legend_x = bounds.x + style->padding;
        float legend_y = bounds.y + 6.0f;
        for (i = 0; i < series_count; ++i) {
            if (!series[i].label) {
                continue;
            }
            result = kit_render_push_rect(frame,
                                          &(KitRenderRectCommand){
                                              {legend_x, legend_y + 4.0f, 10.0f, 10.0f},
                                              2.0f,
                                              series[i].color,
                                              {0.0f, 0.0f, 1.0f, 1.0f}
                                          });
            if (result.code != CORE_OK) return result;
            result = kit_render_push_text(frame,
                                          &(KitRenderTextCommand){
                                              {legend_x + 16.0f, legend_y + 10.0f},
                                              series[i].label,
                                              CORE_FONT_ROLE_UI_REGULAR,
                                              CORE_FONT_TEXT_SIZE_CAPTION,
                                              CORE_THEME_COLOR_TEXT_PRIMARY,
                                              {0.0f, 0.0f, 1.0f, 1.0f}
                                          });
            if (result.code != CORE_OK) return result;
            legend_x += 120.0f;
        }
    }

    return core_result_ok();
}
