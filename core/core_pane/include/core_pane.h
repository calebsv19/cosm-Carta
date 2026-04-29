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

typedef enum CorePaneValidationCode {
    CORE_PANE_VALIDATION_OK = 0,
    CORE_PANE_VALIDATION_ERR_INVALID_ARG = 1,
    CORE_PANE_VALIDATION_ERR_EMPTY_GRAPH = 2,
    CORE_PANE_VALIDATION_ERR_INVALID_ROOT = 3,
    CORE_PANE_VALIDATION_ERR_INVALID_BOUNDS = 4,
    CORE_PANE_VALIDATION_ERR_NODE_INDEX_OUT_OF_RANGE = 5,
    CORE_PANE_VALIDATION_ERR_CYCLE_DETECTED = 6,
    CORE_PANE_VALIDATION_ERR_DUPLICATE_CHILD = 7,
    CORE_PANE_VALIDATION_ERR_SELF_CHILD = 8,
    CORE_PANE_VALIDATION_ERR_SPLIT_BOUNDS_INVALID = 9,
    CORE_PANE_VALIDATION_ERR_OUTPUT_CAPACITY = 10
} CorePaneValidationCode;

typedef struct CorePaneValidationReport {
    CorePaneValidationCode code;
    uint32_t node_index;
    uint32_t related_index;
} CorePaneValidationReport;

float core_pane_clamp_ratio(float ratio_01, float min_ratio_01, float max_ratio_01);
bool core_pane_validate_graph(const CorePaneNode *nodes,
                              uint32_t node_count,
                              uint32_t root_index,
                              CorePaneRect bounds,
                              CorePaneValidationReport *out_report);
const char *core_pane_validation_code_string(CorePaneValidationCode code);

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
