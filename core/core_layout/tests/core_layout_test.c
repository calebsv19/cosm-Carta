#include "core_layout.h"

#include <assert.h>

int main(void) {
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
    core_layout_acknowledge_rebuild(&state);
    assert(!state.rebuild_required);

    assert(core_layout_enter_authoring(&state));
    assert(core_layout_cancel_authoring(&state));
    assert(state.mode == CORE_LAYOUT_MODE_RUNTIME);
    assert(state.active_revision == 2u);
    assert(state.draft_revision == 2u);

    assert(!core_layout_mark_draft_changed(&state));
    assert(!core_layout_apply_authoring(&state));
    assert(!core_layout_cancel_authoring(&state));

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
    assert(!core_layout_apply_external_revision(&state,
                                                CORE_LAYOUT_REVISION_SOURCE_UNKNOWN,
                                                1u,
                                                0u));

    return 0;
}
