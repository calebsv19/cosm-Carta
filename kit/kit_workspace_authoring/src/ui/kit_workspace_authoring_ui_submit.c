#include "kit_workspace_authoring_ui.h"

#include <string.h>

int kit_workspace_authoring_ui_pane_overlay_visible(int layout_mode,
                                                    int authoring_layout_mode_value,
                                                    int overlay_mode,
                                                    int font_theme_overlay_mode_value) {
    return !(layout_mode == authoring_layout_mode_value && overlay_mode == font_theme_overlay_mode_value);
}

void kit_workspace_authoring_ui_derive_frame(KitWorkspaceAuthoringRenderDeriveFrame *out_derive,
                                             int width,
                                             int height,
                                             int selected_pane_id,
                                             int selected_corner_group_has_key,
                                             int32_t selected_corner_group_qx,
                                             int32_t selected_corner_group_qy,
                                             int hover_pane_id,
                                             int shift_held,
                                             int splitter_drag_rewrite_armed,
                                             int splitter_drag_preview_active,
                                             int splitter_drag_preview_vertical_line,
                                             float splitter_drag_preview_coord,
                                             float splitter_drag_range_start,
                                             float splitter_drag_range_end,
                                             uint32_t splitter_snap_corner_id,
                                             uint8_t splitter_snap_lock_state,
                                             int frame_index,
                                             int pending_apply) {
    if (!out_derive) {
        return;
    }
    memset(out_derive, 0, sizeof(*out_derive));
    out_derive->width = width;
    out_derive->height = height;
    out_derive->selected_pane_id = selected_pane_id;
    out_derive->selected_corner_group_has_key = selected_corner_group_has_key;
    out_derive->selected_corner_group_qx = selected_corner_group_qx;
    out_derive->selected_corner_group_qy = selected_corner_group_qy;
    out_derive->hover_pane_id = hover_pane_id;
    out_derive->shift_held = shift_held;
    out_derive->splitter_drag_rewrite_armed = splitter_drag_rewrite_armed;
    out_derive->splitter_drag_preview_active = splitter_drag_preview_active;
    out_derive->splitter_drag_preview_vertical_line = splitter_drag_preview_vertical_line;
    out_derive->splitter_drag_preview_coord = splitter_drag_preview_coord;
    out_derive->splitter_drag_range_start = splitter_drag_range_start;
    out_derive->splitter_drag_range_end = splitter_drag_range_end;
    out_derive->splitter_snap_corner_id = splitter_snap_corner_id;
    out_derive->splitter_snap_lock_state = splitter_snap_lock_state;
    out_derive->frame_index = frame_index;
    out_derive->pending_apply = pending_apply;
}

void kit_workspace_authoring_ui_submit_frame(
    void *host_context,
    const KitWorkspaceAuthoringRenderDeriveFrame *derive,
    KitWorkspaceAuthoringRenderSubmitDrawFn draw_scene,
    KitWorkspaceAuthoringRenderSubmitRebuildRequiredFn rebuild_required,
    KitWorkspaceAuthoringRenderSubmitAcknowledgeRebuildFn acknowledge_rebuild,
    KitWorkspaceAuthoringRenderSubmitOutcome *outcome) {
    if (!outcome) {
        return;
    }
    memset(outcome, 0, sizeof(*outcome));
    if (!derive || !draw_scene) {
        outcome->draw_result = (CoreResult){ CORE_ERR_INVALID_ARG, "invalid render submit request" };
        return;
    }

    outcome->draw_result = draw_scene(host_context, derive);
    if (outcome->draw_result.code != CORE_OK) {
        return;
    }

    if (rebuild_required && acknowledge_rebuild && rebuild_required(host_context)) {
        acknowledge_rebuild(host_context);
        outcome->rebuild_acknowledged = 1u;
    }
}
