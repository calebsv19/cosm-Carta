#include "core_pane.h"

#include <assert.h>
#include <math.h>

static int nearf(float a, float b, float eps) {
    float d = a - b;
    if (d < 0.0f) {
        d = -d;
    }
    return d <= eps;
}

static void test_solve_basic_horizontal_split(void) {
    CorePaneNode nodes[3] = {
        { CORE_PANE_NODE_SPLIT, 10u, CORE_PANE_AXIS_HORIZONTAL, 0.4f, 1u, 2u, { 0.0f, 0.0f } },
        { CORE_PANE_NODE_LEAF, 1u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } },
        { CORE_PANE_NODE_LEAF, 2u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } }
    };
    CorePaneLeafRect leaves[4] = {0};
    uint32_t leaf_count = 0u;

    assert(core_pane_solve(nodes,
                           3u,
                           0u,
                           (CorePaneRect){ 0.0f, 0.0f, 100.0f, 60.0f },
                           leaves,
                           4u,
                           &leaf_count));
    assert(leaf_count == 2u);
    assert(leaves[0].id == 1u);
    assert(leaves[1].id == 2u);
    assert(nearf(leaves[0].rect.width, 40.0f, 0.0001f));
    assert(nearf(leaves[1].rect.width, 60.0f, 0.0001f));
}

static void test_solve_respects_min_constraints(void) {
    CorePaneNode nodes[3] = {
        { CORE_PANE_NODE_SPLIT, 20u, CORE_PANE_AXIS_HORIZONTAL, 0.1f, 1u, 2u, { 30.0f, 50.0f } },
        { CORE_PANE_NODE_LEAF, 1u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } },
        { CORE_PANE_NODE_LEAF, 2u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } }
    };
    CorePaneLeafRect leaves[4] = {0};
    uint32_t leaf_count = 0u;

    assert(core_pane_solve(nodes,
                           3u,
                           0u,
                           (CorePaneRect){ 0.0f, 0.0f, 100.0f, 40.0f },
                           leaves,
                           4u,
                           &leaf_count));
    assert(leaf_count == 2u);
    assert(nearf(leaves[0].rect.width, 30.0f, 0.0001f));
    assert(nearf(leaves[1].rect.width, 70.0f, 0.0001f));
}

static void test_hit_and_drag_splitter(void) {
    CorePaneNode nodes[3] = {
        { CORE_PANE_NODE_SPLIT, 30u, CORE_PANE_AXIS_HORIZONTAL, 0.5f, 1u, 2u, { 20.0f, 20.0f } },
        { CORE_PANE_NODE_LEAF, 1u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } },
        { CORE_PANE_NODE_LEAF, 2u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } }
    };
    CorePaneSplitterHit hit = {0};

    assert(core_pane_hit_test_splitter(nodes,
                                       3u,
                                       0u,
                                       (CorePaneRect){ 0.0f, 0.0f, 200.0f, 120.0f },
                                       10.0f,
                                       100.0f,
                                       30.0f,
                                       &hit));
    assert(hit.active);
    assert(hit.node_index == 0u);
    assert(hit.axis == CORE_PANE_AXIS_HORIZONTAL);

    assert(core_pane_apply_splitter_drag(nodes, 3u, &hit, 50.0f, 0.0f));
    assert(nearf(nodes[0].ratio_01, 0.75f, 0.0001f));

    assert(core_pane_apply_splitter_drag(nodes, 3u, &hit, -400.0f, 0.0f));
    assert(nearf(nodes[0].ratio_01, 0.1f, 0.0001f));
}

static void test_solve_rejects_cycle_graph(void) {
    CorePaneNode nodes[2] = {
        { CORE_PANE_NODE_SPLIT, 11u, CORE_PANE_AXIS_HORIZONTAL, 0.5f, 0u, 1u, { 0.0f, 0.0f } },
        { CORE_PANE_NODE_LEAF, 2u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } }
    };
    CorePaneLeafRect leaves[4] = {0};
    uint32_t leaf_count = 0u;

    assert(!core_pane_solve(nodes,
                            2u,
                            0u,
                            (CorePaneRect){ 0.0f, 0.0f, 120.0f, 80.0f },
                            leaves,
                            4u,
                            &leaf_count));
}

static void test_solve_rejects_duplicate_child_reference(void) {
    CorePaneNode nodes[2] = {
        { CORE_PANE_NODE_SPLIT, 12u, CORE_PANE_AXIS_HORIZONTAL, 0.5f, 1u, 1u, { 0.0f, 0.0f } },
        { CORE_PANE_NODE_LEAF, 3u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } }
    };
    CorePaneLeafRect leaves[4] = {0};
    uint32_t leaf_count = 0u;

    assert(!core_pane_solve(nodes,
                            2u,
                            0u,
                            (CorePaneRect){ 0.0f, 0.0f, 120.0f, 80.0f },
                            leaves,
                            4u,
                            &leaf_count));
}

static void test_solve_handles_non_finite_ratio_with_stable_fallback(void) {
    CorePaneNode nodes[3] = {
        { CORE_PANE_NODE_SPLIT, 13u, CORE_PANE_AXIS_HORIZONTAL, NAN, 1u, 2u, { NAN, INFINITY } },
        { CORE_PANE_NODE_LEAF, 4u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } },
        { CORE_PANE_NODE_LEAF, 5u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } }
    };
    CorePaneLeafRect leaves[4] = {0};
    uint32_t leaf_count = 0u;

    assert(core_pane_solve(nodes,
                           3u,
                           0u,
                           (CorePaneRect){ 0.0f, 0.0f, 100.0f, 60.0f },
                           leaves,
                           4u,
                           &leaf_count));
    assert(leaf_count == 2u);
    assert(nearf(leaves[0].rect.width, 50.0f, 0.0001f));
    assert(nearf(leaves[1].rect.width, 50.0f, 0.0001f));
}

static void test_drag_sequence_is_deterministic(void) {
    CorePaneNode nodes[3] = {
        { CORE_PANE_NODE_SPLIT, 30u, CORE_PANE_AXIS_HORIZONTAL, 0.5f, 1u, 2u, { 20.0f, 20.0f } },
        { CORE_PANE_NODE_LEAF, 1u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } },
        { CORE_PANE_NODE_LEAF, 2u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } }
    };
    CorePaneSplitterHit hit = {0};

    hit.active = true;
    hit.node_index = 0u;
    hit.axis = CORE_PANE_AXIS_HORIZONTAL;
    hit.parent_span = 200.0f;
    hit.min_ratio_01 = 0.1f;
    hit.max_ratio_01 = 0.9f;

    assert(core_pane_apply_splitter_drag(nodes, 3u, &hit, 10.0f, 0.0f));
    assert(nearf(nodes[0].ratio_01, 0.55f, 0.0001f));

    assert(core_pane_apply_splitter_drag(nodes, 3u, &hit, 10.0f, 0.0f));
    assert(nearf(nodes[0].ratio_01, 0.60f, 0.0001f));

    assert(core_pane_apply_splitter_drag(nodes, 3u, &hit, -20.0f, 0.0f));
    assert(nearf(nodes[0].ratio_01, 0.50f, 0.0001f));
}

int main(void) {
    test_solve_basic_horizontal_split();
    test_solve_respects_min_constraints();
    test_hit_and_drag_splitter();
    test_solve_rejects_cycle_graph();
    test_solve_rejects_duplicate_child_reference();
    test_solve_handles_non_finite_ratio_with_stable_fallback();
    test_drag_sequence_is_deterministic();
    return 0;
}
