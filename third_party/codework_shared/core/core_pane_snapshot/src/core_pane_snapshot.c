#include "core_pane_snapshot.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    CORE_PANE_SNAPSHOT_STATE_UNVISITED = 0,
    CORE_PANE_SNAPSHOT_STATE_VISITING = 1,
    CORE_PANE_SNAPSHOT_STATE_VISITED = 2
};

static int is_finite(float value) {
    return isfinite((double)value) ? 1 : 0;
}

static int is_leaf_field_shape_valid(const CorePaneSnapshotNodeRecordV1 *node) {
    if (!node) {
        return 0;
    }
    if (node->axis != CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL) {
        return 0;
    }
    if (node->ratio_01 != 0.0f) {
        return 0;
    }
    if (node->child_a_index != UINT32_MAX || node->child_b_index != UINT32_MAX) {
        return 0;
    }
    if (node->min_size_a != 0.0f || node->min_size_b != 0.0f) {
        return 0;
    }
    return 1;
}

static int is_split_field_shape_valid(const CorePaneSnapshotNodeRecordV1 *node) {
    if (!node) {
        return 0;
    }
    if (node->axis != CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL &&
        node->axis != CORE_PANE_SNAPSHOT_AXIS_VERTICAL) {
        return 0;
    }
    if (!is_finite(node->ratio_01) || node->ratio_01 < 0.0f || node->ratio_01 > 1.0f) {
        return 0;
    }
    if (!is_finite(node->min_size_a) || !is_finite(node->min_size_b) ||
        node->min_size_a < 0.0f || node->min_size_b < 0.0f) {
        return 0;
    }
    return 1;
}

static CorePaneSnapshotResult dfs_validate(uint32_t node_index,
                                           const CorePaneSnapshotNodeRecordV1 *nodes,
                                           const uint32_t *index_to_position,
                                           uint8_t *visit_state,
                                           uint32_t node_count) {
    const CorePaneSnapshotNodeRecordV1 *node;
    uint32_t node_position;
    CorePaneSnapshotResult result;

    if (node_index >= node_count) {
        return CORE_PANE_SNAPSHOT_ERR_INVALID_CHILD_REF;
    }

    node_position = index_to_position[node_index];
    if (node_position == UINT32_MAX) {
        return CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_INDEX;
    }

    if (visit_state[node_index] == CORE_PANE_SNAPSHOT_STATE_VISITING) {
        return CORE_PANE_SNAPSHOT_ERR_CYCLE_DETECTED;
    }
    if (visit_state[node_index] == CORE_PANE_SNAPSHOT_STATE_VISITED) {
        return CORE_PANE_SNAPSHOT_OK;
    }

    visit_state[node_index] = CORE_PANE_SNAPSHOT_STATE_VISITING;
    node = &nodes[node_position];

    if (node->node_type == CORE_PANE_SNAPSHOT_NODE_SPLIT) {
        if (node->child_a_index >= node_count || node->child_b_index >= node_count ||
            node->child_a_index == node->child_b_index ||
            node->child_a_index == node->node_index ||
            node->child_b_index == node->node_index) {
            return CORE_PANE_SNAPSHOT_ERR_INVALID_CHILD_REF;
        }

        result = dfs_validate(node->child_a_index, nodes, index_to_position, visit_state, node_count);
        if (result != CORE_PANE_SNAPSHOT_OK) {
            return result;
        }
        result = dfs_validate(node->child_b_index, nodes, index_to_position, visit_state, node_count);
        if (result != CORE_PANE_SNAPSHOT_OK) {
            return result;
        }
    }

    visit_state[node_index] = CORE_PANE_SNAPSHOT_STATE_VISITED;
    return CORE_PANE_SNAPSHOT_OK;
}

static int pane_is_leaf_id(const CorePaneSnapshotNodeRecordV1 *nodes,
                           uint32_t node_count,
                           uint32_t pane_node_id) {
    uint32_t i;
    for (i = 0u; i < node_count; ++i) {
        if (nodes[i].node_id == pane_node_id &&
            nodes[i].node_type == CORE_PANE_SNAPSHOT_NODE_LEAF) {
            return 1;
        }
    }
    return 0;
}

CorePaneSnapshotResult core_pane_snapshot_validate_v1(const CorePaneSnapshotV1 *snapshot) {
    uint32_t i;
    uint32_t j;
    uint8_t *seen_indices = NULL;
    uint8_t *visit_state = NULL;
    uint32_t *index_to_position = NULL;
    CorePaneSnapshotResult result = CORE_PANE_SNAPSHOT_OK;

    if (!snapshot) {
        return CORE_PANE_SNAPSHOT_ERR_INVALID_ARG;
    }
    if (snapshot->meta.schema_major != CORE_PANE_SNAPSHOT_SCHEMA_MAJOR_V1 ||
        snapshot->meta.schema_minor != CORE_PANE_SNAPSHOT_SCHEMA_MINOR_V1) {
        return CORE_PANE_SNAPSHOT_ERR_UNSUPPORTED_SCHEMA;
    }
    if (snapshot->meta.flags != 0u || snapshot->meta.reserved0 != 0u) {
        return CORE_PANE_SNAPSHOT_ERR_INVALID_META;
    }
    if (snapshot->meta.node_count == 0u ||
        snapshot->meta.root_node_index >= snapshot->meta.node_count ||
        snapshot->nodes == NULL) {
        return CORE_PANE_SNAPSHOT_ERR_INVALID_META;
    }
    if (snapshot->meta.module_binding_count > 0u && snapshot->module_bindings == NULL) {
        return CORE_PANE_SNAPSHOT_ERR_INVALID_META;
    }

    seen_indices = (uint8_t *)calloc(snapshot->meta.node_count, sizeof(*seen_indices));
    visit_state = (uint8_t *)calloc(snapshot->meta.node_count, sizeof(*visit_state));
    index_to_position = (uint32_t *)malloc(snapshot->meta.node_count * sizeof(*index_to_position));
    if (!seen_indices || !visit_state || !index_to_position) {
        free(seen_indices);
        free(visit_state);
        free(index_to_position);
        return CORE_PANE_SNAPSHOT_ERR_INVALID_META;
    }
    for (i = 0u; i < snapshot->meta.node_count; ++i) {
        index_to_position[i] = UINT32_MAX;
    }

    for (i = 0u; i < snapshot->meta.node_count; ++i) {
        const CorePaneSnapshotNodeRecordV1 *node = &snapshot->nodes[i];

        if (node->node_index >= snapshot->meta.node_count) {
            result = CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_INDEX;
            goto done;
        }
        if (seen_indices[node->node_index]) {
            result = CORE_PANE_SNAPSHOT_ERR_DUP_NODE_INDEX;
            goto done;
        }
        seen_indices[node->node_index] = 1u;
        index_to_position[node->node_index] = i;

        if (node->node_id == 0u) {
            result = CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_ID;
            goto done;
        }
        for (j = 0u; j < i; ++j) {
            if (snapshot->nodes[j].node_id == node->node_id) {
                result = CORE_PANE_SNAPSHOT_ERR_DUP_NODE_ID;
                goto done;
            }
        }

        if (node->node_type == CORE_PANE_SNAPSHOT_NODE_LEAF) {
            if (!is_leaf_field_shape_valid(node)) {
                result = CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS;
                goto done;
            }
        } else if (node->node_type == CORE_PANE_SNAPSHOT_NODE_SPLIT) {
            if (!is_split_field_shape_valid(node)) {
                result = CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS;
                goto done;
            }
        } else {
            result = CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS;
            goto done;
        }
    }

    for (i = 0u; i < snapshot->meta.node_count; ++i) {
        if (!seen_indices[i]) {
            result = CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_INDEX;
            goto done;
        }
    }

    result = dfs_validate(snapshot->meta.root_node_index,
                          snapshot->nodes,
                          index_to_position,
                          visit_state,
                          snapshot->meta.node_count);
    if (result != CORE_PANE_SNAPSHOT_OK) {
        goto done;
    }

    for (i = 0u; i < snapshot->meta.node_count; ++i) {
        if (visit_state[i] != CORE_PANE_SNAPSHOT_STATE_VISITED) {
            result = CORE_PANE_SNAPSHOT_ERR_DISCONNECTED_GRAPH;
            goto done;
        }
    }

    for (i = 0u; i < snapshot->meta.module_binding_count; ++i) {
        const CorePaneSnapshotModuleBindingRecordV1 *binding = &snapshot->module_bindings[i];

        if (binding->instance_id == 0u || binding->pane_node_id == 0u || binding->module_type_id == 0u) {
            result = CORE_PANE_SNAPSHOT_ERR_INVALID_BINDING;
            goto done;
        }
        if (!pane_is_leaf_id(snapshot->nodes, snapshot->meta.node_count, binding->pane_node_id)) {
            result = CORE_PANE_SNAPSHOT_ERR_BINDING_PANE_NOT_LEAF;
            goto done;
        }

        for (j = i + 1u; j < snapshot->meta.module_binding_count; ++j) {
            if (snapshot->module_bindings[j].instance_id == binding->instance_id) {
                result = CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_INSTANCE;
                goto done;
            }
            if (snapshot->module_bindings[j].pane_node_id == binding->pane_node_id) {
                result = CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_PANE;
                goto done;
            }
        }
    }

done:
    free(seen_indices);
    free(visit_state);
    free(index_to_position);
    return result;
}

const char *core_pane_snapshot_result_string(CorePaneSnapshotResult result) {
    switch (result) {
        case CORE_PANE_SNAPSHOT_OK:
            return "ok";
        case CORE_PANE_SNAPSHOT_ERR_INVALID_ARG:
            return "invalid_arg";
        case CORE_PANE_SNAPSHOT_ERR_UNSUPPORTED_SCHEMA:
            return "unsupported_schema";
        case CORE_PANE_SNAPSHOT_ERR_INVALID_META:
            return "invalid_meta";
        case CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_INDEX:
            return "invalid_node_index";
        case CORE_PANE_SNAPSHOT_ERR_DUP_NODE_INDEX:
            return "duplicate_node_index";
        case CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_ID:
            return "invalid_node_id";
        case CORE_PANE_SNAPSHOT_ERR_DUP_NODE_ID:
            return "duplicate_node_id";
        case CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS:
            return "invalid_node_fields";
        case CORE_PANE_SNAPSHOT_ERR_INVALID_CHILD_REF:
            return "invalid_child_ref";
        case CORE_PANE_SNAPSHOT_ERR_CYCLE_DETECTED:
            return "cycle_detected";
        case CORE_PANE_SNAPSHOT_ERR_DISCONNECTED_GRAPH:
            return "disconnected_graph";
        case CORE_PANE_SNAPSHOT_ERR_INVALID_BINDING:
            return "invalid_binding";
        case CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_INSTANCE:
            return "duplicate_binding_instance";
        case CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_PANE:
            return "duplicate_binding_pane";
        case CORE_PANE_SNAPSHOT_ERR_BINDING_PANE_NOT_LEAF:
            return "binding_pane_not_leaf";
        default:
            return "unknown";
    }
}
