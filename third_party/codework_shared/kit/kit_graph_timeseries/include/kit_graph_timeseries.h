#ifndef KIT_GRAPH_TIMESERIES_H
#define KIT_GRAPH_TIMESERIES_H

#include "kit_render.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KitGraphTsView {
    float x_min;
    float x_max;
    float y_min;
    float y_max;
} KitGraphTsView;

typedef struct KitGraphTsSeries {
    const float *xs;
    const float *ys;
    uint32_t point_count;
    const char *label;
    KitRenderColor color;
} KitGraphTsSeries;

typedef struct KitGraphTsStyle {
    int show_axes;
    int show_grid;
    int show_legend;
    int show_hover_crosshair;
    int show_hover_label;
    uint32_t grid_x_divisions;
    uint32_t grid_y_divisions;
    uint32_t max_render_points;
    float line_thickness;
    float padding;
    float hover_radius;
} KitGraphTsStyle;

typedef struct KitGraphTsHover {
    int active;
    uint32_t series_index;
    uint32_t nearest_index;
    float nearest_x;
    float nearest_y;
    float plot_x;
    float plot_y;
} KitGraphTsHover;

void kit_graph_ts_style_default(KitGraphTsStyle *out_style);

uint32_t kit_graph_ts_recommended_stride(uint32_t point_count,
                                         float plot_width,
                                         uint32_t max_render_points);

CoreResult kit_graph_ts_compute_view(const KitGraphTsSeries *series,
                                     uint32_t series_count,
                                     KitGraphTsView *out_view);

CoreResult kit_graph_ts_zoom_view(KitGraphTsView *view, float factor);

CoreResult kit_graph_ts_hover_inspect(const KitGraphTsSeries *series,
                                      const KitGraphTsView *view,
                                      KitRenderRect bounds,
                                      float mouse_x,
                                      float mouse_y,
                                      KitGraphTsHover *out_hover);

CoreResult kit_graph_ts_draw_hover_overlay(KitRenderFrame *frame,
                                           KitRenderRect bounds,
                                           const KitGraphTsView *view,
                                           const KitGraphTsStyle *style,
                                           const KitGraphTsHover *hover);

CoreResult kit_graph_ts_draw_plot(KitRenderFrame *frame,
                                  KitRenderRect bounds,
                                  const KitGraphTsSeries *series,
                                  uint32_t series_count,
                                  const KitGraphTsView *view,
                                  const KitGraphTsStyle *style);

#ifdef __cplusplus
}
#endif

#endif
