#ifndef KIT_WORKSPACE_AUTHORING_H
#define KIT_WORKSPACE_AUTHORING_H

#include <stdint.h>

#include "core_base.h"
#include "core_pane.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum KitWorkspaceAuthoringModBits {
    KIT_WORKSPACE_AUTHORING_MOD_SHIFT = 1u << 0,
    KIT_WORKSPACE_AUTHORING_MOD_ALT = 1u << 1,
    KIT_WORKSPACE_AUTHORING_MOD_CTRL = 1u << 2,
    KIT_WORKSPACE_AUTHORING_MOD_GUI = 1u << 3
} KitWorkspaceAuthoringModBits;

typedef enum KitWorkspaceAuthoringKey {
    KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN = 0,
    KIT_WORKSPACE_AUTHORING_KEY_TAB = 1,
    KIT_WORKSPACE_AUTHORING_KEY_ENTER = 2,
    KIT_WORKSPACE_AUTHORING_KEY_ESCAPE = 3,
    KIT_WORKSPACE_AUTHORING_KEY_H = 4,
    KIT_WORKSPACE_AUTHORING_KEY_V = 5,
    KIT_WORKSPACE_AUTHORING_KEY_X = 6,
    KIT_WORKSPACE_AUTHORING_KEY_BACKSPACE = 7,
    KIT_WORKSPACE_AUTHORING_KEY_R = 8,
    KIT_WORKSPACE_AUTHORING_KEY_DIGIT_0 = 9,
    KIT_WORKSPACE_AUTHORING_KEY_DIGIT_1 = 10,
    KIT_WORKSPACE_AUTHORING_KEY_DIGIT_2 = 11,
    KIT_WORKSPACE_AUTHORING_KEY_DIGIT_3 = 12,
    KIT_WORKSPACE_AUTHORING_KEY_DIGIT_4 = 13,
    KIT_WORKSPACE_AUTHORING_KEY_DIGIT_5 = 14,
    KIT_WORKSPACE_AUTHORING_KEY_DIGIT_6 = 15,
    KIT_WORKSPACE_AUTHORING_KEY_C = 16,
    KIT_WORKSPACE_AUTHORING_KEY_Z = 17
} KitWorkspaceAuthoringKey;

typedef CoreResult (*KitWorkspaceAuthoringExecuteActionFn)(void *host_context,
                                                           const char *action_id,
                                                           uint32_t *io_selected_pane_id,
                                                           int *io_pending_apply);
typedef void (*KitWorkspaceAuthoringClosePickerFn)(void *host_context);

typedef struct KitWorkspaceAuthoringActionHooks {
    KitWorkspaceAuthoringExecuteActionFn execute_action;
    KitWorkspaceAuthoringClosePickerFn close_picker;
} KitWorkspaceAuthoringActionHooks;

typedef int (*KitWorkspaceAuthoringClampStepFn)(void *host_context, int target_step);
typedef int (*KitWorkspaceAuthoringGetStepFn)(void *host_context);
typedef int (*KitWorkspaceAuthoringGetIncrementFn)(void *host_context);
typedef int (*KitWorkspaceAuthoringGetDefaultFn)(void *host_context);
typedef CoreResult (*KitWorkspaceAuthoringSetStepFn)(void *host_context, int step);
typedef void (*KitWorkspaceAuthoringPersistStepFn)(void *host_context);

typedef struct KitWorkspaceAuthoringTextStepHooks {
    KitWorkspaceAuthoringClampStepFn clamp_step;
    KitWorkspaceAuthoringGetStepFn current_step;
    KitWorkspaceAuthoringGetIncrementFn step_increment;
    KitWorkspaceAuthoringGetDefaultFn default_step;
    KitWorkspaceAuthoringSetStepFn set_step;
    KitWorkspaceAuthoringPersistStepFn persist_step;
} KitWorkspaceAuthoringTextStepHooks;

CorePaneRect kit_workspace_authoring_root_bounds(int width, int height);

int kit_workspace_authoring_pane_overlay_active(int layout_mode,
                                                int authoring_layout_mode_value,
                                                int overlay_mode,
                                                int pane_overlay_mode_value);

CoreResult kit_workspace_authoring_execute_action(void *host_context,
                                                  const KitWorkspaceAuthoringActionHooks *hooks,
                                                  const char *action_id,
                                                  uint32_t *io_selected_pane_id,
                                                  int *io_pending_apply,
                                                  int layout_mode,
                                                  int authoring_layout_mode_value,
                                                  int overlay_mode,
                                                  int pane_overlay_mode_value);

CoreResult kit_workspace_authoring_apply_text_size_step(void *host_context,
                                                        const KitWorkspaceAuthoringTextStepHooks *hooks,
                                                        int target_step,
                                                        int *out_applied_step);

CoreResult kit_workspace_authoring_adjust_text_size_step(void *host_context,
                                                         const KitWorkspaceAuthoringTextStepHooks *hooks,
                                                         int direction,
                                                         int *out_applied_step);

CoreResult kit_workspace_authoring_reset_text_size_step(void *host_context,
                                                        const KitWorkspaceAuthoringTextStepHooks *hooks,
                                                        int *out_applied_step);

const char *kit_workspace_authoring_trigger_from_key(KitWorkspaceAuthoringKey key, uint32_t mod_bits);

int kit_workspace_authoring_entry_chord_pressed(KitWorkspaceAuthoringKey key,
                                                uint32_t mod_bits,
                                                int key_c_down,
                                                int key_v_down);

#ifdef __cplusplus
}
#endif

#endif
