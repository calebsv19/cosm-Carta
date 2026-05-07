#ifndef KIT_WORKSPACE_AUTHORING_UI_H
#define KIT_WORKSPACE_AUTHORING_UI_H

#include <stdint.h>

#include "core_base.h"
#include "core_font.h"
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

typedef enum KitWorkspaceAuthoringFontThemeButtonId {
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE = 0,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC = 1,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC = 2,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET = 3,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT = 4,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_IDE = 5,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_CUSTOM_STUB = 6,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_DAW_DEFAULT = 7,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_STANDARD_GREY = 8,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_MIDNIGHT_CONTRAST = 9,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_SOFT_LIGHT = 10,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_GREYSCALE = 11,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_CREATE_STUB = 12,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_EDIT_STUB = 13
} KitWorkspaceAuthoringFontThemeButtonId;

typedef enum KitWorkspaceAuthoringFontThemeActionType {
    KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_NONE = 0,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_DEC = 1,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_INC = 2,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_RESET = 3,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_FONT_PRESET = 4,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_THEME_PRESET = 5,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_CUSTOM_THEME_STATUS = 6
} KitWorkspaceAuthoringFontThemeActionType;

enum {
    KIT_WORKSPACE_AUTHORING_FONT_THEME_FONT_PRESET_BUTTON_COUNT = 3,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_THEME_PRESET_BUTTON_COUNT = 5,
    KIT_WORKSPACE_AUTHORING_FONT_THEME_CUSTOM_THEME_BUTTON_COUNT = 2
};

typedef struct KitWorkspaceAuthoringFontThemeLayout {
    KitRenderRect panel;
    KitRenderRect font_preset_section;
    KitRenderRect text_size_section;
    KitRenderRect theme_preset_section;
    KitRenderRect custom_theme_section;
    KitRenderRect text_size_dec_button;
    KitRenderRect text_size_value_chip;
    KitRenderRect text_size_inc_button;
    KitRenderRect text_size_reset_button;
    KitRenderRect font_preset_buttons[KIT_WORKSPACE_AUTHORING_FONT_THEME_FONT_PRESET_BUTTON_COUNT];
    KitRenderRect theme_preset_buttons[KIT_WORKSPACE_AUTHORING_FONT_THEME_THEME_PRESET_BUTTON_COUNT];
    KitRenderRect custom_theme_buttons[KIT_WORKSPACE_AUTHORING_FONT_THEME_CUSTOM_THEME_BUTTON_COUNT];
    uint32_t font_preset_button_count;
    uint32_t theme_preset_button_count;
    uint32_t custom_theme_button_count;
} KitWorkspaceAuthoringFontThemeLayout;

typedef struct KitWorkspaceAuthoringFontThemeAction {
    KitWorkspaceAuthoringFontThemeActionType type;
    KitWorkspaceAuthoringFontThemeButtonId button_id;
    CoreFontPresetId font_preset_id;
    CoreThemePresetId theme_preset_id;
    const char *custom_status_text;
} KitWorkspaceAuthoringFontThemeAction;

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

int kit_workspace_authoring_ui_font_theme_build_layout(const KitRenderContext *render_ctx,
                                                       int width,
                                                       int height,
                                                       KitWorkspaceAuthoringFontThemeLayout *out_layout);

KitWorkspaceAuthoringFontThemeButtonId kit_workspace_authoring_ui_font_theme_hit_button(
    const KitWorkspaceAuthoringFontThemeLayout *layout,
    float x,
    float y);

const char *kit_workspace_authoring_ui_font_theme_button_label(
    KitWorkspaceAuthoringFontThemeButtonId button_id);

uint8_t kit_workspace_authoring_ui_font_theme_button_enabled(
    KitWorkspaceAuthoringFontThemeButtonId button_id);

int kit_workspace_authoring_ui_font_theme_button_font_preset_id(
    KitWorkspaceAuthoringFontThemeButtonId button_id,
    CoreFontPresetId *out_font_id);

int kit_workspace_authoring_ui_font_theme_button_theme_preset_id(
    KitWorkspaceAuthoringFontThemeButtonId button_id,
    CoreThemePresetId *out_theme_id);

int kit_workspace_authoring_ui_font_theme_button_custom_theme_status(
    KitWorkspaceAuthoringFontThemeButtonId button_id,
    const char **out_status_text);

KitWorkspaceAuthoringFontThemeAction kit_workspace_authoring_ui_font_theme_action_for_button(
    KitWorkspaceAuthoringFontThemeButtonId button_id);

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
