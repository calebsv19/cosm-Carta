#ifndef CORE_LAYOUT_H
#define CORE_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum CoreLayoutMode {
    CORE_LAYOUT_MODE_RUNTIME = 0,
    CORE_LAYOUT_MODE_AUTHORING = 1
} CoreLayoutMode;

typedef enum CoreLayoutRevisionSource {
    CORE_LAYOUT_REVISION_SOURCE_UNKNOWN = 0,
    CORE_LAYOUT_REVISION_SOURCE_USER_EDIT = 1,
    CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT = 2,
    CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_RESTORE = 3
} CoreLayoutRevisionSource;

typedef struct CoreLayoutRevisionMetadata {
    CoreLayoutRevisionSource source;
    uint16_t snapshot_schema_major;
    uint16_t snapshot_schema_minor;
    uint32_t reserved0;
} CoreLayoutRevisionMetadata;

typedef struct CoreLayoutState {
    CoreLayoutMode mode;
    uint64_t active_revision;
    uint64_t draft_revision;
    bool has_pending_changes;
    bool rebuild_required;
    CoreLayoutRevisionMetadata revision_metadata;
} CoreLayoutState;

void core_layout_state_init(CoreLayoutState *state);
bool core_layout_enter_authoring(CoreLayoutState *state);
bool core_layout_mark_draft_changed(CoreLayoutState *state);
bool core_layout_apply_authoring(CoreLayoutState *state);
bool core_layout_cancel_authoring(CoreLayoutState *state);
void core_layout_acknowledge_rebuild(CoreLayoutState *state);
bool core_layout_apply_external_revision(CoreLayoutState *state,
                                         CoreLayoutRevisionSource source,
                                         uint16_t snapshot_schema_major,
                                         uint16_t snapshot_schema_minor);
void core_layout_set_revision_metadata(CoreLayoutState *state,
                                       const CoreLayoutRevisionMetadata *metadata);
void core_layout_get_revision_metadata(const CoreLayoutState *state,
                                       CoreLayoutRevisionMetadata *out_metadata);

#endif
