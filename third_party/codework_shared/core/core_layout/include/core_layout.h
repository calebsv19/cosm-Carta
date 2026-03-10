#ifndef CORE_LAYOUT_H
#define CORE_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum CoreLayoutMode {
    CORE_LAYOUT_MODE_RUNTIME = 0,
    CORE_LAYOUT_MODE_AUTHORING = 1
} CoreLayoutMode;

typedef struct CoreLayoutState {
    CoreLayoutMode mode;
    uint64_t active_revision;
    uint64_t draft_revision;
    bool has_pending_changes;
    bool rebuild_required;
} CoreLayoutState;

void core_layout_state_init(CoreLayoutState *state);
bool core_layout_enter_authoring(CoreLayoutState *state);
bool core_layout_mark_draft_changed(CoreLayoutState *state);
bool core_layout_apply_authoring(CoreLayoutState *state);
bool core_layout_cancel_authoring(CoreLayoutState *state);
void core_layout_acknowledge_rebuild(CoreLayoutState *state);

#endif
