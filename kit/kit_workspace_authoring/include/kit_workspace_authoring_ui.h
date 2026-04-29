#ifndef KIT_WORKSPACE_AUTHORING_UI_H
#define KIT_WORKSPACE_AUTHORING_UI_H

#include <stdint.h>

#include "core_base.h"
#include "core_pane.h"
#include "core_theme.h"
#include "kit_render.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum KitWorkspaceAuthoringOverlayButtonId {
    KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE = 0,
    KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE = 1,
    KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY = 2,
    KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL = 3,
    KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD = 4
} KitWorkspaceAuthoringOverlayButtonId;

typedef struct KitWorkspaceAuthoringOverlayButton {
    KitWorkspaceAuthoringOverlayButtonId id;
    CorePaneRect rect;
    const char *label;
    uint8_t visible;
    uint8_t enabled;
} KitWorkspaceAuthoringOverlayButton;

typedef enum KitWorkspaceAuthoringDropIntent {
    KIT_WORKSPACE_AUTHORING_DROP_INTENT_NONE = 0,
    KIT_WORKSPACE_AUTHORING_DROP_INTENT_LEFT = 1,
    KIT_WORKSPACE_AUTHORING_DROP_INTENT_RIGHT = 2,
    KIT_WORKSPACE_AUTHORING_DROP_INTENT_TOP = 3,
    KIT_WORKSPACE_AUTHORING_DROP_INTENT_BOTTOM = 4
} KitWorkspaceAuthoringDropIntent;

typedef struct KitWorkspaceAuthoringRenderDeriveFrame {
    int width;
    int height;
    int selected_pane_id;
    int selected_corner_group_has_key;
    int32_t selected_corner_group_qx;
    int32_t selected_corner_group_qy;
    int hover_pane_id;
    int shift_held;
    int splitter_drag_rewrite_armed;
    int splitter_drag_preview_active;
    int splitter_drag_preview_vertical_line;
    float splitter_drag_preview_coord;
    float splitter_drag_range_start;
    float splitter_drag_range_end;
    uint32_t splitter_snap_corner_id;
    uint8_t splitter_snap_lock_state;
    int frame_index;
    int pending_apply;
} KitWorkspaceAuthoringRenderDeriveFrame;

typedef struct KitWorkspaceAuthoringRenderSubmitOutcome {
    CoreResult draw_result;
    uint8_t rebuild_acknowledged;
} KitWorkspaceAuthoringRenderSubmitOutcome;

typedef CoreResult (*KitWorkspaceAuthoringRenderSubmitDrawFn)(
    void *host_context,
    const KitWorkspaceAuthoringRenderDeriveFrame *derive);
typedef int (*KitWorkspaceAuthoringRenderSubmitRebuildRequiredFn)(void *host_context);
typedef void (*KitWorkspaceAuthoringRenderSubmitAcknowledgeRebuildFn)(void *host_context);

uint32_t kit_workspace_authoring_ui_build_overlay_buttons(int viewport_width,
                                                          int authoring_active,
                                                          int pane_overlay_active,
                                                          KitWorkspaceAuthoringOverlayButton *out_buttons,
                                                          uint32_t cap);

KitWorkspaceAuthoringOverlayButtonId kit_workspace_authoring_ui_overlay_hit_test(
    const KitWorkspaceAuthoringOverlayButton *buttons,
    uint32_t count,
    float x,
    float y);

const char *kit_workspace_authoring_ui_drop_intent_label(KitWorkspaceAuthoringDropIntent intent);

KitWorkspaceAuthoringDropIntent kit_workspace_authoring_ui_drop_intent_from_point(CorePaneRect rect,
                                                                                   float x,
                                                                                   float y,
                                                                                   CorePaneRect *out_ghost_rect);

CoreResult kit_workspace_authoring_ui_draw_overlay_buttons(KitRenderContext *render_ctx,
                                                           KitRenderFrame *frame,
                                                           const KitWorkspaceAuthoringOverlayButton *buttons,
                                                           uint32_t button_count,
                                                           KitWorkspaceAuthoringOverlayButtonId hover_id,
                                                           KitWorkspaceAuthoringOverlayButtonId pressed_id);

int kit_workspace_authoring_ui_pane_overlay_visible(int layout_mode,
                                                    int authoring_layout_mode_value,
                                                    int overlay_mode,
                                                    int font_theme_overlay_mode_value);

CoreResult kit_workspace_authoring_ui_push_surface_clear(KitRenderContext *render_ctx,
                                                         KitRenderFrame *frame,
                                                         CoreThemeColorToken token,
                                                         KitRenderColor fallback);

CoreResult kit_workspace_authoring_ui_draw_splitter_preview(KitRenderContext *render_ctx,
                                                            KitRenderFrame *frame,
                                                            int active,
                                                            int vertical_line,
                                                            float axis_coord,
                                                            float range_start,
                                                            float range_end);

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
                                             int pending_apply);

void kit_workspace_authoring_ui_submit_frame(
    void *host_context,
    const KitWorkspaceAuthoringRenderDeriveFrame *derive,
    KitWorkspaceAuthoringRenderSubmitDrawFn draw_scene,
    KitWorkspaceAuthoringRenderSubmitRebuildRequiredFn rebuild_required,
    KitWorkspaceAuthoringRenderSubmitAcknowledgeRebuildFn acknowledge_rebuild,
    KitWorkspaceAuthoringRenderSubmitOutcome *outcome);

#ifdef __cplusplus
}
#endif

#endif
