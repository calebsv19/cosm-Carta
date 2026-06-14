#include "core_pane_snapshot.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

static CorePaneSnapshotV1 make_valid_snapshot(void) {
    static const CorePaneSnapshotNodeRecordV1 k_nodes[3] = {
        { 0u, 100u, CORE_PANE_SNAPSHOT_NODE_SPLIT, CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.5f, 1u, 2u, 64.0f, 64.0f },
        { 1u, 1u, CORE_PANE_SNAPSHOT_NODE_LEAF, CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.0f, UINT32_MAX, UINT32_MAX, 0.0f, 0.0f },
        { 2u, 2u, CORE_PANE_SNAPSHOT_NODE_LEAF, CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.0f, UINT32_MAX, UINT32_MAX, 0.0f, 0.0f }
    };
    static const CorePaneSnapshotModuleBindingRecordV1 k_bindings[2] = {
        { 1u, 1u, 1001u, 11u, 22u },
        { 2u, 2u, 1002u, 33u, 44u }
    };
    CorePaneSnapshotV1 snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.meta.schema_major = CORE_PANE_SNAPSHOT_SCHEMA_MAJOR_V1;
    snapshot.meta.schema_minor = CORE_PANE_SNAPSHOT_SCHEMA_MINOR_V1;
    snapshot.meta.active_revision = 1u;
    snapshot.meta.draft_revision = 1u;
    snapshot.meta.root_node_index = 0u;
    snapshot.meta.node_count = 3u;
    snapshot.meta.module_binding_count = 2u;
    snapshot.nodes = k_nodes;
    snapshot.module_bindings = k_bindings;
    return snapshot;
}

static void copy_valid_nodes(CorePaneSnapshotNodeRecordV1 out_nodes[3]) {
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();
    memcpy(out_nodes, snapshot.nodes, sizeof(CorePaneSnapshotNodeRecordV1) * 3u);
}

static void copy_valid_bindings(CorePaneSnapshotModuleBindingRecordV1 out_bindings[2]) {
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();
    memcpy(out_bindings,
           snapshot.module_bindings,
           sizeof(CorePaneSnapshotModuleBindingRecordV1) * 2u);
}

static void test_valid_snapshot_passes(void) {
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_OK);
}

static void test_invalid_args_and_meta_fail(void) {
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();

    assert(core_pane_snapshot_validate_v1(NULL) == CORE_PANE_SNAPSHOT_ERR_INVALID_ARG);

    snapshot.meta.flags = 1u;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_META);
    snapshot = make_valid_snapshot();

    snapshot.meta.reserved0 = 1u;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_META);
    snapshot = make_valid_snapshot();

    snapshot.meta.node_count = 0u;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_META);
    snapshot = make_valid_snapshot();

    snapshot.meta.root_node_index = snapshot.meta.node_count;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_META);
    snapshot = make_valid_snapshot();

    snapshot.nodes = NULL;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_META);
    snapshot = make_valid_snapshot();

    snapshot.meta.module_binding_count = 1u;
    snapshot.module_bindings = NULL;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_META);
}

static void test_zero_binding_count_with_null_bindings_passes(void) {
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();
    snapshot.meta.module_binding_count = 0u;
    snapshot.module_bindings = NULL;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_OK);
}

static void test_duplicate_and_invalid_node_identity_fail(void) {
    CorePaneSnapshotNodeRecordV1 nodes[3];
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();

    copy_valid_nodes(nodes);
    nodes[2].node_index = 1u;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_DUP_NODE_INDEX);

    copy_valid_nodes(nodes);
    nodes[2].node_index = 7u;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_INDEX);

    copy_valid_nodes(nodes);
    nodes[2].node_index = 3u;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_INDEX);

    copy_valid_nodes(nodes);
    nodes[1].node_id = 0u;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_ID);

    copy_valid_nodes(nodes);
    nodes[2].node_id = nodes[1].node_id;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_DUP_NODE_ID);
}

static void test_invalid_node_field_shapes_fail(void) {
    CorePaneSnapshotNodeRecordV1 nodes[3];
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();

    copy_valid_nodes(nodes);
    nodes[1].axis = CORE_PANE_SNAPSHOT_AXIS_VERTICAL;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS);

    copy_valid_nodes(nodes);
    nodes[0].axis = (uint8_t)99u;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS);

    copy_valid_nodes(nodes);
    nodes[0].ratio_01 = NAN;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS);

    copy_valid_nodes(nodes);
    nodes[0].min_size_a = -1.0f;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS);

    copy_valid_nodes(nodes);
    nodes[0].min_size_b = INFINITY;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS);
}

static void test_child_reference_and_connectivity_failures(void) {
    CorePaneSnapshotNodeRecordV1 nodes[3];
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();

    copy_valid_nodes(nodes);
    nodes[0].child_a_index = 0u;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_CHILD_REF);

    copy_valid_nodes(nodes);
    nodes[0].child_b_index = 1u;
    snapshot.nodes = nodes;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_CHILD_REF);

    {
        CorePaneSnapshotNodeRecordV1 disconnected_nodes[4] = {
            { 0u, 100u, CORE_PANE_SNAPSHOT_NODE_SPLIT, CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.5f, 1u, 2u, 64.0f, 64.0f },
            { 1u, 1u, CORE_PANE_SNAPSHOT_NODE_LEAF, CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.0f, UINT32_MAX, UINT32_MAX, 0.0f, 0.0f },
            { 2u, 2u, CORE_PANE_SNAPSHOT_NODE_LEAF, CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.0f, UINT32_MAX, UINT32_MAX, 0.0f, 0.0f },
            { 3u, 3u, CORE_PANE_SNAPSHOT_NODE_LEAF, CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.0f, UINT32_MAX, UINT32_MAX, 0.0f, 0.0f }
        };
        CorePaneSnapshotV1 disconnected = make_valid_snapshot();
        disconnected.meta.node_count = 4u;
        disconnected.nodes = disconnected_nodes;
        assert(core_pane_snapshot_validate_v1(&disconnected) == CORE_PANE_SNAPSHOT_ERR_DISCONNECTED_GRAPH);
    }
}

static void test_binding_validation_failures(void) {
    CorePaneSnapshotModuleBindingRecordV1 bindings[2];
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();

    copy_valid_bindings(bindings);
    bindings[0].instance_id = 0u;
    snapshot.module_bindings = bindings;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_BINDING);

    copy_valid_bindings(bindings);
    bindings[0].pane_node_id = 0u;
    snapshot.module_bindings = bindings;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_BINDING);

    copy_valid_bindings(bindings);
    bindings[0].module_type_id = 0u;
    snapshot.module_bindings = bindings;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_INVALID_BINDING);

    copy_valid_bindings(bindings);
    bindings[1].pane_node_id = 999u;
    snapshot.module_bindings = bindings;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_BINDING_PANE_NOT_LEAF);

    copy_valid_bindings(bindings);
    bindings[1].pane_node_id = 100u;
    snapshot.module_bindings = bindings;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_BINDING_PANE_NOT_LEAF);

    copy_valid_bindings(bindings);
    bindings[1].instance_id = bindings[0].instance_id;
    snapshot.module_bindings = bindings;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_INSTANCE);

    copy_valid_bindings(bindings);
    bindings[1].pane_node_id = bindings[0].pane_node_id;
    snapshot.module_bindings = bindings;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_PANE);
}

static void test_result_strings_cover_all_values(void) {
    static const struct {
        CorePaneSnapshotResult result;
        const char *expected;
    } k_cases[] = {
        { CORE_PANE_SNAPSHOT_OK, "ok" },
        { CORE_PANE_SNAPSHOT_ERR_INVALID_ARG, "invalid_arg" },
        { CORE_PANE_SNAPSHOT_ERR_UNSUPPORTED_SCHEMA, "unsupported_schema" },
        { CORE_PANE_SNAPSHOT_ERR_INVALID_META, "invalid_meta" },
        { CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_INDEX, "invalid_node_index" },
        { CORE_PANE_SNAPSHOT_ERR_DUP_NODE_INDEX, "duplicate_node_index" },
        { CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_ID, "invalid_node_id" },
        { CORE_PANE_SNAPSHOT_ERR_DUP_NODE_ID, "duplicate_node_id" },
        { CORE_PANE_SNAPSHOT_ERR_INVALID_NODE_FIELDS, "invalid_node_fields" },
        { CORE_PANE_SNAPSHOT_ERR_INVALID_CHILD_REF, "invalid_child_ref" },
        { CORE_PANE_SNAPSHOT_ERR_CYCLE_DETECTED, "cycle_detected" },
        { CORE_PANE_SNAPSHOT_ERR_DISCONNECTED_GRAPH, "disconnected_graph" },
        { CORE_PANE_SNAPSHOT_ERR_INVALID_BINDING, "invalid_binding" },
        { CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_INSTANCE, "duplicate_binding_instance" },
        { CORE_PANE_SNAPSHOT_ERR_DUP_BINDING_PANE, "duplicate_binding_pane" },
        { CORE_PANE_SNAPSHOT_ERR_BINDING_PANE_NOT_LEAF, "binding_pane_not_leaf" }
    };
    size_t i;

    for (i = 0u; i < sizeof(k_cases) / sizeof(k_cases[0]); ++i) {
        assert(strcmp(core_pane_snapshot_result_string(k_cases[i].result), k_cases[i].expected) == 0);
    }

    assert(strcmp(core_pane_snapshot_result_string((CorePaneSnapshotResult)999), "unknown") == 0);
}

static void test_unsupported_schema_fails(void) {
    CorePaneSnapshotV1 snapshot = make_valid_snapshot();
    snapshot.meta.schema_minor = 1u;
    assert(core_pane_snapshot_validate_v1(&snapshot) == CORE_PANE_SNAPSHOT_ERR_UNSUPPORTED_SCHEMA);
}

int main(void) {
    test_valid_snapshot_passes();
    test_invalid_args_and_meta_fail();
    test_zero_binding_count_with_null_bindings_passes();
    test_duplicate_and_invalid_node_identity_fail();
    test_invalid_node_field_shapes_fail();
    test_child_reference_and_connectivity_failures();
    test_binding_validation_failures();
    test_result_strings_cover_all_values();
    test_unsupported_schema_fails();
    return 0;
}
