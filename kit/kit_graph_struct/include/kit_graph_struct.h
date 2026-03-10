#ifndef KIT_GRAPH_STRUCT_H
#define KIT_GRAPH_STRUCT_H

#include "kit_render.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KitGraphStructNode {
    uint32_t id;
    const char *label;
} KitGraphStructNode;

typedef struct KitGraphStructEdge {
    uint32_t from_id;
    uint32_t to_id;
} KitGraphStructEdge;

typedef CoreResult (*KitGraphStructMeasureTextFn)(void *user,
                                                  CoreFontRoleId font_role,
                                                  CoreFontTextSizeTier text_tier,
                                                  const char *text,
                                                  KitRenderTextMetrics *out_metrics);

typedef enum KitGraphStructRouteMode {
    KIT_GRAPH_STRUCT_ROUTE_STRAIGHT = 0,
    KIT_GRAPH_STRUCT_ROUTE_ORTHOGONAL = 1
} KitGraphStructRouteMode;

enum {
    KIT_GRAPH_STRUCT_EDGE_ROUTE_MAX_POINTS = 6
};

typedef struct KitGraphStructEdgeRoute {
    uint32_t edge_index;
    uint32_t point_count;
    KitRenderVec2 points[KIT_GRAPH_STRUCT_EDGE_ROUTE_MAX_POINTS];
} KitGraphStructEdgeRoute;

typedef enum KitGraphStructLayoutMode {
    KIT_GRAPH_STRUCT_LAYOUT_LAYERED_TREE = 0,
    KIT_GRAPH_STRUCT_LAYOUT_LAYERED_DAG = 1
} KitGraphStructLayoutMode;

typedef struct KitGraphStructLayoutStyle {
    float padding;
    float level_gap;
    float sibling_gap;
    float node_width;
    float node_height;
    float node_min_width;
    float node_max_width;
    float node_padding_x;
    float label_char_width;
    float edge_label_padding_x;
    float edge_label_height;
    float edge_label_lane_gap;
    CoreFontRoleId node_label_font_role;
    CoreFontTextSizeTier node_label_text_tier;
    KitGraphStructMeasureTextFn measure_text_fn;
    void *measure_text_user;
} KitGraphStructLayoutStyle;

typedef struct KitGraphStructViewport {
    float pan_x;
    float pan_y;
    float zoom;
} KitGraphStructViewport;

typedef struct KitGraphStructNodeLayout {
    uint32_t node_id;
    KitRenderRect rect;
    uint32_t depth;
} KitGraphStructNodeLayout;

typedef struct KitGraphStructHit {
    int active;
    uint32_t node_index;
    uint32_t node_id;
} KitGraphStructHit;

typedef struct KitGraphStructEdgeLabel {
    const char *text;
} KitGraphStructEdgeLabel;

typedef struct KitGraphStructEdgeLabelLayout {
    uint32_t edge_index;
    KitRenderRect rect;
    KitRenderVec2 anchor;
} KitGraphStructEdgeLabelLayout;

typedef struct KitGraphStructEdgeHit {
    int active;
    uint32_t edge_index;
    float distance_sq;
} KitGraphStructEdgeHit;

typedef struct KitGraphStructEdgeLabelHit {
    int active;
    uint32_t edge_index;
} KitGraphStructEdgeLabelHit;

typedef enum KitGraphStructEdgeLabelDensityMode {
    KIT_GRAPH_STRUCT_EDGE_LABEL_DENSITY_ALL = 0,
    KIT_GRAPH_STRUCT_EDGE_LABEL_DENSITY_CULL_OVERLAP = 1
} KitGraphStructEdgeLabelDensityMode;

typedef struct KitGraphStructEdgeLabelOptions {
    KitGraphStructEdgeLabelDensityMode density_mode;
    float current_zoom;
    float min_zoom_for_labels;
} KitGraphStructEdgeLabelOptions;

void kit_graph_struct_layout_style_default(KitGraphStructLayoutStyle *out_style);
void kit_graph_struct_viewport_default(KitGraphStructViewport *out_viewport);
void kit_graph_struct_edge_label_options_default(KitGraphStructEdgeLabelOptions *out_options);

CoreResult kit_graph_struct_compute_layered_tree_layout(const KitGraphStructNode *nodes,
                                                        uint32_t node_count,
                                                        const KitGraphStructEdge *edges,
                                                        uint32_t edge_count,
                                                        KitRenderRect bounds,
                                                        const KitGraphStructViewport *viewport,
                                                        const KitGraphStructLayoutStyle *style,
                                                        KitGraphStructNodeLayout *out_layouts);

CoreResult kit_graph_struct_compute_layered_dag_layout(const KitGraphStructNode *nodes,
                                                       uint32_t node_count,
                                                       const KitGraphStructEdge *edges,
                                                       uint32_t edge_count,
                                                       KitRenderRect bounds,
                                                       const KitGraphStructViewport *viewport,
                                                       const KitGraphStructLayoutStyle *style,
                                                       KitGraphStructNodeLayout *out_layouts);

CoreResult kit_graph_struct_hit_test(const KitGraphStructNodeLayout *layouts,
                                     uint32_t node_count,
                                     float mouse_x,
                                     float mouse_y,
                                     KitGraphStructHit *out_hit);

CoreResult kit_graph_struct_hit_test_edge_routes(const KitGraphStructEdgeRoute *routes,
                                                 uint32_t route_count,
                                                 float mouse_x,
                                                 float mouse_y,
                                                 float threshold_px,
                                                 KitGraphStructEdgeHit *out_hit);

CoreResult kit_graph_struct_hit_test_edge_labels(const KitGraphStructEdgeLabelLayout *label_layouts,
                                                 uint32_t label_count,
                                                 float mouse_x,
                                                 float mouse_y,
                                                 KitGraphStructEdgeLabelHit *out_hit);

CoreResult kit_graph_struct_focus_on_node(const KitGraphStructNodeLayout *layouts,
                                          uint32_t node_count,
                                          uint32_t node_id,
                                          KitRenderRect bounds,
                                          float padding,
                                          KitGraphStructViewport *viewport);

CoreResult kit_graph_struct_viewport_pan_by(KitGraphStructViewport *viewport,
                                            float delta_x,
                                            float delta_y);

CoreResult kit_graph_struct_viewport_zoom_by(KitGraphStructViewport *viewport,
                                             float factor,
                                             float min_zoom,
                                             float max_zoom);

CoreResult kit_graph_struct_viewport_center_layout(const KitGraphStructNodeLayout *layouts,
                                                   uint32_t node_count,
                                                   KitRenderRect bounds,
                                                   float padding,
                                                   KitGraphStructViewport *viewport);

CoreResult kit_graph_struct_compute_edge_label_layouts(const KitGraphStructNode *nodes,
                                                       uint32_t node_count,
                                                       const KitGraphStructEdge *edges,
                                                       uint32_t edge_count,
                                                       const KitGraphStructNodeLayout *layouts,
                                                       const KitGraphStructLayoutStyle *style,
                                                       const KitGraphStructEdgeLabel *edge_labels,
                                                       uint32_t edge_label_count,
                                                       KitGraphStructEdgeLabelLayout *out_layouts);

CoreResult kit_graph_struct_compute_edge_label_layouts_routed(const KitGraphStructNodeLayout *layouts,
                                                              uint32_t node_count,
                                                              const KitGraphStructEdgeRoute *routes,
                                                              uint32_t route_count,
                                                              const KitGraphStructLayoutStyle *style,
                                                              const KitGraphStructEdgeLabel *edge_labels,
                                                              uint32_t edge_label_count,
                                                              const KitGraphStructEdgeLabelOptions *options,
                                                              KitGraphStructEdgeLabelLayout *out_layouts);

CoreResult kit_graph_struct_compute_edge_routes(const KitGraphStructEdge *edges,
                                                uint32_t edge_count,
                                                const KitGraphStructNodeLayout *layouts,
                                                uint32_t layout_count,
                                                KitGraphStructRouteMode route_mode,
                                                KitGraphStructEdgeRoute *out_routes);

CoreResult kit_graph_struct_draw(KitRenderFrame *frame,
                                 KitRenderRect bounds,
                                 const KitGraphStructNode *nodes,
                                 uint32_t node_count,
                                 const KitGraphStructEdge *edges,
                                 uint32_t edge_count,
                                 const KitGraphStructNodeLayout *layouts,
                                 uint32_t selected_node_id,
                                 int has_selected,
                                 uint32_t hovered_node_id,
                                 int has_hovered);

#ifdef __cplusplus
}
#endif

#endif
