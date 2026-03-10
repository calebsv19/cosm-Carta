#include "core_layout.h"

void core_layout_state_init(CoreLayoutState *state) {
    if (!state) {
        return;
    }

    state->mode = CORE_LAYOUT_MODE_RUNTIME;
    state->active_revision = 1u;
    state->draft_revision = 1u;
    state->has_pending_changes = false;
    state->rebuild_required = false;
}

bool core_layout_enter_authoring(CoreLayoutState *state) {
    if (!state || state->mode == CORE_LAYOUT_MODE_AUTHORING) {
        return false;
    }

    state->mode = CORE_LAYOUT_MODE_AUTHORING;
    state->draft_revision = state->active_revision;
    state->has_pending_changes = false;
    return true;
}

bool core_layout_mark_draft_changed(CoreLayoutState *state) {
    if (!state || state->mode != CORE_LAYOUT_MODE_AUTHORING) {
        return false;
    }

    state->has_pending_changes = true;
    return true;
}

bool core_layout_apply_authoring(CoreLayoutState *state) {
    if (!state || state->mode != CORE_LAYOUT_MODE_AUTHORING) {
        return false;
    }

    if (state->has_pending_changes) {
        state->active_revision += 1u;
        state->draft_revision = state->active_revision;
        state->rebuild_required = true;
    }

    state->has_pending_changes = false;
    state->mode = CORE_LAYOUT_MODE_RUNTIME;
    return true;
}

bool core_layout_cancel_authoring(CoreLayoutState *state) {
    if (!state || state->mode != CORE_LAYOUT_MODE_AUTHORING) {
        return false;
    }

    state->draft_revision = state->active_revision;
    state->has_pending_changes = false;
    state->mode = CORE_LAYOUT_MODE_RUNTIME;
    return true;
}

void core_layout_acknowledge_rebuild(CoreLayoutState *state) {
    if (!state) {
        return;
    }

    state->rebuild_required = false;
}
