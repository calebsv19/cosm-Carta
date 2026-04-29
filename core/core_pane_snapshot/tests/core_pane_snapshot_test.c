#include "core_pane_snapshot.h"

#include <assert.h>

static CorePaneSnapshotV1 make_valid_snapshot(void) {
    static const CorePaneSnapshotNodeRecordV1 k_nodes[3] = {
        { 0u, 100u, CORE_PANE_SNAPSHOT_NODE_SPLIT, CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.5f, 1u, 2u, 64.0f, 64.0f },
        { 1u, 1u, CORE_PANE_SNAPSHOT_NODE_LEAF, CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.0f, UINT32_MAX, UINT32_MAX, 0.0f, 0.0f },
        { 2u, 2u, CORE_PANE_SNAPSHOT_NODE_LEAF, CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.0f, UINT32_MAX, UINT32_MAX, 0.0f, 0.0f }
    };
    static const CorePaneSnapshotModuleBindingRecordV1 k_bindings[2] = {
        { 1u, 1u, 1001u, 0u, 0u },
        { 2u, 2u, 1002u, 0u, 0u }
    };
    CorePaneSnapshotV1 snapshot = {0};

    snapshot.meta.schema_major = CORE_PANE_SNAPSHOT_SCHEMA_MAJOR_V1;
    snapshot.meta.schema_minor = CORE_PANE_SNAPSHOT_SCHEMA_MINOR_V1;
    snapshot.meta.flags = 0u;
    snapshot.meta.active_revision = 1u;
    snapshot.meta.draft_revision = 1u;
    snapshot.meta.root_node_index = 0u;
    snapshot.meta.node_count = 3u;
    snapshot.meta.module_binding_count = 2u;
    snapshot.meta.reserved0 = 0u;
    snapshot.nodes = k_nodes;
    snapshot.module_bindings = k_bindings;
    return snapshot;
}

static void test_valid_snapshot_passes(void) {
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_OK);
}

static void test_duplicate_node_index_fails(void) {
    CorePaneSnapshotNodeRecordV1 nodes[3];
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();

    nodes[0] = snapshot.nodes[0];
    nodes[1] = snapshot.nodes[1];
    nodes[2] = snapshot.nodes[2];
    nodes[2].node_index = 1u;

    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_DUP_NODE_INDEX);
}

static void test_cycle_fails(void) {
    CorePaneSnapshotNodeRecordV1 nodes[3];
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();

    nodes[0] = snapshot.nodes[0];
    nodes[1] = snapshot.nodes[1];
    nodes[2] = snapshot.nodes[2];
    nodes[0].child_a_index = 0u;

    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_CHILD_REF);
}

static void test_disconnected_graph_fails(void) {
    CorePaneSnapshotNodeRecordV1 nodes[3];
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();

    nodes[0] = snapshot.nodes[0];
    nodes[1] = snapshot.nodes[1];
    nodes[2] = snapshot.nodes[2];
    nodes[0].child_b_index = 1u;

    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_CHILD_REF);
}

static void test_binding_unknown_leaf_fails(void) {
    CorePaneSnapshotModuleBindingRecordV1 bindings[2];
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();

    bindings[0] = snapshot.module_bindings[0];
    bindings[1] = snapshot.module_bindings[1];
    bindings[1].pane_node_id = 999u;

    snapshot.module_bindings = bindings;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_BINDING_PANE_NOT_LEAF);
}

static void test_duplicate_binding_instance_fails(void) {
    CorePaneSnapshotModuleBindingRecordV1 bindings[2];
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();

    bindings[0] = snapshot.module_bindings[0];
    bindings[1] = snapshot.module_bindings[1];
    bindings[1].instance_id = bindings[0].instance_id;

    snapshot.module_bindings = bindings;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_INSTANCE);
}

static void test_unsupported_schema_fails(void) {
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();
    snapshot.meta.schema_minor = 1u;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_UNSUPPORTED_SCHEMA);
}

int main(void) {
    test_valid_snapshot_passes();
    test_duplicate_node_index_fails();
    test_cycle_fails();
    test_disconnected_graph_fails();
    test_binding_unknown_leaf_fails();
    test_duplicate_binding_instance_fails();
    test_unsupported_schema_fails();
    return 0;
}
