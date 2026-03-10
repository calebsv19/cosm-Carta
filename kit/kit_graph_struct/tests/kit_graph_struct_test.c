#include "kit_graph_struct.h"

#include <math.h>
#include <stdio.h>

static int nearf(float a, float b) {
    return fabsf(a - b) < 1e-5f;
}

static CoreResult test_measure_text_callback(void *user,
                                             CoreFontRoleId font_role,
                                             CoreFontTextSizeTier text_tier,
                                             const char *text,
                                             KitRenderTextMetrics *out_metrics) {
    const float *forced_width = (const float *)user;
    (void)font_role;
    (void)text_tier;
    (void)text;
    if (!forced_width || !out_metrics) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid test callback" };
    }
    out_metrics->width_px = *forced_width;
    out_metrics->height_px = 16.0f;
    return core_result_ok();
}

static int rect_overlap(KitRenderRect a, KitRenderRect b) {
    if (a.x + a.width <= b.x) return 0;
    if (b.x + b.width <= a.x) return 0;
    if (a.y + a.height <= b.y) return 0;
    if (b.y + b.height <= a.y) return 0;
    return 1;
}

static int find_layout_index(const KitGraphStructNodeLayout *layouts,
                             uint32_t node_count,
                             uint32_t node_id) {
    uint32_t i;
    for (i = 0u; i < node_count; ++i) {
        if (layouts[i].node_id == node_id) {
            return (int)i;
        }
    }
    return -1;
}

static int test_tree_layout(void) {
    KitGraphStructNode nodes[4] = {
        {1u, "Root"},
        {2u, "Left"},
        {3u, "Right"},
        {4u, "Leaf"}
    };
    KitGraphStructEdge edges[3] = {
        {1u, 2u},
        {1u, 3u},
        {2u, 4u}
    };
    KitGraphStructNodeLayout layouts[4];
    CoreResult result;

    result = kit_graph_struct_compute_layered_tree_layout(nodes,
                                                          4u,
                                                          edges,
                                                          3u,
                                                          (KitRenderRect){0.0f, 0.0f, 640.0f, 480.0f},
                                                          0,
                                                          0,
                                                          layouts);
    if (result.code != CORE_OK) return 1;
    if (layouts[0].depth != 0u) return 1;
    if (layouts[1].depth != 1u) return 1;
    if (layouts[3].depth != 2u) return 1;
    if (!(layouts[2].rect.x > layouts[1].rect.x)) return 1;
    if (!(layouts[3].rect.y > layouts[1].rect.y)) return 1;
    if (!nearf(layouts[0].rect.width, 120.0f)) return 1;
    return 0;
}

static int test_dag_layout_and_focus(void) {
    KitGraphStructNode nodes[5] = {
        {1u, "A"},
        {2u, "B"},
        {3u, "C"},
        {4u, "D"},
        {5u, "E"}
    };
    KitGraphStructEdge edges[5] = {
        {1u, 2u},
        {1u, 3u},
        {2u, 4u},
        {3u, 4u},
        {4u, 5u}
    };
    KitGraphStructNodeLayout layouts[5];
    KitGraphStructViewport viewport;
    CoreResult result;

    kit_graph_struct_viewport_default(&viewport);
    result = kit_graph_struct_compute_layered_dag_layout(nodes,
                                                         5u,
                                                         edges,
                                                         5u,
                                                         (KitRenderRect){0.0f, 0.0f, 800.0f, 600.0f},
                                                         &viewport,
                                                         0,
                                                         layouts);
    if (result.code != CORE_OK) return 1;
    if (layouts[0].depth != 0u) return 1;
    if (layouts[1].depth != 1u) return 1;
    if (layouts[2].depth != 1u) return 1;
    if (layouts[3].depth != 2u) return 1;
    if (layouts[4].depth != 3u) return 1;

    result = kit_graph_struct_focus_on_node(layouts,
                                            5u,
                                            5u,
                                            (KitRenderRect){0.0f, 0.0f, 800.0f, 600.0f},
                                            24.0f,
                                            &viewport);
    if (result.code != CORE_OK) return 1;
    if (!(viewport.pan_x < 0.0f || viewport.pan_y < 0.0f)) return 1;
    return 0;
}

static int test_hit_and_draw(void) {
    KitGraphStructNode nodes[2] = {
        {1u, "A"},
        {2u, "B"}
    };
    KitGraphStructEdge edges[1] = {
        {1u, 2u}
    };
    KitGraphStructNodeLayout layouts[2] = {
        {1u, {20.0f, 30.0f, 100.0f, 40.0f}, 0u},
        {2u, {160.0f, 120.0f, 100.0f, 40.0f}, 1u}
    };
    KitGraphStructHit hit;
    KitRenderContext render_ctx;
    KitRenderCommand commands[64];
    KitRenderCommandBuffer buffer;
    KitRenderFrame frame;
    CoreResult result;

    result = kit_graph_struct_hit_test(layouts, 2u, 40.0f, 50.0f, &hit);
    if (result.code != CORE_OK) return 1;
    if (!hit.active || hit.node_id != 1u) return 1;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;

    buffer.commands = commands;
    buffer.capacity = 64;
    buffer.count = 0;
    result = kit_render_begin_frame(&render_ctx, 640u, 480u, &buffer, &frame);
    if (result.code != CORE_OK) return 1;

    result = kit_graph_struct_draw(&frame,
                                   (KitRenderRect){10.0f, 10.0f, 300.0f, 220.0f},
                                   nodes,
                                   2u,
                                   edges,
                                   1u,
                                   layouts,
                                   1u,
                                   1,
                                   2u,
                                   1);
    if (result.code != CORE_OK) return 1;
    if (buffer.count < 6u) {
        fprintf(stderr, "expected at least 6 commands, got %zu\n", buffer.count);
        return 1;
    }

    result = kit_render_end_frame(&render_ctx, &frame);
    if (result.code != CORE_OK) return 1;
    return 0;
}

static int test_edge_label_layouts(void) {
    KitGraphStructNode nodes[3] = {
        {1u, "Root Node"},
        {2u, "Dependency A"},
        {3u, "Dependency B"}
    };
    KitGraphStructEdge edges[2] = {
        {2u, 1u},
        {3u, 1u}
    };
    KitGraphStructEdgeLabel labels[2] = {
        {"supports"},
        {"references"}
    };
    KitGraphStructNodeLayout layouts[3];
    KitGraphStructEdgeLabelLayout edge_layouts[2];
    CoreResult result;

    result = kit_graph_struct_compute_layered_dag_layout(nodes,
                                                         3u,
                                                         edges,
                                                         2u,
                                                         (KitRenderRect){0.0f, 0.0f, 700.0f, 480.0f},
                                                         0,
                                                         0,
                                                         layouts);
    if (result.code != CORE_OK) return 1;

    result = kit_graph_struct_compute_edge_label_layouts(nodes,
                                                         3u,
                                                         edges,
                                                         2u,
                                                         layouts,
                                                         0,
                                                         labels,
                                                         2u,
                                                         edge_layouts);
    if (result.code != CORE_OK) return 1;
    if (!(edge_layouts[0].rect.width > 40.0f)) return 1;
    if (!(edge_layouts[1].rect.width > 40.0f)) return 1;
    if (!(edge_layouts[0].rect.height > 0.0f)) return 1;
    if (!(edge_layouts[1].rect.height > 0.0f)) return 1;
    if (rect_overlap(edge_layouts[0].rect, layouts[0].rect)) return 1;
    if (rect_overlap(edge_layouts[0].rect, layouts[1].rect)) return 1;
    if (rect_overlap(edge_layouts[0].rect, layouts[2].rect)) return 1;
    if (rect_overlap(edge_layouts[1].rect, layouts[0].rect)) return 1;
    if (rect_overlap(edge_layouts[1].rect, layouts[1].rect)) return 1;
    if (rect_overlap(edge_layouts[1].rect, layouts[2].rect)) return 1;
    return 0;
}

static int test_layered_layout_is_stable_for_input_permutations(void) {
    KitGraphStructNode nodes_a[4] = {
        {10u, "Root"},
        {20u, "Gamma"},
        {30u, "Alpha"},
        {40u, "Beta"}
    };
    KitGraphStructNode nodes_b[4] = {
        {30u, "Alpha"},
        {10u, "Root"},
        {40u, "Beta"},
        {20u, "Gamma"}
    };
    KitGraphStructEdge edges[3] = {
        {10u, 20u},
        {10u, 30u},
        {10u, 40u}
    };
    KitGraphStructNodeLayout layouts_a[4];
    KitGraphStructNodeLayout layouts_b[4];
    CoreResult result;
    int idx_a_alpha;
    int idx_a_beta;
    int idx_a_gamma;
    int idx_b_alpha;
    int idx_b_beta;
    int idx_b_gamma;

    result = kit_graph_struct_compute_layered_dag_layout(nodes_a,
                                                         4u,
                                                         edges,
                                                         3u,
                                                         (KitRenderRect){0.0f, 0.0f, 900.0f, 540.0f},
                                                         0,
                                                         0,
                                                         layouts_a);
    if (result.code != CORE_OK) return 1;

    result = kit_graph_struct_compute_layered_dag_layout(nodes_b,
                                                         4u,
                                                         edges,
                                                         3u,
                                                         (KitRenderRect){0.0f, 0.0f, 900.0f, 540.0f},
                                                         0,
                                                         0,
                                                         layouts_b);
    if (result.code != CORE_OK) return 1;

    idx_a_alpha = find_layout_index(layouts_a, 4u, 30u);
    idx_a_beta = find_layout_index(layouts_a, 4u, 40u);
    idx_a_gamma = find_layout_index(layouts_a, 4u, 20u);
    idx_b_alpha = find_layout_index(layouts_b, 4u, 30u);
    idx_b_beta = find_layout_index(layouts_b, 4u, 40u);
    idx_b_gamma = find_layout_index(layouts_b, 4u, 20u);
    if (idx_a_alpha < 0 || idx_a_beta < 0 || idx_a_gamma < 0 ||
        idx_b_alpha < 0 || idx_b_beta < 0 || idx_b_gamma < 0) {
        return 1;
    }

    if (!(layouts_a[idx_a_alpha].rect.x < layouts_a[idx_a_beta].rect.x &&
          layouts_a[idx_a_beta].rect.x < layouts_a[idx_a_gamma].rect.x)) {
        return 1;
    }
    if (!(layouts_b[idx_b_alpha].rect.x < layouts_b[idx_b_beta].rect.x &&
          layouts_b[idx_b_beta].rect.x < layouts_b[idx_b_gamma].rect.x)) {
        return 1;
    }

    if (!nearf(layouts_a[idx_a_alpha].rect.x, layouts_b[idx_b_alpha].rect.x)) return 1;
    if (!nearf(layouts_a[idx_a_beta].rect.x, layouts_b[idx_b_beta].rect.x)) return 1;
    if (!nearf(layouts_a[idx_a_gamma].rect.x, layouts_b[idx_b_gamma].rect.x)) return 1;
    return 0;
}

static int test_layered_layout_reduces_simple_crossing(void) {
    KitGraphStructNode nodes[4] = {
        {1u, "A"},
        {2u, "B"},
        {3u, "C"},
        {4u, "D"}
    };
    KitGraphStructEdge edges[2] = {
        {1u, 4u},
        {2u, 3u}
    };
    KitGraphStructNodeLayout layouts[4];
    CoreResult result;
    int idx_c;
    int idx_d;

    result = kit_graph_struct_compute_layered_dag_layout(nodes,
                                                         4u,
                                                         edges,
                                                         2u,
                                                         (KitRenderRect){0.0f, 0.0f, 900.0f, 540.0f},
                                                         0,
                                                         0,
                                                         layouts);
    if (result.code != CORE_OK) return 1;

    idx_c = find_layout_index(layouts, 4u, 3u);
    idx_d = find_layout_index(layouts, 4u, 4u);
    if (idx_c < 0 || idx_d < 0) return 1;

    /* Without crossing reduction label order would place C before D.
       For edges A->D and B->C, a reduced-crossing order puts D left of C. */
    if (!(layouts[idx_d].rect.x < layouts[idx_c].rect.x)) return 1;
    return 0;
}

static int test_edge_routes_modes_and_boundary_attach(void) {
    KitGraphStructEdge edges[2] = {
        {1u, 2u},
        {1u, 3u}
    };
    KitGraphStructNodeLayout layouts[3] = {
        {1u, {100.0f, 100.0f, 80.0f, 40.0f}, 0u},
        {2u, {260.0f, 110.0f, 80.0f, 40.0f}, 1u},
        {3u, {120.0f, 240.0f, 80.0f, 40.0f}, 1u}
    };
    KitGraphStructEdgeRoute straight_routes[2];
    KitGraphStructEdgeRoute ortho_routes[2];
    CoreResult result;

    result = kit_graph_struct_compute_edge_routes(edges,
                                                  2u,
                                                  layouts,
                                                  3u,
                                                  KIT_GRAPH_STRUCT_ROUTE_STRAIGHT,
                                                  straight_routes);
    if (result.code != CORE_OK) return 1;
    if (straight_routes[0].point_count != 2u) return 1;
    if (straight_routes[1].point_count != 2u) return 1;

    /* Edge 1->2 should attach from right side of node 1 to left side of node 2. */
    if (!nearf(straight_routes[0].points[0].x, 180.0f)) return 1;
    if (!nearf(straight_routes[0].points[0].y, 120.0f)) return 1;
    if (!nearf(straight_routes[0].points[1].x, 260.0f)) return 1;
    if (!nearf(straight_routes[0].points[1].y, 130.0f)) return 1;

    /* Edge 1->3 should attach from bottom of node 1 to top of node 3. */
    if (!nearf(straight_routes[1].points[0].x, 140.0f)) return 1;
    if (!nearf(straight_routes[1].points[0].y, 140.0f)) return 1;
    if (!nearf(straight_routes[1].points[1].x, 160.0f)) return 1;
    if (!nearf(straight_routes[1].points[1].y, 240.0f)) return 1;

    result = kit_graph_struct_compute_edge_routes(edges,
                                                  2u,
                                                  layouts,
                                                  3u,
                                                  KIT_GRAPH_STRUCT_ROUTE_ORTHOGONAL,
                                                  ortho_routes);
    if (result.code != CORE_OK) return 1;
    if (ortho_routes[0].point_count != 4u) return 1;
    if (ortho_routes[1].point_count != 4u) return 1;

    /* Orthogonal route uses axis-aligned segments. */
    if (!nearf(ortho_routes[0].points[0].y, ortho_routes[0].points[1].y)) return 1;
    if (!nearf(ortho_routes[0].points[1].x, ortho_routes[0].points[2].x)) return 1;
    if (!nearf(ortho_routes[0].points[2].y, ortho_routes[0].points[3].y)) return 1;
    return 0;
}

static int test_routed_edge_label_layouts_with_density_controls(void) {
    KitGraphStructNode nodes[3] = {
        {1u, "Root"},
        {2u, "Alpha"},
        {3u, "Beta"}
    };
    KitGraphStructEdge edges[2] = {
        {1u, 2u},
        {1u, 3u}
    };
    KitGraphStructNodeLayout layouts[3];
    KitGraphStructEdgeRoute routes[2];
    KitGraphStructEdgeLabel labels[2] = {
        {"supports"},
        {"references"}
    };
    KitGraphStructEdgeLabelLayout label_layouts[2];
    KitGraphStructEdgeLabelOptions options;
    CoreResult result;

    result = kit_graph_struct_compute_layered_dag_layout(nodes,
                                                         3u,
                                                         edges,
                                                         2u,
                                                         (KitRenderRect){0.0f, 0.0f, 700.0f, 480.0f},
                                                         0,
                                                         0,
                                                         layouts);
    if (result.code != CORE_OK) return 1;

    result = kit_graph_struct_compute_edge_routes(edges,
                                                  2u,
                                                  layouts,
                                                  3u,
                                                  KIT_GRAPH_STRUCT_ROUTE_ORTHOGONAL,
                                                  routes);
    if (result.code != CORE_OK) return 1;

    kit_graph_struct_edge_label_options_default(&options);
    options.density_mode = KIT_GRAPH_STRUCT_EDGE_LABEL_DENSITY_CULL_OVERLAP;
    options.current_zoom = 1.0f;
    options.min_zoom_for_labels = 0.0f;
    result = kit_graph_struct_compute_edge_label_layouts_routed(layouts,
                                                                3u,
                                                                routes,
                                                                2u,
                                                                0,
                                                                labels,
                                                                2u,
                                                                &options,
                                                                label_layouts);
    if (result.code != CORE_OK) return 1;
    if (!(label_layouts[0].rect.width > 0.0f)) return 1;
    if (!(label_layouts[1].rect.width > 0.0f)) return 1;

    options.min_zoom_for_labels = 2.0f;
    result = kit_graph_struct_compute_edge_label_layouts_routed(layouts,
                                                                3u,
                                                                routes,
                                                                2u,
                                                                0,
                                                                labels,
                                                                2u,
                                                                &options,
                                                                label_layouts);
    if (result.code != CORE_OK) return 1;
    if (!(label_layouts[0].rect.width == 0.0f && label_layouts[0].rect.height == 0.0f)) return 1;
    if (!(label_layouts[1].rect.width == 0.0f && label_layouts[1].rect.height == 0.0f)) return 1;
    return 0;
}

static int test_edge_and_label_hit_helpers_and_viewport_helpers(void) {
    KitGraphStructEdgeRoute routes[2] = {
        {0u, 4u, {{100.0f, 120.0f}, {180.0f, 120.0f}, {180.0f, 180.0f}, {240.0f, 180.0f}}},
        {1u, 2u, {{220.0f, 220.0f}, {300.0f, 260.0f}}}
    };
    KitGraphStructEdgeLabelLayout labels[2] = {
        {0u, {140.0f, 108.0f, 64.0f, 16.0f}, {172.0f, 116.0f}},
        {1u, {260.0f, 252.0f, 58.0f, 16.0f}, {289.0f, 260.0f}}
    };
    KitGraphStructEdgeHit edge_hit;
    KitGraphStructEdgeLabelHit label_hit;
    KitGraphStructNodeLayout layouts[2] = {
        {1u, {20.0f, 30.0f, 80.0f, 40.0f}, 0u},
        {2u, {180.0f, 150.0f, 90.0f, 44.0f}, 1u}
    };
    KitGraphStructViewport viewport;
    CoreResult result;

    result = kit_graph_struct_hit_test_edge_routes(routes, 2u, 179.0f, 130.0f, 10.0f, &edge_hit);
    if (result.code != CORE_OK) return 1;
    if (!edge_hit.active || edge_hit.edge_index != 0u) return 1;

    result = kit_graph_struct_hit_test_edge_labels(labels, 2u, 145.0f, 112.0f, &label_hit);
    if (result.code != CORE_OK) return 1;
    if (!label_hit.active || label_hit.edge_index != 0u) return 1;

    kit_graph_struct_viewport_default(&viewport);
    result = kit_graph_struct_viewport_pan_by(&viewport, 12.0f, -6.0f);
    if (result.code != CORE_OK) return 1;
    if (!nearf(viewport.pan_x, 12.0f) || !nearf(viewport.pan_y, -6.0f)) return 1;

    result = kit_graph_struct_viewport_zoom_by(&viewport, 1.5f, 0.5f, 2.0f);
    if (result.code != CORE_OK) return 1;
    if (!nearf(viewport.zoom, 1.5f)) return 1;

    result = kit_graph_struct_viewport_center_layout(layouts,
                                                     2u,
                                                     (KitRenderRect){0.0f, 0.0f, 500.0f, 400.0f},
                                                     20.0f,
                                                     &viewport);
    if (result.code != CORE_OK) return 1;
    if (!(viewport.pan_x > 12.0f)) return 1;
    return 0;
}

static int test_layout_uses_measure_text_callback(void) {
    KitGraphStructNode nodes[1] = {
        {1u, "A"}
    };
    KitGraphStructNodeLayout layouts[1];
    KitGraphStructLayoutStyle style;
    float forced_label_width = 280.0f;
    CoreResult result;

    kit_graph_struct_layout_style_default(&style);
    style.node_width = 80.0f;
    style.node_min_width = 20.0f;
    style.node_max_width = 500.0f;
    style.node_padding_x = 6.0f;
    style.measure_text_fn = test_measure_text_callback;
    style.measure_text_user = &forced_label_width;

    result = kit_graph_struct_compute_layered_dag_layout(nodes,
                                                         1u,
                                                         0,
                                                         0u,
                                                         (KitRenderRect){0.0f, 0.0f, 640.0f, 480.0f},
                                                         0,
                                                         &style,
                                                         layouts);
    if (result.code != CORE_OK) return 1;
    if (!(layouts[0].rect.width > 280.0f)) return 1;
    return 0;
}

int main(void) {
    if (test_tree_layout() != 0) {
        fprintf(stderr, "test_tree_layout failed\n");
        return 1;
    }
    if (test_dag_layout_and_focus() != 0) {
        fprintf(stderr, "test_dag_layout_and_focus failed\n");
        return 1;
    }
    if (test_hit_and_draw() != 0) {
        fprintf(stderr, "test_hit_and_draw failed\n");
        return 1;
    }
    if (test_edge_label_layouts() != 0) {
        fprintf(stderr, "test_edge_label_layouts failed\n");
        return 1;
    }
    if (test_layered_layout_is_stable_for_input_permutations() != 0) {
        fprintf(stderr, "test_layered_layout_is_stable_for_input_permutations failed\n");
        return 1;
    }
    if (test_layered_layout_reduces_simple_crossing() != 0) {
        fprintf(stderr, "test_layered_layout_reduces_simple_crossing failed\n");
        return 1;
    }
    if (test_edge_routes_modes_and_boundary_attach() != 0) {
        fprintf(stderr, "test_edge_routes_modes_and_boundary_attach failed\n");
        return 1;
    }
    if (test_routed_edge_label_layouts_with_density_controls() != 0) {
        fprintf(stderr, "test_routed_edge_label_layouts_with_density_controls failed\n");
        return 1;
    }
    if (test_edge_and_label_hit_helpers_and_viewport_helpers() != 0) {
        fprintf(stderr, "test_edge_and_label_hit_helpers_and_viewport_helpers failed\n");
        return 1;
    }
    if (test_layout_uses_measure_text_callback() != 0) {
        fprintf(stderr, "test_layout_uses_measure_text_callback failed\n");
        return 1;
    }
    puts("kit_graph_struct tests passed");
    return 0;
}
