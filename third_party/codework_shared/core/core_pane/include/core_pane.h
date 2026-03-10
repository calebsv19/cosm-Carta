#ifndef CORE_PANE_H
#define CORE_PANE_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t CorePaneId;

typedef struct CorePaneRect {
    float x;
    float y;
    float width;
    float height;
} CorePaneRect;

typedef enum CorePaneAxis {
    CORE_PANE_AXIS_HORIZONTAL = 0,
    CORE_PANE_AXIS_VERTICAL = 1
} CorePaneAxis;

typedef enum CorePaneNodeType {
    CORE_PANE_NODE_LEAF = 0,
    CORE_PANE_NODE_SPLIT = 1
} CorePaneNodeType;

typedef struct CorePaneConstraints {
    float min_size_a;
    float min_size_b;
} CorePaneConstraints;

typedef struct CorePaneNode {
    CorePaneNodeType type;
    CorePaneId id;
    CorePaneAxis axis;
    float ratio_01;
    uint32_t child_a;
    uint32_t child_b;
    CorePaneConstraints constraints;
} CorePaneNode;

typedef struct CorePaneLeafRect {
    CorePaneId id;
    CorePaneRect rect;
} CorePaneLeafRect;

typedef struct CorePaneSplitterHit {
    bool active;
    uint32_t node_index;
    CorePaneAxis axis;
    CorePaneRect splitter_bounds;
    float ratio_01;
    float parent_span;
    float min_ratio_01;
    float max_ratio_01;
} CorePaneSplitterHit;

float core_pane_clamp_ratio(float ratio_01, float min_ratio_01, float max_ratio_01);

bool core_pane_solve(const CorePaneNode *nodes,
                     uint32_t node_count,
                     uint32_t root_index,
                     CorePaneRect bounds,
                     CorePaneLeafRect *out_leaf_rects,
                     uint32_t out_leaf_cap,
                     uint32_t *out_leaf_count);

bool core_pane_hit_test_splitter(const CorePaneNode *nodes,
                                 uint32_t node_count,
                                 uint32_t root_index,
                                 CorePaneRect bounds,
                                 float handle_thickness,
                                 float point_x,
                                 float point_y,
                                 CorePaneSplitterHit *out_hit);

bool core_pane_apply_splitter_drag(CorePaneNode *nodes,
                                   uint32_t node_count,
                                   const CorePaneSplitterHit *hit,
                                   float delta_x,
                                   float delta_y);

#endif
