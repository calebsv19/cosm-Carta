/*
 * kit_graph_struct.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "kit_graph_struct.h"

#include <stdlib.h>
#include <string.h>

static CoreResult kit_graph_struct_invalid(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static int find_node_index(const KitGraphStructNode *nodes,
                           uint32_t node_count,
                           uint32_t node_id) {
    uint32_t i;
    for (i = 0; i < node_count; ++i) {
        if (nodes[i].id == node_id) {
            return (int)i;
        }
    }
    return -1;
}

static int find_layout_index(const KitGraphStructNodeLayout *layouts,
                             uint32_t layout_count,
                             uint32_t node_id) {
    uint32_t i;
    for (i = 0u; i < layout_count; ++i) {
        if (layouts[i].node_id == node_id) {
            return (int)i;
        }
    }
    return -1;
}

static KitGraphStructLayoutStyle layout_style_or_default(const KitGraphStructLayoutStyle *style) {
    KitGraphStructLayoutStyle local;
    if (style) {
        return *style;
    }
    kit_graph_struct_layout_style_default(&local);
    return local;
}

static KitGraphStructViewport viewport_or_default(const KitGraphStructViewport *viewport) {
    KitGraphStructViewport local;
    if (viewport) {
        return *viewport;
    }
    kit_graph_struct_viewport_default(&local);
    return local;
}

static int node_layout_order_compare(const KitGraphStructNode *nodes,
                                     uint32_t a_index,
                                     uint32_t b_index) {
    const char *a_label;
    const char *b_label;
    int label_cmp;

    if (!nodes) {
        return 0;
    }

    a_label = nodes[a_index].label ? nodes[a_index].label : "";
    b_label = nodes[b_index].label ? nodes[b_index].label : "";
    label_cmp = strcmp(a_label, b_label);
    if (label_cmp != 0) {
        return label_cmp;
    }
    if (nodes[a_index].id < nodes[b_index].id) {
        return -1;
    }
    if (nodes[a_index].id > nodes[b_index].id) {
        return 1;
    }
    return 0;
}

static CoreResult compute_depths_longest_path(const KitGraphStructNode *nodes,
                                              uint32_t node_count,
                                              const KitGraphStructEdge *edges,
                                              uint32_t edge_count,
                                              uint32_t *out_depths) {
    uint32_t *indegree;
    uint32_t *queue;
    uint32_t head = 0u;
    uint32_t tail = 0u;
    uint32_t i;
    uint32_t visited = 0u;

    if (!nodes || !out_depths) {
        return kit_graph_struct_invalid("invalid depth computation");
    }

    indegree = (uint32_t *)calloc(node_count, sizeof(uint32_t));
    queue = (uint32_t *)calloc(node_count, sizeof(uint32_t));
    if (!indegree || !queue) {
        free(indegree);
        free(queue);
        return kit_graph_struct_invalid("allocation failed");
    }

    for (i = 0; i < node_count; ++i) {
        out_depths[i] = 0u;
    }

    for (i = 0; i < edge_count; ++i) {
        int to_index = find_node_index(nodes, node_count, edges[i].to_id);
        if (to_index >= 0) {
            indegree[to_index] += 1u;
        }
    }

    for (i = 0; i < node_count; ++i) {
        if (indegree[i] == 0u) {
            queue[tail++] = i;
        }
    }

    while (head < tail) {
        uint32_t from_index = queue[head++];
        uint32_t e;
        visited += 1u;

        for (e = 0; e < edge_count; ++e) {
            int edge_from_index = find_node_index(nodes, node_count, edges[e].from_id);
            int to_index = find_node_index(nodes, node_count, edges[e].to_id);
            if (edge_from_index < 0 || to_index < 0) {
                continue;
            }
            if ((uint32_t)edge_from_index != from_index) {
                continue;
            }
            if (out_depths[to_index] < out_depths[from_index] + 1u) {
                out_depths[to_index] = out_depths[from_index] + 1u;
            }
            if (indegree[to_index] > 0u) {
                indegree[to_index] -= 1u;
                if (indegree[to_index] == 0u) {
                    queue[tail++] = (uint32_t)to_index;
                }
            }
        }
    }

    if (visited < node_count) {
        for (i = 0; i < node_count; ++i) {
            if (indegree[i] != 0u) {
                out_depths[i] = 0u;
            }
        }
    }

    free(indegree);
    free(queue);
    return core_result_ok();
}

static void rebuild_layer_local_positions(const uint32_t *ordered_node_indices,
                                          const uint32_t *level_offsets,
                                          uint32_t max_depth,
                                          uint32_t *out_local_position) {
    uint32_t level;
    if (!ordered_node_indices || !level_offsets || !out_local_position) {
        return;
    }
    for (level = 0u; level <= max_depth; ++level) {
        uint32_t start = level_offsets[level];
        uint32_t end = level_offsets[level + 1u];
        uint32_t p;
        for (p = start; p < end; ++p) {
            uint32_t node_index = ordered_node_indices[p];
            out_local_position[node_index] = p - start;
        }
    }
}

static void reorder_level_by_barycenter(const KitGraphStructNode *nodes,
                                        const uint32_t *depths,
                                        uint32_t level,
                                        int use_previous_layer,
                                        const uint32_t *level_offsets,
                                        uint32_t *ordered_node_indices,
                                        const int *edge_from_indices,
                                        const int *edge_to_indices,
                                        uint32_t edge_count,
                                        const uint32_t *local_position,
                                        float *bary_keys) {
    uint32_t start;
    uint32_t end;
    uint32_t j;

    if (!nodes || !depths || !level_offsets || !ordered_node_indices ||
        !edge_from_indices || !edge_to_indices || !local_position || !bary_keys) {
        return;
    }

    start = level_offsets[level];
    end = level_offsets[level + 1u];
    if (end <= start + 1u) {
        return;
    }

    for (j = start; j < end; ++j) {
        uint32_t node_index = ordered_node_indices[j];
        float sum = 0.0f;
        uint32_t count = 0u;
        uint32_t e;
        bary_keys[node_index] = (float)(j - start);

        for (e = 0u; e < edge_count; ++e) {
            int from_index = edge_from_indices[e];
            int to_index = edge_to_indices[e];

            if (from_index < 0 || to_index < 0) {
                continue;
            }
            if (use_previous_layer) {
                if ((uint32_t)to_index == node_index &&
                    depths[from_index] + 1u == depths[node_index]) {
                    sum += (float)local_position[from_index];
                    count += 1u;
                }
            } else {
                if ((uint32_t)from_index == node_index &&
                    depths[to_index] == depths[node_index] + 1u) {
                    sum += (float)local_position[to_index];
                    count += 1u;
                }
            }
        }
        if (count > 0u) {
            bary_keys[node_index] = sum / (float)count;
        }
    }

    for (j = start + 1u; j < end; ++j) {
        uint32_t key_node = ordered_node_indices[j];
        uint32_t k = j;
        while (k > start) {
            uint32_t prev_node = ordered_node_indices[k - 1u];
            float key_prev = bary_keys[prev_node];
            float key_cur = bary_keys[key_node];
            float diff = key_prev - key_cur;
            int move = 0;

            if (diff > 0.0001f) {
                move = 1;
            } else if (diff >= -0.0001f && diff <= 0.0001f &&
                       node_layout_order_compare(nodes, prev_node, key_node) > 0) {
                move = 1;
            }

            if (!move) {
                break;
            }
            ordered_node_indices[k] = prev_node;
            k -= 1u;
        }
        ordered_node_indices[k] = key_node;
    }
}

static CoreResult apply_layered_layout(const KitGraphStructNode *nodes,
                                       uint32_t node_count,
                                       const KitGraphStructEdge *edges,
                                       uint32_t edge_count,
                                       KitRenderRect bounds,
                                       const KitGraphStructViewport *viewport,
                                       const KitGraphStructLayoutStyle *style,
                                       const uint32_t *depths,
                                       KitGraphStructNodeLayout *out_layouts) {
    uint32_t *level_counts;
    uint32_t *level_offsets;
    uint32_t *level_fill;
    uint32_t *ordered_node_indices;
    uint32_t *local_position;
    float *level_row_widths;
    float *level_cursor_x;
    float *node_widths;
    float *bary_keys;
    int *edge_from_indices;
    int *edge_to_indices;
    uint32_t max_depth = 0u;
    uint32_t i;
    KitGraphStructLayoutStyle local_style;
    KitGraphStructViewport local_viewport;
    float inner_width;
    float base_node_width;
    float scaled_gap;
    float scaled_padding;
    float scaled_level_gap;
    float scaled_node_height;
    float scaled_min_width;
    float scaled_max_width;
    float scaled_char_width;
    float scaled_node_padding_x;
    CoreFontRoleId label_font_role;
    CoreFontTextSizeTier label_text_tier;

    if (!nodes || !depths || !out_layouts) {
        return kit_graph_struct_invalid("invalid layout application");
    }
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return kit_graph_struct_invalid("invalid layout bounds");
    }

    local_style = layout_style_or_default(style);
    local_viewport = viewport_or_default(viewport);
    if (local_viewport.zoom <= 0.0f) {
        return kit_graph_struct_invalid("invalid viewport zoom");
    }

    for (i = 0u; i < node_count; ++i) {
        if (depths[i] > max_depth) {
            max_depth = depths[i];
        }
    }

    level_counts = (uint32_t *)calloc(max_depth + 1u, sizeof(uint32_t));
    level_offsets = (uint32_t *)calloc(max_depth + 2u, sizeof(uint32_t));
    level_fill = (uint32_t *)calloc(max_depth + 1u, sizeof(uint32_t));
    ordered_node_indices = (uint32_t *)calloc(node_count, sizeof(uint32_t));
    local_position = (uint32_t *)calloc(node_count, sizeof(uint32_t));
    level_row_widths = (float *)calloc(max_depth + 1u, sizeof(float));
    level_cursor_x = (float *)calloc(max_depth + 1u, sizeof(float));
    node_widths = (float *)calloc(node_count, sizeof(float));
    bary_keys = (float *)calloc(node_count, sizeof(float));
    edge_from_indices = (int *)calloc(edge_count > 0u ? edge_count : 1u, sizeof(int));
    edge_to_indices = (int *)calloc(edge_count > 0u ? edge_count : 1u, sizeof(int));
    if (!level_counts || !level_offsets || !level_fill || !ordered_node_indices ||
        !local_position || !level_row_widths || !level_cursor_x || !node_widths ||
        !bary_keys || !edge_from_indices || !edge_to_indices) {
        free(level_counts);
        free(level_offsets);
        free(level_fill);
        free(ordered_node_indices);
        free(local_position);
        free(level_row_widths);
        free(level_cursor_x);
        free(node_widths);
        free(bary_keys);
        free(edge_from_indices);
        free(edge_to_indices);
        return kit_graph_struct_invalid("allocation failed");
    }

    base_node_width = local_style.node_width;
    if (base_node_width <= 0.0f) {
        base_node_width = 120.0f;
    }
    scaled_gap = local_style.sibling_gap * local_viewport.zoom;
    scaled_padding = local_style.padding * local_viewport.zoom;
    scaled_level_gap = local_style.level_gap * local_viewport.zoom;
    scaled_node_height = local_style.node_height * local_viewport.zoom;
    scaled_min_width = local_style.node_min_width * local_viewport.zoom;
    if (scaled_min_width <= 0.0f) {
        scaled_min_width = base_node_width * local_viewport.zoom;
    }
    scaled_max_width = local_style.node_max_width * local_viewport.zoom;
    if (scaled_max_width < scaled_min_width) {
        scaled_max_width = scaled_min_width;
    }
    scaled_char_width = local_style.label_char_width * local_viewport.zoom;
    if (scaled_char_width <= 0.0f) {
        scaled_char_width = 7.0f * local_viewport.zoom;
    }
    scaled_node_padding_x = local_style.node_padding_x * local_viewport.zoom;
    if (scaled_node_padding_x < 0.0f) {
        scaled_node_padding_x = 0.0f;
    }
    inner_width = bounds.width - (scaled_padding * 2.0f);
    if (inner_width < 0.0f) {
        inner_width = 0.0f;
    }
    label_font_role = local_style.node_label_font_role;
    if ((int)label_font_role < 0 || label_font_role >= CORE_FONT_ROLE_COUNT) {
        label_font_role = CORE_FONT_ROLE_UI_MEDIUM;
    }
    label_text_tier = local_style.node_label_text_tier;
    if ((int)label_text_tier < 0 || label_text_tier >= CORE_FONT_TEXT_SIZE_COUNT) {
        label_text_tier = CORE_FONT_TEXT_SIZE_CAPTION;
    }

    for (i = 0; i < node_count; ++i) {
        uint32_t depth = depths[i];
        float label_width = base_node_width * local_viewport.zoom;
        float width = base_node_width * local_viewport.zoom;

        if (nodes[i].label && nodes[i].label[0] != '\0') {
            int measured = 0;
            if (local_style.measure_text_fn) {
                KitRenderTextMetrics metrics;
                CoreResult measure_result = local_style.measure_text_fn(local_style.measure_text_user,
                                                                        label_font_role,
                                                                        label_text_tier,
                                                                        nodes[i].label,
                                                                        &metrics);
                if (measure_result.code == CORE_OK) {
                    label_width = (metrics.width_px * local_viewport.zoom) + (scaled_node_padding_x * 2.0f);
                    measured = 1;
                }
            }
            if (!measured) {
                size_t label_len = strlen(nodes[i].label);
                label_width = ((float)label_len * scaled_char_width) + (scaled_node_padding_x * 2.0f);
            }
            if (label_width < scaled_min_width) {
                label_width = scaled_min_width;
            } else if (label_width > scaled_max_width) {
                label_width = scaled_max_width;
            }
            width = label_width;
        } else if (width < scaled_min_width) {
            width = scaled_min_width;
        }

        node_widths[i] = width;
        level_counts[depth] += 1u;
        level_row_widths[depth] += width;
    }

    level_offsets[0] = 0u;
    for (i = 0u; i <= max_depth; ++i) {
        level_offsets[i + 1u] = level_offsets[i] + level_counts[i];
        level_fill[i] = 0u;
    }
    for (i = 0u; i < node_count; ++i) {
        uint32_t depth = depths[i];
        uint32_t write_index = level_offsets[depth] + level_fill[depth];
        ordered_node_indices[write_index] = i;
        level_fill[depth] += 1u;
    }
    for (i = 0u; i <= max_depth; ++i) {
        uint32_t start = level_offsets[i];
        uint32_t end = level_offsets[i + 1u];
        uint32_t j;
        for (j = start + 1u; j < end; ++j) {
            uint32_t key = ordered_node_indices[j];
            uint32_t k = j;
            while (k > start &&
                   node_layout_order_compare(nodes,
                                             ordered_node_indices[k - 1u],
                                             key) > 0) {
                ordered_node_indices[k] = ordered_node_indices[k - 1u];
                k -= 1u;
            }
            ordered_node_indices[k] = key;
        }
    }

    if (edges && edge_count > 0u && max_depth > 0u) {
        int pass;
        for (i = 0u; i < edge_count; ++i) {
            edge_from_indices[i] = find_node_index(nodes, node_count, edges[i].from_id);
            edge_to_indices[i] = find_node_index(nodes, node_count, edges[i].to_id);
        }

        for (pass = 0; pass < 4; ++pass) {
            int d;

            rebuild_layer_local_positions(ordered_node_indices,
                                          level_offsets,
                                          max_depth,
                                          local_position);
            for (d = 1; d <= (int)max_depth; ++d) {
                reorder_level_by_barycenter(nodes,
                                            depths,
                                            (uint32_t)d,
                                            1,
                                            level_offsets,
                                            ordered_node_indices,
                                            edge_from_indices,
                                            edge_to_indices,
                                            edge_count,
                                            local_position,
                                            bary_keys);
            }

            rebuild_layer_local_positions(ordered_node_indices,
                                          level_offsets,
                                          max_depth,
                                          local_position);
            for (d = (int)max_depth - 1; d >= 0; --d) {
                reorder_level_by_barycenter(nodes,
                                            depths,
                                            (uint32_t)d,
                                            0,
                                            level_offsets,
                                            ordered_node_indices,
                                            edge_from_indices,
                                            edge_to_indices,
                                            edge_count,
                                            local_position,
                                            bary_keys);
            }
        }
    }

    for (i = 0u; i <= max_depth; ++i) {
        float gap_total = 0.0f;
        float start_x;
        if (level_counts[i] > 1u) {
            gap_total = ((float)level_counts[i] - 1.0f) * scaled_gap;
        }
        level_row_widths[i] += gap_total;

        start_x = bounds.x + scaled_padding + local_viewport.pan_x;
        if (inner_width > level_row_widths[i]) {
            start_x += (inner_width - level_row_widths[i]) * 0.5f;
        }
        level_cursor_x[i] = start_x;
    }

    for (i = 0u; i <= max_depth; ++i) {
        uint32_t start = level_offsets[i];
        uint32_t end = level_offsets[i + 1u];
        uint32_t j;
        for (j = start; j < end; ++j) {
            uint32_t node_index = ordered_node_indices[j];
            uint32_t depth = depths[node_index];
            float x = level_cursor_x[depth];
            float y = bounds.y + scaled_padding + local_viewport.pan_y +
                      (float)depth * (scaled_node_height + scaled_level_gap);

            out_layouts[node_index].node_id = nodes[node_index].id;
            out_layouts[node_index].depth = depth;
            out_layouts[node_index].rect.x = x;
            out_layouts[node_index].rect.y = y;
            out_layouts[node_index].rect.width = node_widths[node_index];
            out_layouts[node_index].rect.height = scaled_node_height;
            level_cursor_x[depth] += node_widths[node_index] + scaled_gap;
        }
    }

    free(level_counts);
    free(level_offsets);
    free(level_fill);
    free(ordered_node_indices);
    free(local_position);
    free(level_row_widths);
    free(level_cursor_x);
    free(node_widths);
    free(bary_keys);
    free(edge_from_indices);
    free(edge_to_indices);
    return core_result_ok();
}

void kit_graph_struct_layout_style_default(KitGraphStructLayoutStyle *out_style) {
    if (!out_style) return;
    out_style->padding = 24.0f;
    out_style->level_gap = 84.0f;
    out_style->sibling_gap = 28.0f;
    out_style->node_width = 120.0f;
    out_style->node_height = 42.0f;
    out_style->node_min_width = 120.0f;
    out_style->node_max_width = 240.0f;
    out_style->node_padding_x = 10.0f;
    out_style->label_char_width = 7.0f;
    out_style->edge_label_padding_x = 6.0f;
    out_style->edge_label_height = 16.0f;
    out_style->edge_label_lane_gap = 4.0f;
    out_style->node_label_font_role = CORE_FONT_ROLE_UI_MEDIUM;
    out_style->node_label_text_tier = CORE_FONT_TEXT_SIZE_CAPTION;
    out_style->measure_text_fn = 0;
    out_style->measure_text_user = 0;
}

void kit_graph_struct_viewport_default(KitGraphStructViewport *out_viewport) {
    if (!out_viewport) return;
    out_viewport->pan_x = 0.0f;
    out_viewport->pan_y = 0.0f;
    out_viewport->zoom = 1.0f;
}

void kit_graph_struct_edge_label_options_default(KitGraphStructEdgeLabelOptions *out_options) {
    if (!out_options) return;
    out_options->density_mode = KIT_GRAPH_STRUCT_EDGE_LABEL_DENSITY_ALL;
    out_options->current_zoom = 1.0f;
    out_options->min_zoom_for_labels = 0.0f;
}

CoreResult kit_graph_struct_compute_layered_tree_layout(const KitGraphStructNode *nodes,
                                                        uint32_t node_count,
                                                        const KitGraphStructEdge *edges,
                                                        uint32_t edge_count,
                                                        KitRenderRect bounds,
                                                        const KitGraphStructViewport *viewport,
                                                        const KitGraphStructLayoutStyle *style,
                                                        KitGraphStructNodeLayout *out_layouts) {
    uint32_t *depths;
    uint32_t i;
    CoreResult result;

    if (!nodes || node_count == 0u || !out_layouts) {
        return kit_graph_struct_invalid("invalid layout input");
    }

    depths = (uint32_t *)calloc(node_count, sizeof(uint32_t));
    if (!depths) {
        return kit_graph_struct_invalid("allocation failed");
    }

    for (i = 0; i < edge_count; ++i) {
        int from_index = find_node_index(nodes, node_count, edges[i].from_id);
        int to_index = find_node_index(nodes, node_count, edges[i].to_id);
        if (from_index < 0 || to_index < 0) {
            continue;
        }
        if (depths[to_index] < depths[from_index] + 1u) {
            depths[to_index] = depths[from_index] + 1u;
        }
    }

    result = apply_layered_layout(nodes,
                                  node_count,
                                  edges,
                                  edge_count,
                                  bounds,
                                  viewport,
                                  style,
                                  depths,
                                  out_layouts);
    free(depths);
    return result;
}

CoreResult kit_graph_struct_compute_layered_dag_layout(const KitGraphStructNode *nodes,
                                                       uint32_t node_count,
                                                       const KitGraphStructEdge *edges,
                                                       uint32_t edge_count,
                                                       KitRenderRect bounds,
                                                       const KitGraphStructViewport *viewport,
                                                       const KitGraphStructLayoutStyle *style,
                                                       KitGraphStructNodeLayout *out_layouts) {
    uint32_t *depths;
    CoreResult result;

    if (!nodes || node_count == 0u || !out_layouts) {
        return kit_graph_struct_invalid("invalid layout input");
    }

    depths = (uint32_t *)calloc(node_count, sizeof(uint32_t));
    if (!depths) {
        return kit_graph_struct_invalid("allocation failed");
    }

    result = compute_depths_longest_path(nodes, node_count, edges, edge_count, depths);
    if (result.code == CORE_OK) {
        result = apply_layered_layout(nodes,
                                      node_count,
                                      edges,
                                      edge_count,
                                      bounds,
                                      viewport,
                                      style,
                                      depths,
                                      out_layouts);
    }

    free(depths);
    return result;
}

CoreResult kit_graph_struct_hit_test(const KitGraphStructNodeLayout *layouts,
                                     uint32_t node_count,
                                     float mouse_x,
                                     float mouse_y,
                                     KitGraphStructHit *out_hit) {
    uint32_t i;

    if (!layouts || !out_hit) {
        return kit_graph_struct_invalid("invalid hit test");
    }

    out_hit->active = 0;
    out_hit->node_index = 0;
    out_hit->node_id = 0;

    for (i = 0; i < node_count; ++i) {
        const KitRenderRect *r = &layouts[i].rect;
        if (mouse_x < r->x || mouse_y < r->y) continue;
        if (mouse_x > r->x + r->width) continue;
        if (mouse_y > r->y + r->height) continue;
        out_hit->active = 1;
        out_hit->node_index = i;
        out_hit->node_id = layouts[i].node_id;
        break;
    }

    return core_result_ok();
}

static float point_to_segment_distance_sq(float px,
                                          float py,
                                          float x0,
                                          float y0,
                                          float x1,
                                          float y1) {
    float vx = x1 - x0;
    float vy = y1 - y0;
    float wx = px - x0;
    float wy = py - y0;
    float c1 = (wx * vx) + (wy * vy);
    float c2 = (vx * vx) + (vy * vy);
    float t = 0.0f;
    float proj_x;
    float proj_y;
    float dx;
    float dy;

    if (c2 > 0.0f) {
        t = c1 / c2;
    }
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }

    proj_x = x0 + (t * vx);
    proj_y = y0 + (t * vy);
    dx = px - proj_x;
    dy = py - proj_y;
    return (dx * dx) + (dy * dy);
}

CoreResult kit_graph_struct_hit_test_edge_routes(const KitGraphStructEdgeRoute *routes,
                                                 uint32_t route_count,
                                                 float mouse_x,
                                                 float mouse_y,
                                                 float threshold_px,
                                                 KitGraphStructEdgeHit *out_hit) {
    uint32_t i;
    float best_dist_sq = 0.0f;
    int has_best = 0;
    float threshold_sq;

    if (!routes || !out_hit) {
        return kit_graph_struct_invalid("invalid edge route hit test");
    }
    if (threshold_px < 0.0f) {
        return kit_graph_struct_invalid("invalid edge route hit threshold");
    }

    out_hit->active = 0;
    out_hit->edge_index = 0u;
    out_hit->distance_sq = 0.0f;
    threshold_sq = threshold_px * threshold_px;

    for (i = 0u; i < route_count; ++i) {
        const KitGraphStructEdgeRoute *route = &routes[i];
        uint32_t p;

        if (route->point_count < 2u) {
            continue;
        }

        for (p = 0u; p + 1u < route->point_count; ++p) {
            float dist_sq = point_to_segment_distance_sq(mouse_x,
                                                         mouse_y,
                                                         route->points[p].x,
                                                         route->points[p].y,
                                                         route->points[p + 1u].x,
                                                         route->points[p + 1u].y);
            if (dist_sq > threshold_sq) {
                continue;
            }
            if (!has_best || dist_sq < best_dist_sq) {
                best_dist_sq = dist_sq;
                out_hit->active = 1;
                out_hit->edge_index = route->edge_index;
                out_hit->distance_sq = dist_sq;
                has_best = 1;
            }
        }
    }

    return core_result_ok();
}

CoreResult kit_graph_struct_hit_test_edge_labels(const KitGraphStructEdgeLabelLayout *label_layouts,
                                                 uint32_t label_count,
                                                 float mouse_x,
                                                 float mouse_y,
                                                 KitGraphStructEdgeLabelHit *out_hit) {
    uint32_t i;

    if (!label_layouts || !out_hit) {
        return kit_graph_struct_invalid("invalid edge label hit test");
    }

    out_hit->active = 0;
    out_hit->edge_index = 0u;
    for (i = 0u; i < label_count; ++i) {
        const KitRenderRect *r = &label_layouts[i].rect;
        if (r->width <= 0.0f || r->height <= 0.0f) {
            continue;
        }
        if (mouse_x < r->x || mouse_y < r->y) continue;
        if (mouse_x > r->x + r->width) continue;
        if (mouse_y > r->y + r->height) continue;
        out_hit->active = 1;
        out_hit->edge_index = label_layouts[i].edge_index;
        break;
    }

    return core_result_ok();
}

CoreResult kit_graph_struct_focus_on_node(const KitGraphStructNodeLayout *layouts,
                                          uint32_t node_count,
                                          uint32_t node_id,
                                          KitRenderRect bounds,
                                          float padding,
                                          KitGraphStructViewport *viewport) {
    uint32_t i;
    float safe_padding = padding;

    if (!layouts || !viewport) {
        return kit_graph_struct_invalid("invalid focus input");
    }
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return kit_graph_struct_invalid("invalid focus bounds");
    }
    if (safe_padding < 0.0f) {
        safe_padding = 0.0f;
    }

    for (i = 0; i < node_count; ++i) {
        if (layouts[i].node_id == node_id) {
            float target_x = bounds.x + safe_padding + ((bounds.width - (safe_padding * 2.0f)) * 0.5f);
            float target_y = bounds.y + safe_padding + ((bounds.height - (safe_padding * 2.0f)) * 0.5f);
            float current_x = layouts[i].rect.x + (layouts[i].rect.width * 0.5f);
            float current_y = layouts[i].rect.y + (layouts[i].rect.height * 0.5f);
            viewport->pan_x += target_x - current_x;
            viewport->pan_y += target_y - current_y;
            return core_result_ok();
        }
    }

    return kit_graph_struct_invalid("node not found for focus");
}

CoreResult kit_graph_struct_viewport_pan_by(KitGraphStructViewport *viewport,
                                            float delta_x,
                                            float delta_y) {
    if (!viewport) {
        return kit_graph_struct_invalid("invalid viewport pan");
    }
    viewport->pan_x += delta_x;
    viewport->pan_y += delta_y;
    return core_result_ok();
}

CoreResult kit_graph_struct_viewport_zoom_by(KitGraphStructViewport *viewport,
                                             float factor,
                                             float min_zoom,
                                             float max_zoom) {
    float clamped_min_zoom = min_zoom;
    float clamped_max_zoom = max_zoom;
    float next_zoom;

    if (!viewport) {
        return kit_graph_struct_invalid("invalid viewport zoom");
    }
    if (factor <= 0.0f) {
        return kit_graph_struct_invalid("invalid zoom factor");
    }
    if (clamped_min_zoom <= 0.0f) {
        clamped_min_zoom = 0.1f;
    }
    if (clamped_max_zoom < clamped_min_zoom) {
        clamped_max_zoom = clamped_min_zoom;
    }

    next_zoom = viewport->zoom * factor;
    if (next_zoom < clamped_min_zoom) {
        next_zoom = clamped_min_zoom;
    } else if (next_zoom > clamped_max_zoom) {
        next_zoom = clamped_max_zoom;
    }
    viewport->zoom = next_zoom;
    return core_result_ok();
}

CoreResult kit_graph_struct_viewport_center_layout(const KitGraphStructNodeLayout *layouts,
                                                   uint32_t node_count,
                                                   KitRenderRect bounds,
                                                   float padding,
                                                   KitGraphStructViewport *viewport) {
    float safe_padding = padding;
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    float current_center_x;
    float current_center_y;
    float target_center_x;
    float target_center_y;
    uint32_t i;

    if (!layouts || node_count == 0u || !viewport) {
        return kit_graph_struct_invalid("invalid viewport center input");
    }
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return kit_graph_struct_invalid("invalid viewport center bounds");
    }
    if (safe_padding < 0.0f) {
        safe_padding = 0.0f;
    }

    min_x = layouts[0].rect.x;
    min_y = layouts[0].rect.y;
    max_x = layouts[0].rect.x + layouts[0].rect.width;
    max_y = layouts[0].rect.y + layouts[0].rect.height;
    for (i = 1u; i < node_count; ++i) {
        float x0 = layouts[i].rect.x;
        float y0 = layouts[i].rect.y;
        float x1 = layouts[i].rect.x + layouts[i].rect.width;
        float y1 = layouts[i].rect.y + layouts[i].rect.height;
        if (x0 < min_x) min_x = x0;
        if (y0 < min_y) min_y = y0;
        if (x1 > max_x) max_x = x1;
        if (y1 > max_y) max_y = y1;
    }

    current_center_x = (min_x + max_x) * 0.5f;
    current_center_y = (min_y + max_y) * 0.5f;
    target_center_x = bounds.x + safe_padding + ((bounds.width - (safe_padding * 2.0f)) * 0.5f);
    target_center_y = bounds.y + safe_padding + ((bounds.height - (safe_padding * 2.0f)) * 0.5f);
    viewport->pan_x += target_center_x - current_center_x;
    viewport->pan_y += target_center_y - current_center_y;
    return core_result_ok();
}

static int rects_overlap(KitRenderRect a, KitRenderRect b) {
    if (a.x + a.width <= b.x) return 0;
    if (b.x + b.width <= a.x) return 0;
    if (a.y + a.height <= b.y) return 0;
    if (b.y + b.height <= a.y) return 0;
    return 1;
}

static int rect_overlaps_any_node(KitRenderRect rect,
                                  const KitGraphStructNodeLayout *layouts,
                                  uint32_t node_count) {
    uint32_t i;
    for (i = 0u; i < node_count; ++i) {
        if (rects_overlap(rect, layouts[i].rect)) {
            return 1;
        }
    }
    return 0;
}

static int rect_overlaps_previous_labels(KitRenderRect rect,
                                         const KitGraphStructEdgeLabelLayout *label_layouts,
                                         uint32_t up_to_index) {
    uint32_t i;
    for (i = 0u; i < up_to_index; ++i) {
        if (label_layouts[i].rect.width <= 0.0f || label_layouts[i].rect.height <= 0.0f) {
            continue;
        }
        if (rects_overlap(rect, label_layouts[i].rect)) {
            return 1;
        }
    }
    return 0;
}

CoreResult kit_graph_struct_compute_edge_label_layouts(const KitGraphStructNode *nodes,
                                                       uint32_t node_count,
                                                       const KitGraphStructEdge *edges,
                                                       uint32_t edge_count,
                                                       const KitGraphStructNodeLayout *layouts,
                                                       const KitGraphStructLayoutStyle *style,
                                                       const KitGraphStructEdgeLabel *edge_labels,
                                                       uint32_t edge_label_count,
                                                       KitGraphStructEdgeLabelLayout *out_layouts) {
    KitGraphStructLayoutStyle local_style;
    uint32_t i;
    float char_width;
    float label_padding_x;
    float label_height;
    float lane_gap;

    if (!nodes || !edges || !layouts || !out_layouts) {
        return kit_graph_struct_invalid("invalid edge label layout input");
    }

    local_style = layout_style_or_default(style);
    char_width = local_style.label_char_width > 0.0f ? local_style.label_char_width : 7.0f;
    label_padding_x = local_style.edge_label_padding_x > 0.0f ? local_style.edge_label_padding_x : 6.0f;
    label_height = local_style.edge_label_height > 0.0f ? local_style.edge_label_height : 16.0f;
    lane_gap = local_style.edge_label_lane_gap >= 0.0f ? local_style.edge_label_lane_gap : 4.0f;

    for (i = 0u; i < edge_count; ++i) {
        int from_index = find_node_index(nodes, node_count, edges[i].from_id);
        int to_index = find_node_index(nodes, node_count, edges[i].to_id);
        const char *text = "";
        float text_width;
        float width;
        float mid_x;
        float mid_y;
        int attempt;
        int placed = 0;

        if (from_index < 0 || to_index < 0) {
            out_layouts[i].edge_index = i;
            out_layouts[i].anchor = (KitRenderVec2){0.0f, 0.0f};
            out_layouts[i].rect = (KitRenderRect){0.0f, 0.0f, 0.0f, 0.0f};
            continue;
        }

        if (edge_labels && i < edge_label_count && edge_labels[i].text) {
            text = edge_labels[i].text;
        }

        text_width = (float)strlen(text) * char_width;
        width = text_width + (label_padding_x * 2.0f);
        if (width < 46.0f) {
            width = 46.0f;
        } else if (width > 220.0f) {
            width = 220.0f;
        }

        mid_x = ((layouts[from_index].rect.x + (layouts[from_index].rect.width * 0.5f)) +
                 (layouts[to_index].rect.x + (layouts[to_index].rect.width * 0.5f))) * 0.5f;
        mid_y = ((layouts[from_index].rect.y + layouts[from_index].rect.height) +
                 (layouts[to_index].rect.y)) * 0.5f;

        out_layouts[i].edge_index = i;
        out_layouts[i].anchor = (KitRenderVec2){mid_x, mid_y};
        out_layouts[i].rect = (KitRenderRect){
            mid_x - (width * 0.5f),
            mid_y - (label_height * 0.5f),
            width,
            label_height
        };

        for (attempt = 0; attempt <= 12; ++attempt) {
            int lane_magnitude = attempt == 0 ? 0 : ((attempt + 1) / 2);
            int lane_sign = 1;
            float lane_offset_y = 0.0f;
            KitRenderRect candidate = out_layouts[i].rect;

            if (attempt > 0 && (attempt % 2) == 0) {
                lane_sign = -1;
            }
            lane_offset_y = (float)lane_sign * ((label_height + lane_gap) * (float)lane_magnitude);
            candidate.y = (mid_y - (label_height * 0.5f)) + lane_offset_y;

            if (rect_overlaps_previous_labels(candidate, out_layouts, i)) {
                continue;
            }
            if (rect_overlaps_any_node(candidate, layouts, node_count)) {
                continue;
            }

            out_layouts[i].rect = candidate;
            placed = 1;
            break;
        }

        if (!placed) {
            /* Fall back to the base anchor rect if no clean lane was found. */
            out_layouts[i].rect = (KitRenderRect){
                mid_x - (width * 0.5f),
                mid_y - (label_height * 0.5f),
                width,
                label_height
            };
        }
    }

    return core_result_ok();
}

static void edge_boundary_points(KitRenderRect from_rect,
                                 KitRenderRect to_rect,
                                 KitRenderVec2 *out_start,
                                 KitRenderVec2 *out_end) {
    KitRenderVec2 from_center;
    KitRenderVec2 to_center;
    float dx;
    float dy;
    float adx;
    float ady;

    if (!out_start || !out_end) {
        return;
    }

    from_center.x = from_rect.x + (from_rect.width * 0.5f);
    from_center.y = from_rect.y + (from_rect.height * 0.5f);
    to_center.x = to_rect.x + (to_rect.width * 0.5f);
    to_center.y = to_rect.y + (to_rect.height * 0.5f);
    dx = to_center.x - from_center.x;
    dy = to_center.y - from_center.y;
    adx = dx < 0.0f ? -dx : dx;
    ady = dy < 0.0f ? -dy : dy;

    if (adx >= ady) {
        if (dx >= 0.0f) {
            out_start->x = from_rect.x + from_rect.width;
            out_start->y = from_center.y;
            out_end->x = to_rect.x;
            out_end->y = to_center.y;
        } else {
            out_start->x = from_rect.x;
            out_start->y = from_center.y;
            out_end->x = to_rect.x + to_rect.width;
            out_end->y = to_center.y;
        }
    } else {
        if (dy >= 0.0f) {
            out_start->x = from_center.x;
            out_start->y = from_rect.y + from_rect.height;
            out_end->x = to_center.x;
            out_end->y = to_rect.y;
        } else {
            out_start->x = from_center.x;
            out_start->y = from_rect.y;
            out_end->x = to_center.x;
            out_end->y = to_rect.y + to_rect.height;
        }
    }
}

CoreResult kit_graph_struct_compute_edge_routes(const KitGraphStructEdge *edges,
                                                uint32_t edge_count,
                                                const KitGraphStructNodeLayout *layouts,
                                                uint32_t layout_count,
                                                KitGraphStructRouteMode route_mode,
                                                KitGraphStructEdgeRoute *out_routes) {
    uint32_t i;

    if (!edges || !layouts || !out_routes) {
        return kit_graph_struct_invalid("invalid edge route input");
    }
    if (route_mode != KIT_GRAPH_STRUCT_ROUTE_STRAIGHT &&
        route_mode != KIT_GRAPH_STRUCT_ROUTE_ORTHOGONAL) {
        return kit_graph_struct_invalid("invalid route mode");
    }

    for (i = 0u; i < edge_count; ++i) {
        int from_index = find_layout_index(layouts, layout_count, edges[i].from_id);
        int to_index = find_layout_index(layouts, layout_count, edges[i].to_id);
        KitRenderVec2 start = {0.0f, 0.0f};
        KitRenderVec2 end = {0.0f, 0.0f};
        KitGraphStructEdgeRoute *route = &out_routes[i];

        route->edge_index = i;
        route->point_count = 0u;
        memset(route->points, 0, sizeof(route->points));

        if (from_index < 0 || to_index < 0) {
            continue;
        }

        edge_boundary_points(layouts[from_index].rect, layouts[to_index].rect, &start, &end);
        route->points[0] = start;

        if (route_mode == KIT_GRAPH_STRUCT_ROUTE_STRAIGHT) {
            route->points[1] = end;
            route->point_count = 2u;
            continue;
        }

        {
            float dx = end.x - start.x;
            float dy = end.y - start.y;
            float adx = dx < 0.0f ? -dx : dx;
            float ady = dy < 0.0f ? -dy : dy;

            if (adx >= ady) {
                float mid_x = start.x + (dx * 0.5f);
                route->points[1] = (KitRenderVec2){mid_x, start.y};
                route->points[2] = (KitRenderVec2){mid_x, end.y};
                route->points[3] = end;
                route->point_count = 4u;
            } else {
                float mid_y = start.y + (dy * 0.5f);
                route->points[1] = (KitRenderVec2){start.x, mid_y};
                route->points[2] = (KitRenderVec2){end.x, mid_y};
                route->points[3] = end;
                route->point_count = 4u;
            }
        }
    }

    return core_result_ok();
}

CoreResult kit_graph_struct_compute_edge_label_layouts_routed(const KitGraphStructNodeLayout *layouts,
                                                              uint32_t node_count,
                                                              const KitGraphStructEdgeRoute *routes,
                                                              uint32_t route_count,
                                                              const KitGraphStructLayoutStyle *style,
                                                              const KitGraphStructEdgeLabel *edge_labels,
                                                              uint32_t edge_label_count,
                                                              const KitGraphStructEdgeLabelOptions *options,
                                                              KitGraphStructEdgeLabelLayout *out_layouts) {
    KitGraphStructLayoutStyle local_style;
    KitGraphStructEdgeLabelOptions local_options;
    float char_width;
    float label_padding_x;
    float label_height;
    float lane_gap;
    uint32_t i;

    if (!layouts || !routes || !out_layouts) {
        return kit_graph_struct_invalid("invalid routed edge label layout input");
    }

    local_style = layout_style_or_default(style);
    if (options) {
        local_options = *options;
    } else {
        kit_graph_struct_edge_label_options_default(&local_options);
    }
    char_width = local_style.label_char_width > 0.0f ? local_style.label_char_width : 7.0f;
    label_padding_x = local_style.edge_label_padding_x > 0.0f ? local_style.edge_label_padding_x : 6.0f;
    label_height = local_style.edge_label_height > 0.0f ? local_style.edge_label_height : 16.0f;
    lane_gap = local_style.edge_label_lane_gap >= 0.0f ? local_style.edge_label_lane_gap : 4.0f;

    if (local_options.current_zoom < local_options.min_zoom_for_labels) {
        for (i = 0u; i < route_count; ++i) {
            out_layouts[i].edge_index = i;
            out_layouts[i].anchor = (KitRenderVec2){0.0f, 0.0f};
            out_layouts[i].rect = (KitRenderRect){0.0f, 0.0f, 0.0f, 0.0f};
        }
        return core_result_ok();
    }

    for (i = 0u; i < route_count; ++i) {
        const char *text = "";
        float text_width;
        float width;
        int attempt;
        int placed = 0;
        KitRenderVec2 anchor = {0.0f, 0.0f};
        int route_is_horizontal = 1;
        KitRenderRect base_rect = {0.0f, 0.0f, 0.0f, 0.0f};
        uint32_t p;
        float best_len_sq = -1.0f;

        out_layouts[i].edge_index = i;
        out_layouts[i].anchor = (KitRenderVec2){0.0f, 0.0f};
        out_layouts[i].rect = (KitRenderRect){0.0f, 0.0f, 0.0f, 0.0f};

        if (routes[i].point_count < 2u) {
            continue;
        }
        if (edge_labels && i < edge_label_count && edge_labels[i].text) {
            text = edge_labels[i].text;
        }

        text_width = (float)strlen(text) * char_width;
        width = text_width + (label_padding_x * 2.0f);
        if (width < 46.0f) {
            width = 46.0f;
        } else if (width > 220.0f) {
            width = 220.0f;
        }

        for (p = 0u; p + 1u < routes[i].point_count; ++p) {
            float dx = routes[i].points[p + 1u].x - routes[i].points[p].x;
            float dy = routes[i].points[p + 1u].y - routes[i].points[p].y;
            float len_sq = (dx * dx) + (dy * dy);
            if (len_sq > best_len_sq) {
                float adx = dx < 0.0f ? -dx : dx;
                float ady = dy < 0.0f ? -dy : dy;
                best_len_sq = len_sq;
                anchor.x = routes[i].points[p].x + (dx * 0.5f);
                anchor.y = routes[i].points[p].y + (dy * 0.5f);
                route_is_horizontal = adx >= ady ? 1 : 0;
            }
        }
        if (best_len_sq <= 0.0f) {
            continue;
        }

        out_layouts[i].anchor = anchor;
        base_rect = (KitRenderRect){
            anchor.x - (width * 0.5f),
            anchor.y - (label_height * 0.5f),
            width,
            label_height
        };

        for (attempt = 0; attempt <= 12; ++attempt) {
            int lane_magnitude = attempt == 0 ? 0 : ((attempt + 1) / 2);
            int lane_sign = 1;
            float lane_offset = 0.0f;
            KitRenderRect candidate = base_rect;

            if (attempt > 0 && (attempt % 2) == 0) {
                lane_sign = -1;
            }
            lane_offset = (float)lane_sign * ((label_height + lane_gap) * (float)lane_magnitude);
            if (route_is_horizontal) {
                candidate.y = base_rect.y + lane_offset;
            } else {
                candidate.x = base_rect.x + lane_offset;
            }

            if (rect_overlaps_any_node(candidate, layouts, node_count)) {
                continue;
            }
            if (local_options.density_mode == KIT_GRAPH_STRUCT_EDGE_LABEL_DENSITY_CULL_OVERLAP &&
                rect_overlaps_previous_labels(candidate, out_layouts, i)) {
                continue;
            }

            out_layouts[i].rect = candidate;
            placed = 1;
            break;
        }

        if (!placed) {
            if (local_options.density_mode == KIT_GRAPH_STRUCT_EDGE_LABEL_DENSITY_CULL_OVERLAP) {
                out_layouts[i].rect = (KitRenderRect){0.0f, 0.0f, 0.0f, 0.0f};
            } else {
                out_layouts[i].rect = base_rect;
            }
        }
    }

    return core_result_ok();
}

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
                                 int has_hovered) {
    uint32_t i;
    CoreResult result;

    if (!frame || !nodes || !layouts) {
        return kit_graph_struct_invalid("invalid draw input");
    }

    result = kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            bounds, 10.0f, {24, 29, 38, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return result;

    for (i = 0; i < edge_count; ++i) {
        int from_index = find_node_index(nodes, node_count, edges[i].from_id);
        int to_index = find_node_index(nodes, node_count, edges[i].to_id);
        KitRenderVec2 p0;
        KitRenderVec2 p1;
        if (from_index < 0 || to_index < 0) continue;

        p0.x = layouts[from_index].rect.x + (layouts[from_index].rect.width * 0.5f);
        p0.y = layouts[from_index].rect.y + layouts[from_index].rect.height;
        p1.x = layouts[to_index].rect.x + (layouts[to_index].rect.width * 0.5f);
        p1.y = layouts[to_index].rect.y;
        result = kit_render_push_line(
            frame,
            &(KitRenderLineCommand){
                p0, p1, 2.0f, {92, 108, 132, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
            });
        if (result.code != CORE_OK) return result;
    }

    for (i = 0; i < node_count; ++i) {
        KitRenderColor fill = {58, 68, 86, 255};
        if (has_selected && nodes[i].id == selected_node_id) {
            fill = (KitRenderColor){78, 122, 226, 255};
        } else if (has_hovered && nodes[i].id == hovered_node_id) {
            fill = (KitRenderColor){88, 94, 118, 255};
        }

        result = kit_render_push_rect(
            frame,
            &(KitRenderRectCommand){
                layouts[i].rect, 8.0f, fill, {0.0f, 0.0f, 1.0f, 1.0f}
            });
        if (result.code != CORE_OK) return result;

        if (nodes[i].label) {
            result = kit_render_push_text(
                frame,
                &(KitRenderTextCommand){
                    {layouts[i].rect.x + 10.0f, layouts[i].rect.y + (layouts[i].rect.height * 0.5f)},
                    nodes[i].label,
                    CORE_FONT_ROLE_UI_REGULAR,
                    CORE_FONT_TEXT_SIZE_CAPTION,
                    CORE_THEME_COLOR_TEXT_PRIMARY,
                    {0.0f, 0.0f, 1.0f, 1.0f}
                });
            if (result.code != CORE_OK) return result;
        }
    }

    return core_result_ok();
}
