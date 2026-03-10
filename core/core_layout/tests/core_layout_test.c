#include "core_layout.h"

#include <assert.h>

int main(void) {
    CoreLayoutState state;

    core_layout_state_init(&state);
    assert(state.mode == CORE_LAYOUT_MODE_RUNTIME);
    assert(state.active_revision == 1u);
    assert(state.draft_revision == 1u);
    assert(!state.has_pending_changes);
    assert(!state.rebuild_required);

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

    return 0;
}
