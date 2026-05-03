#include "core_layout.h"

static CoreLayoutRevisionMetadata core_layout_default_revision_metadata(void) {
    CoreLayoutRevisionMetadata metadata;
    metadata.source = CORE_LAYOUT_REVISION_SOURCE_UNKNOWN;
    metadata.snapshot_schema_major = 0u;
    metadata.snapshot_schema_minor = 0u;
    metadata.reserved0 = 0u;
    return metadata;
}

static int core_layout_source_is_valid(CoreLayoutRevisionSource source) {
    return source == CORE_LAYOUT_REVISION_SOURCE_UNKNOWN ||
           source == CORE_LAYOUT_REVISION_SOURCE_USER_EDIT ||
           source == CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT ||
           source == CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_RESTORE;
}

static void core_layout_apply_metadata_if_valid(CoreLayoutState *state,
                                                const CoreLayoutRevisionMetadata *metadata) {
    if (!state || !metadata || !core_layout_source_is_valid(metadata->source)) {
        return;
    }
    state->revision_metadata = *metadata;
    state->revision_metadata.reserved0 = 0u;
}

void core_layout_state_init(CoreLayoutState *state) {
    if (!state) {
        return;
    }

    state->mode = CORE_LAYOUT_MODE_RUNTIME;
    state->active_revision = 1u;
    state->draft_revision = 1u;
    state->has_pending_changes = false;
    state->rebuild_required = false;
    state->revision_metadata = core_layout_default_revision_metadata();
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
    CoreLayoutRevisionMetadata metadata;
    if (!state || state->mode != CORE_LAYOUT_MODE_AUTHORING) {
        return false;
    }

    if (state->has_pending_changes) {
        metadata = state->revision_metadata;
        if (!core_layout_source_is_valid(metadata.source) ||
            metadata.source == CORE_LAYOUT_REVISION_SOURCE_UNKNOWN) {
            metadata.source = CORE_LAYOUT_REVISION_SOURCE_USER_EDIT;
            metadata.snapshot_schema_major = 0u;
            metadata.snapshot_schema_minor = 0u;
        }
        state->active_revision += 1u;
        state->draft_revision = state->active_revision;
        state->rebuild_required = true;
        core_layout_apply_metadata_if_valid(state, &metadata);
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

bool core_layout_apply_external_revision(CoreLayoutState *state,
                                         CoreLayoutRevisionSource source,
                                         uint16_t snapshot_schema_major,
                                         uint16_t snapshot_schema_minor) {
    CoreLayoutRevisionMetadata metadata;
    if (!state || !core_layout_source_is_valid(source) ||
        source == CORE_LAYOUT_REVISION_SOURCE_UNKNOWN) {
        return false;
    }

    metadata.source = source;
    metadata.snapshot_schema_major = snapshot_schema_major;
    metadata.snapshot_schema_minor = snapshot_schema_minor;
    metadata.reserved0 = 0u;

    state->mode = CORE_LAYOUT_MODE_RUNTIME;
    state->has_pending_changes = false;
    state->active_revision += 1u;
    state->draft_revision = state->active_revision;
    state->rebuild_required = true;
    core_layout_apply_metadata_if_valid(state, &metadata);
    return true;
}

void core_layout_set_revision_metadata(CoreLayoutState *state,
                                       const CoreLayoutRevisionMetadata *metadata) {
    if (!state || !metadata) {
        return;
    }
    core_layout_apply_metadata_if_valid(state, metadata);
}

void core_layout_get_revision_metadata(const CoreLayoutState *state,
                                       CoreLayoutRevisionMetadata *out_metadata) {
    if (!out_metadata) {
        return;
    }
    *out_metadata = core_layout_default_revision_metadata();
    if (!state) {
        return;
    }
    *out_metadata = state->revision_metadata;
}
