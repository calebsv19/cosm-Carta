#ifndef CORE_PANE_SNAPSHOT_H
#define CORE_PANE_SNAPSHOT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_PANE_SNAPSHOT_SCHEMA_MAJOR_V1 1u
#define CORE_PANE_SNAPSHOT_SCHEMA_MINOR_V1 0u

typedef enum CorePaneSnapshotNodeType {
    CORE_PANE_SNAPSHOT_NODE_LEAF = 0,
    CORE_PANE_SNAPSHOT_NODE_SPLIT = 1
} CorePaneSnapshotNodeType;

typedef enum CorePaneSnapshotAxis {
    CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL = 0,
    CORE_PANE_SNAPSHOT_AXIS_VERTICAL = 1
} CorePaneSnapshotAxis;

typedef struct CorePaneSnapshotMetaV1 {
    uint16_t schema_major;
    uint16_t schema_minor;
    uint32_t flags;
    uint64_t active_revision;
    uint64_t draft_revision;
    uint32_t root_node_index;
    uint32_t node_count;
    uint32_t module_binding_count;
    uint32_t reserved0;
} CorePaneSnapshotMetaV1;

typedef struct CorePaneSnapshotNodeRecordV1 {
    uint32_t node_index;
    uint32_t node_id;
    uint8_t node_type;
    uint8_t axis;
    uint16_t reserved0;
    float ratio_01;
    uint32_t child_a_index;
    uint32_t child_b_index;
    float min_size_a;
    float min_size_b;
} CorePaneSnapshotNodeRecordV1;

typedef struct CorePaneSnapshotModuleBindingRecordV1 {
    uint32_t instance_id;
    uint32_t pane_node_id;
    uint32_t module_type_id;
    uint16_t config_variant;
    uint16_t state_flags;
} CorePaneSnapshotModuleBindingRecordV1;

typedef struct CorePaneSnapshotV1 {
    CorePaneSnapshotMetaV1 meta;
    const CorePaneSnapshotNodeRecordV1 *nodes;
    const CorePaneSnapshotModuleBindingRecordV1 *module_bindings;
} CorePaneSnapshotV1;

typedef enum CorePaneSnapshotResult {
    CORE_PANE_SNAPSHOT_OK = 0,
    CORE_PANE_SNAPSHOT_ERR_INVALID_ARG = 1,
    CORE_PANE_SNAPSHOT_ERR_UNSUPPORTED_SCHEMA = 2,
    CORE_PANE_SNAPSHOT_ERR_INVALID_META = 3,
    CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_INDEX = 4,
    CORE_PANE_SNAPSHOT_ERR_DUP_NODE_INDEX = 5,
    CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_ID = 6,
    CORE_PANE_SNAPSHOT_ERR_DUP_NODE_ID = 7,
    CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS = 8,
    CORE_PANE_SNAPSHOT_ERR_INVALID_CHILD_REF = 9,
    CORE_PANE_SNAPSHOT_ERR_CYCLE_DETECTED = 10,
    CORE_PANE_SNAPSHOT_ERR_DISCONNECTED_GRAPH = 11,
    CORE_PANE_SNAPSHOT_ERR_INVALID_BINDING = 12,
    CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_INSTANCE = 13,
    CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_PANE = 14,
    CORE_PANE_SNAPSHOT_ERR_BINDING_PANE_NOT_LEAF = 15
} CorePaneSnapshotResult;

CorePaneSnapshotResult core_pane_snapshot_validate_v1(const CorePaneSnapshotV1 *snapshot);
const char *core_pane_snapshot_result_string(CorePaneSnapshotResult result);

#ifdef __cplusplus
}
#endif

#endif
