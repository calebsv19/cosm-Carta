#include "core_layout.h"

#include <assert.h>
#include <stddef.h>

static void test_init_and_default_metadata(void) {
    CoreLayoutState state;
    CoreLayoutRevisionMetadata metadata;

    core_layout_state_init(&state);
    assert(state.mode == CORE_LAYOUT_MODE_RUNTIME);
    assert(state.active_revision == 1u);
    assert(state.draft_revision == 1u);
    assert(!state.has_pending_changes);
    assert(!state.rebuild_required);
    core_layout_get_revision_metadata(&state, &metadata);
    assert(metadata.source == CORE_LAYOUT_REVISION_SOURCE_UNKNOWN);
    assert(metadata.snapshot_schema_major == 0u);
    assert(metadata.snapshot_schema_minor == 0u);
    assert(metadata.reserved0 == 0u);
}

static void test_apply_authoring_with_pending_changes_sets_user_edit_metadata(void) {
    CoreLayoutState state;
    CoreLayoutRevisionMetadata metadata;

    core_layout_state_init(&state);
    assert(core_layout_enter_authoring(&state));
    assert(state.mode == CORE_LAYOUT_MODE_AUTHORING);
    assert(!state.has_pending_changes);

    assert(core_layout_mark_draft_changed(&state));
    assert(state.has_pending_changes);

    assert(core_layout_apply_authoring(&state));
    assert(state.mode == CORE_LAYOUT_MODE_RUNTIME);
    assert(state.active_revision == 2u);
    assert(state.draft_revision == 2u);
    assert(state.rebuild_required);
    core_layout_get_revision_metadata(&state, &metadata);
    assert(metadata.source == CORE_LAYOUT_REVISION_SOURCE_USER_EDIT);
    assert(metadata.snapshot_schema_major == 0u);
    assert(metadata.snapshot_schema_minor == 0u);
    assert(metadata.reserved0 == 0u);
    core_layout_acknowledge_rebuild(&state);
    assert(!state.rebuild_required);
}

static void test_apply_authoring_without_pending_changes_is_no_op(void) {
    CoreLayoutState state;
    CoreLayoutRevisionMetadata metadata;

    core_layout_state_init(&state);
    assert(core_layout_enter_authoring(&state));
    assert(core_layout_apply_authoring(&state));
    assert(state.mode == CORE_LAYOUT_MODE_RUNTIME);
    assert(state.active_revision == 1u);
    assert(state.draft_revision == 1u);
    assert(!state.has_pending_changes);
    assert(!state.rebuild_required);
    core_layout_get_revision_metadata(&state, &metadata);
    assert(metadata.source == CORE_LAYOUT_REVISION_SOURCE_UNKNOWN);
}

static void test_cancel_authoring_restores_runtime_without_revision_change(void) {
    CoreLayoutState state;

    core_layout_state_init(&state);

    assert(core_layout_enter_authoring(&state));
    assert(core_layout_cancel_authoring(&state));
    assert(state.mode == CORE_LAYOUT_MODE_RUNTIME);
    assert(state.active_revision == 1u);
    assert(state.draft_revision == 1u);
    assert(!state.has_pending_changes);
}

static void test_invalid_transition_paths_fail(void) {
    CoreLayoutState state;

    core_layout_state_init(&state);

    assert(!core_layout_mark_draft_changed(&state));
    assert(!core_layout_apply_authoring(&state));
    assert(!core_layout_cancel_authoring(&state));
    assert(!core_layout_enter_authoring(NULL));
    assert(!core_layout_mark_draft_changed(NULL));
    assert(!core_layout_apply_authoring(NULL));
    assert(!core_layout_cancel_authoring(NULL));
}

static void test_external_revision_apply_supports_import_and_restore(void) {
    CoreLayoutState state;
    CoreLayoutRevisionMetadata metadata;

    core_layout_state_init(&state);

    assert(core_layout_apply_external_revision(&state,
                                               CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT,
                                               1u,
                                               0u));
    assert(state.mode == CORE_LAYOUT_MODE_RUNTIME);
    assert(state.rebuild_required);
    core_layout_get_revision_metadata(&state, &metadata);
    assert(metadata.source == CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT);
    assert(metadata.snapshot_schema_major == 1u);
    assert(metadata.snapshot_schema_minor == 0u);
    core_layout_acknowledge_rebuild(&state);
    assert(!state.rebuild_required);
    assert(core_layout_apply_external_revision(&state,
                                               CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_RESTORE,
                                               2u,
                                               5u));
    assert(state.mode == CORE_LAYOUT_MODE_RUNTIME);
    assert(state.active_revision == 3u);
    assert(state.draft_revision == 3u);
    assert(!state.has_pending_changes);
    assert(state.rebuild_required);
    core_layout_get_revision_metadata(&state, &metadata);
    assert(metadata.source == CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_RESTORE);
    assert(metadata.snapshot_schema_major == 2u);
    assert(metadata.snapshot_schema_minor == 5u);
    assert(metadata.reserved0 == 0u);
}

static void test_external_revision_resets_authoring_state(void) {
    CoreLayoutState state;

    core_layout_state_init(&state);
    assert(core_layout_enter_authoring(&state));
    assert(core_layout_mark_draft_changed(&state));
    assert(core_layout_apply_external_revision(&state,
                                               CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT,
                                               3u,
                                               1u));
    assert(state.mode == CORE_LAYOUT_MODE_RUNTIME);
    assert(state.active_revision == 2u);
    assert(state.draft_revision == 2u);
    assert(!state.has_pending_changes);
    assert(state.rebuild_required);
}

static void test_invalid_external_revision_source_is_rejected(void) {
    CoreLayoutState state;

    core_layout_state_init(&state);
    assert(!core_layout_apply_external_revision(&state,
                                                CORE_LAYOUT_REVISION_SOURCE_UNKNOWN,
                                                1u,
                                                0u));
    assert(!core_layout_apply_external_revision(NULL,
                                                CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT,
                                                1u,
                                                0u));
}

static void test_revision_metadata_set_get_and_invalid_ignore(void) {
    CoreLayoutState state;
    CoreLayoutRevisionMetadata metadata;

    core_layout_state_init(&state);

    metadata.source = CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT;
    metadata.snapshot_schema_major = 7u;
    metadata.snapshot_schema_minor = 9u;
    metadata.reserved0 = 12345u;
    core_layout_set_revision_metadata(&state, &metadata);

    metadata.source = CORE_LAYOUT_REVISION_SOURCE_UNKNOWN;
    metadata.snapshot_schema_major = 0u;
    metadata.snapshot_schema_minor = 0u;
    metadata.reserved0 = 0u;
    core_layout_get_revision_metadata(&state, &metadata);
    assert(metadata.source == CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT);
    assert(metadata.snapshot_schema_major == 7u);
    assert(metadata.snapshot_schema_minor == 9u);
    assert(metadata.reserved0 == 0u);

    {
        CoreLayoutRevisionMetadata invalid = {
            (CoreLayoutRevisionSource)99,
            11u,
            12u,
            13u
        };
        core_layout_set_revision_metadata(&state, &invalid);
    }
    core_layout_get_revision_metadata(&state, &metadata);
    assert(metadata.source == CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT);
    assert(metadata.snapshot_schema_major == 7u);
    assert(metadata.snapshot_schema_minor == 9u);
    assert(metadata.reserved0 == 0u);
}

static void test_metadata_preload_carries_into_authoring_apply(void) {
    CoreLayoutState state;
    CoreLayoutRevisionMetadata metadata = {
        CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT,
        4u,
        2u,
        999u
    };

    core_layout_state_init(&state);
    core_layout_set_revision_metadata(&state, &metadata);
    assert(core_layout_enter_authoring(&state));
    assert(core_layout_mark_draft_changed(&state));
    assert(core_layout_apply_authoring(&state));
    core_layout_get_revision_metadata(&state, &metadata);
    assert(metadata.source == CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT);
    assert(metadata.snapshot_schema_major == 4u);
    assert(metadata.snapshot_schema_minor == 2u);
    assert(metadata.reserved0 == 0u);
}

static void test_null_helpers_are_no_op_or_default_safe(void) {
    CoreLayoutRevisionMetadata metadata = {
        CORE_LAYOUT_REVISION_SOURCE_USER_EDIT,
        1u,
        1u,
        1u
    };

    core_layout_state_init(NULL);
    core_layout_acknowledge_rebuild(NULL);
    core_layout_set_revision_metadata(NULL, &metadata);
    core_layout_set_revision_metadata(&(CoreLayoutState){0}, NULL);

    core_layout_get_revision_metadata(NULL, &metadata);
    assert(metadata.source == CORE_LAYOUT_REVISION_SOURCE_UNKNOWN);
    assert(metadata.snapshot_schema_major == 0u);
    assert(metadata.snapshot_schema_minor == 0u);
    assert(metadata.reserved0 == 0u);

    core_layout_get_revision_metadata(NULL, NULL);
}

static void test_acknowledge_rebuild_is_idempotent(void) {
    CoreLayoutState state;

    core_layout_state_init(&state);
    core_layout_acknowledge_rebuild(&state);
    assert(!state.rebuild_required);
    assert(core_layout_apply_external_revision(&state,
                                               CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT,
                                               1u,
                                               0u));
    assert(state.rebuild_required);
    core_layout_acknowledge_rebuild(&state);
    assert(!state.rebuild_required);
    core_layout_acknowledge_rebuild(&state);
    assert(!state.rebuild_required);
}

int main(void) {
    test_init_and_default_metadata();
    test_apply_authoring_with_pending_changes_sets_user_edit_metadata();
    test_apply_authoring_without_pending_changes_is_no_op();
    test_cancel_authoring_restores_runtime_without_revision_change();
    test_invalid_transition_paths_fail();
    test_external_revision_apply_supports_import_and_restore();
    test_external_revision_resets_authoring_state();
    test_invalid_external_revision_source_is_rejected();
    test_revision_metadata_set_get_and_invalid_ignore();
    test_metadata_preload_carries_into_authoring_apply();
    test_null_helpers_are_no_op_or_default_safe();
    test_acknowledge_rebuild_is_idempotent();

    return 0;
}
