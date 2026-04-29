/*
 * core_pane.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_pane.h"

#include <math.h>
#include <stdlib.h>

static float pane_clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int pane_isfinite(float value) {
    return isfinite(value) ? 1 : 0;
}

static float non_negative(float value) {
    if (!pane_isfinite(value)) {
        return 0.0f;
    }
    if (value < 0.0f) {
        return 0.0f;
    }
    return value;
}

static bool rect_contains(CorePaneRect rect, float x, float y) {
    if (!pane_isfinite(rect.x) || !pane_isfinite(rect.y) ||
        !pane_isfinite(rect.width) || !pane_isfinite(rect.height) ||
        !pane_isfinite(x) || !pane_isfinite(y)) {
        return false;
    }
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return false;
    }
    if (x < rect.x || y < rect.y) return false;
    if (x > rect.x + rect.width) return false;
    if (y > rect.y + rect.height) return false;
    return true;
}

float core_pane_clamp_ratio(float ratio_01, float min_ratio_01, float max_ratio_01) {
    float min_r = pane_isfinite(min_ratio_01) ? pane_clampf(min_ratio_01, 0.0f, 1.0f) : 0.0f;
    float max_r = pane_isfinite(max_ratio_01) ? pane_clampf(max_ratio_01, 0.0f, 1.0f) : 1.0f;
    float ratio = ratio_01;

    if (max_r < min_r) {
        float tmp = min_r;
        min_r = max_r;
        max_r = tmp;
    }

    if (!pane_isfinite(ratio)) {
        ratio = (min_r + max_r) * 0.5f;
    }

    return pane_clampf(ratio, min_r, max_r);
}

static bool split_bounds_from_node(const CorePaneNode *node,
                                   CorePaneRect parent,
                                   float handle_thickness,
                                   float *out_ratio,
                                   float *out_min_ratio,
                                   float *out_max_ratio,
                                   CorePaneRect *out_a,
                                   CorePaneRect *out_b,
                                   CorePaneRect *out_splitter) {
    float span;
    float min_a;
    float min_b;
    float min_ratio;
    float max_ratio;
    float ratio;
    float split_coord;
    float split_size = handle_thickness;

    if (!node || !out_ratio || !out_min_ratio || !out_max_ratio || !out_a || !out_b || !out_splitter) {
        return false;
    }
    if (node->type != CORE_PANE_NODE_SPLIT) {
        return false;
    }
    if (!pane_isfinite(parent.x) || !pane_isfinite(parent.y) ||
        !pane_isfinite(parent.width) || !pane_isfinite(parent.height)) {
        return false;
    }
    if (parent.width < 0.0f || parent.height < 0.0f) {
        return false;
    }

    span = (node->axis == CORE_PANE_AXIS_HORIZONTAL) ? parent.width : parent.height;
    if (!pane_isfinite(span) || span <= 0.0f) {
        return false;
    }

    min_a = non_negative(node->constraints.min_size_a);
    min_b = non_negative(node->constraints.min_size_b);
    if ((min_a + min_b) > span && (min_a + min_b) > 0.0f) {
        float scale = span / (min_a + min_b);
        min_a *= scale;
        min_b *= scale;
    }

    min_ratio = min_a / span;
    max_ratio = 1.0f - (min_b / span);
    ratio = core_pane_clamp_ratio(node->ratio_01, min_ratio, max_ratio);
    split_coord = (node->axis == CORE_PANE_AXIS_HORIZONTAL)
                      ? (parent.x + (span * ratio))
                      : (parent.y + (span * ratio));

    if (split_size <= 0.0f) {
        split_size = 1.0f;
    }

    if (node->axis == CORE_PANE_AXIS_HORIZONTAL) {
        *out_a = (CorePaneRect){ parent.x, parent.y, split_coord - parent.x, parent.height };
        *out_b = (CorePaneRect){
            split_coord,
            parent.y,
            (parent.x + parent.width) - split_coord,
            parent.height
        };
        *out_splitter = (CorePaneRect){
            split_coord - (split_size * 0.5f),
            parent.y,
            split_size,
            parent.height
        };
    } else {
        *out_a = (CorePaneRect){ parent.x, parent.y, parent.width, split_coord - parent.y };
        *out_b = (CorePaneRect){
            parent.x,
            split_coord,
            parent.width,
            (parent.y + parent.height) - split_coord
        };
        *out_splitter = (CorePaneRect){
            parent.x,
            split_coord - (split_size * 0.5f),
            parent.width,
            split_size
        };
    }

    *out_ratio = ratio;
    *out_min_ratio = min_ratio;
    *out_max_ratio = max_ratio;
    return true;
}

typedef enum PaneVisitState {
    PANE_VISIT_UNVISITED = 0,
    PANE_VISIT_VISITING = 1,
    PANE_VISIT_VISITED = 2
} PaneVisitState;

static void pane_set_report(CorePaneValidationReport *out_report,
                            CorePaneValidationCode code,
                            uint32_t node_index,
                            uint32_t related_index) {
    if (!out_report) {
        return;
    }
    out_report->code = code;
    out_report->node_index = node_index;
    out_report->related_index = related_index;
}

static bool pane_bounds_valid(CorePaneRect bounds) {
    return pane_isfinite(bounds.x) &&
           pane_isfinite(bounds.y) &&
           pane_isfinite(bounds.width) &&
           pane_isfinite(bounds.height) &&
           bounds.width >= 0.0f &&
           bounds.height >= 0.0f;
}

static bool pane_validate_recursive(const CorePaneNode *nodes,
                                    uint32_t node_count,
                                    uint32_t node_index,
                                    CorePaneRect bounds,
                                    uint8_t *visit_state,
                                    CorePaneValidationReport *out_report) {
    const CorePaneNode *node;
    float ratio = 0.0f;
    float min_ratio = 0.0f;
    float max_ratio = 0.0f;
    CorePaneRect child_a = {0};
    CorePaneRect child_b = {0};
    CorePaneRect split_rect = {0};

    if (!nodes || !visit_state || node_index >= node_count) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_NODE_INDEX_OUT_OF_RANGE, node_index, UINT32_MAX);
        return false;
    }
    if (visit_state[node_index] == PANE_VISIT_VISITING) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_CYCLE_DETECTED, node_index, UINT32_MAX);
        return false;
    }
    if (visit_state[node_index] == PANE_VISIT_VISITED) {
        return true;
    }

    visit_state[node_index] = PANE_VISIT_VISITING;
    node = &nodes[node_index];

    if (node->type == CORE_PANE_NODE_LEAF) {
        visit_state[node_index] = PANE_VISIT_VISITED;
        return true;
    }

    if (node->child_a == node->child_b) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_DUPLICATE_CHILD, node_index, node->child_a);
        return false;
    }
    if (node->child_a == node_index || node->child_b == node_index) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_SELF_CHILD, node_index, node_index);
        return false;
    }
    if (node->child_a >= node_count || node->child_b >= node_count) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_NODE_INDEX_OUT_OF_RANGE, node_index, UINT32_MAX);
        return false;
    }
    if (!split_bounds_from_node(node,
                                bounds,
                                1.0f,
                                &ratio,
                                &min_ratio,
                                &max_ratio,
                                &child_a,
                                &child_b,
                                &split_rect)) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_SPLIT_BOUNDS_INVALID, node_index, UINT32_MAX);
        return false;
    }

    (void)ratio;
    (void)min_ratio;
    (void)max_ratio;
    (void)split_rect;

    if (!pane_validate_recursive(nodes, node_count, node->child_a, child_a, visit_state, out_report)) {
        return false;
    }
    if (!pane_validate_recursive(nodes, node_count, node->child_b, child_b, visit_state, out_report)) {
        return false;
    }

    visit_state[node_index] = PANE_VISIT_VISITED;
    return true;
}

bool core_pane_validate_graph(const CorePaneNode *nodes,
                              uint32_t node_count,
                              uint32_t root_index,
                              CorePaneRect bounds,
                              CorePaneValidationReport *out_report) {
    uint8_t *visit_state = NULL;
    uint32_t i;
    bool ok;

    pane_set_report(out_report, CORE_PANE_VALIDATION_OK, UINT32_MAX, UINT32_MAX);

    if (!nodes) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_INVALID_ARG, UINT32_MAX, UINT32_MAX);
        return false;
    }
    if (node_count == 0u) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_EMPTY_GRAPH, UINT32_MAX, UINT32_MAX);
        return false;
    }
    if (root_index >= node_count) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_INVALID_ROOT, root_index, UINT32_MAX);
        return false;
    }
    if (!pane_bounds_valid(bounds)) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_INVALID_BOUNDS, UINT32_MAX, UINT32_MAX);
        return false;
    }

    visit_state = (uint8_t *)calloc((size_t)node_count, sizeof(uint8_t));
    if (!visit_state) {
        pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_INVALID_ARG, UINT32_MAX, UINT32_MAX);
        return false;
    }

    ok = pane_validate_recursive(nodes, node_count, root_index, bounds, visit_state, out_report);
    if (ok) {
        for (i = 0u; i < node_count; ++i) {
            if (visit_state[i] != PANE_VISIT_VISITED) {
                pane_set_report(out_report, CORE_PANE_VALIDATION_ERR_CYCLE_DETECTED, i, UINT32_MAX);
                ok = false;
                break;
            }
        }
    }
    free(visit_state);
    return ok;
}

const char *core_pane_validation_code_string(CorePaneValidationCode code) {
    switch (code) {
        case CORE_PANE_VALIDATION_OK:
            return "ok";
        case CORE_PANE_VALIDATION_ERR_INVALID_ARG:
            return "invalid_arg";
        case CORE_PANE_VALIDATION_ERR_EMPTY_GRAPH:
            return "empty_graph";
        case CORE_PANE_VALIDATION_ERR_INVALID_ROOT:
            return "invalid_root";
        case CORE_PANE_VALIDATION_ERR_INVALID_BOUNDS:
            return "invalid_bounds";
        case CORE_PANE_VALIDATION_ERR_NODE_INDEX_OUT_OF_RANGE:
            return "node_index_out_of_range";
        case CORE_PANE_VALIDATION_ERR_CYCLE_DETECTED:
            return "cycle_detected";
        case CORE_PANE_VALIDATION_ERR_DUPLICATE_CHILD:
            return "duplicate_child";
        case CORE_PANE_VALIDATION_ERR_SELF_CHILD:
            return "self_child";
        case CORE_PANE_VALIDATION_ERR_SPLIT_BOUNDS_INVALID:
            return "split_bounds_invalid";
        case CORE_PANE_VALIDATION_ERR_OUTPUT_CAPACITY:
            return "output_capacity";
        default:
            return "unknown";
    }
}

static bool solve_node_recursive(const CorePaneNode *nodes,
                                 uint32_t node_count,
                                 uint32_t node_index,
                                 CorePaneRect bounds,
                                 CorePaneLeafRect *out_leaf_rects,
                                 uint32_t out_leaf_cap,
                                 uint32_t *io_leaf_count,
                                 uint8_t *visit_state) {
    const CorePaneNode *node;
    float ratio;
    float min_ratio;
    float max_ratio;
    CorePaneRect child_a;
    CorePaneRect child_b;
    CorePaneRect split_rect;

    if (!nodes || !io_leaf_count || !visit_state || node_index >= node_count) {
        return false;
    }
    if (visit_state[node_index] == PANE_VISIT_VISITING || visit_state[node_index] == PANE_VISIT_VISITED) {
        return false;
    }
    visit_state[node_index] = PANE_VISIT_VISITING;

    node = &nodes[node_index];

    if (node->type == CORE_PANE_NODE_LEAF) {
        if (!out_leaf_rects || *io_leaf_count >= out_leaf_cap) {
            return false;
        }
        out_leaf_rects[*io_leaf_count].id = node->id;
        out_leaf_rects[*io_leaf_count].rect = bounds;
        *io_leaf_count += 1u;
        visit_state[node_index] = PANE_VISIT_VISITED;
        return true;
    }

    if (node->child_a >= node_count || node->child_b >= node_count ||
        node->child_a == node->child_b ||
        node->child_a == node_index ||
        node->child_b == node_index) {
        return false;
    }

    if (!split_bounds_from_node(node,
                                bounds,
                                1.0f,
                                &ratio,
                                &min_ratio,
                                &max_ratio,
                                &child_a,
                                &child_b,
                                &split_rect)) {
        return false;
    }

    (void)ratio;
    (void)min_ratio;
    (void)max_ratio;
    (void)split_rect;

    if (!solve_node_recursive(nodes,
                              node_count,
                              node->child_a,
                              child_a,
                              out_leaf_rects,
                              out_leaf_cap,
                              io_leaf_count,
                              visit_state)) {
        return false;
    }
    if (!solve_node_recursive(nodes,
                              node_count,
                              node->child_b,
                              child_b,
                              out_leaf_rects,
                              out_leaf_cap,
                              io_leaf_count,
                              visit_state)) {
        return false;
    }
    visit_state[node_index] = PANE_VISIT_VISITED;
    return true;
}

bool core_pane_solve(const CorePaneNode *nodes,
                     uint32_t node_count,
    uint32_t root_index,
    CorePaneRect bounds,
    CorePaneLeafRect *out_leaf_rects,
    uint32_t out_leaf_cap,
    uint32_t *out_leaf_count) {
    uint32_t count = 0u;
    uint8_t *visit_state = 0;
    bool ok = false;

    if (!out_leaf_rects || out_leaf_cap == 0u || !out_leaf_count) {
        return false;
    }
    if (!core_pane_validate_graph(nodes, node_count, root_index, bounds, 0)) {
        return false;
    }

    visit_state = (uint8_t *)calloc((size_t)node_count, sizeof(uint8_t));
    if (!visit_state) {
        return false;
    }

    ok = solve_node_recursive(nodes,
                              node_count,
                              root_index,
                              bounds,
                              out_leaf_rects,
                              out_leaf_cap,
                              &count,
                              visit_state);
    free(visit_state);
    if (!ok) {
        return false;
    }

    *out_leaf_count = count;
    return true;
}

static bool hit_test_splitter_recursive(const CorePaneNode *nodes,
                                        uint32_t node_count,
                                        uint32_t node_index,
                                        CorePaneRect bounds,
                                        float handle_thickness,
                                        float point_x,
                                        float point_y,
                                        CorePaneSplitterHit *out_hit,
                                        uint8_t *visit_state) {
    const CorePaneNode *node;
    float ratio;
    float min_ratio;
    float max_ratio;
    CorePaneRect child_a;
    CorePaneRect child_b;
    CorePaneRect split_rect;

    if (!nodes || !out_hit || !visit_state || node_index >= node_count) {
        return false;
    }
    if (visit_state[node_index] == PANE_VISIT_VISITING || visit_state[node_index] == PANE_VISIT_VISITED) {
        return false;
    }
    visit_state[node_index] = PANE_VISIT_VISITING;

    node = &nodes[node_index];
    if (node->type != CORE_PANE_NODE_SPLIT) {
        return false;
    }

    if (!split_bounds_from_node(node,
                                bounds,
                                handle_thickness,
                                &ratio,
                                &min_ratio,
                                &max_ratio,
                                &child_a,
                                &child_b,
                                &split_rect)) {
        return false;
    }

    if (node->child_a < node_count &&
        hit_test_splitter_recursive(nodes,
                                    node_count,
                                    node->child_a,
                                    child_a,
                                    handle_thickness,
                                    point_x,
                                    point_y,
                                    out_hit,
                                    visit_state)) {
        return true;
    }

    if (node->child_b < node_count &&
        hit_test_splitter_recursive(nodes,
                                    node_count,
                                    node->child_b,
                                    child_b,
                                    handle_thickness,
                                    point_x,
                                    point_y,
                                    out_hit,
                                    visit_state)) {
        return true;
    }

    if (rect_contains(split_rect, point_x, point_y)) {
        out_hit->active = true;
        out_hit->node_index = node_index;
        out_hit->axis = node->axis;
        out_hit->splitter_bounds = split_rect;
        out_hit->ratio_01 = ratio;
        out_hit->parent_span = (node->axis == CORE_PANE_AXIS_HORIZONTAL) ? bounds.width : bounds.height;
        out_hit->min_ratio_01 = min_ratio;
        out_hit->max_ratio_01 = max_ratio;
        return true;
    }

    visit_state[node_index] = PANE_VISIT_VISITED;
    return false;
}

bool core_pane_hit_test_splitter(const CorePaneNode *nodes,
                                 uint32_t node_count,
                                 uint32_t root_index,
                                 CorePaneRect bounds,
                                 float handle_thickness,
                                 float point_x,
                                 float point_y,
                                 CorePaneSplitterHit *out_hit) {
    uint8_t *visit_state = 0;
    bool hit = false;

    if (!out_hit) {
        return false;
    }
    *out_hit = (CorePaneSplitterHit){ 0 };

    if (!core_pane_validate_graph(nodes, node_count, root_index, bounds, 0)) {
        return false;
    }
    if (!pane_isfinite(point_x) || !pane_isfinite(point_y)) {
        return false;
    }
    if (handle_thickness <= 0.0f) {
        handle_thickness = 1.0f;
    }

    visit_state = (uint8_t *)calloc((size_t)node_count, sizeof(uint8_t));
    if (!visit_state) {
        return false;
    }

    hit = hit_test_splitter_recursive(nodes,
                                      node_count,
                                      root_index,
                                      bounds,
                                      handle_thickness,
                                      point_x,
                                      point_y,
                                      out_hit,
                                      visit_state);
    free(visit_state);
    return hit;
}

bool core_pane_apply_splitter_drag(CorePaneNode *nodes,
                                   uint32_t node_count,
                                   const CorePaneSplitterHit *hit,
                                   float delta_x,
                                   float delta_y) {
    CorePaneNode *node;
    float delta;
    float ratio;
    float min_ratio;
    float max_ratio;

    if (!nodes || !hit || !hit->active || hit->node_index >= node_count) {
        return false;
    }
    node = &nodes[hit->node_index];
    if (node->type != CORE_PANE_NODE_SPLIT) {
        return false;
    }

    if (hit->parent_span <= 0.0f) {
        return false;
    }
    if (!pane_isfinite(delta_x) || !pane_isfinite(delta_y) || !pane_isfinite(node->ratio_01)) {
        return false;
    }

    delta = (hit->axis == CORE_PANE_AXIS_HORIZONTAL) ? delta_x : delta_y;
    min_ratio = pane_clampf(hit->min_ratio_01, 0.0f, 1.0f);
    max_ratio = pane_clampf(hit->max_ratio_01, 0.0f, 1.0f);
    ratio = core_pane_clamp_ratio(node->ratio_01 + (delta / hit->parent_span), min_ratio, max_ratio);
    if (ratio == node->ratio_01) {
        return false;
    }
    node->ratio_01 = ratio;
    return true;
}
