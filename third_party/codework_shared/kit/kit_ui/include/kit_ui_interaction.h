#ifndef KIT_UI_INTERACTION_H
#define KIT_UI_INTERACTION_H

#include "kit_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KIT_UI_INTERACTION_CONTROL_MAX 256u

/* IDs identify actions, not list positions. Zero means no owner. Controls and
 * their geometry are borrowed only for this call; the host owns their lifetime,
 * coordinate mapping, visibility, modal scope, and action dispatch. */
typedef struct KitUiInteractionControl {
    uint32_t id;
    KitRenderRect bounds;
    int enabled;
} KitUiInteractionControl;

typedef enum KitUiInteractionEventType {
    KIT_UI_INTERACTION_SYNC = 0,
    KIT_UI_INTERACTION_POINTER_MOVE,
    KIT_UI_INTERACTION_POINTER_DOWN,
    KIT_UI_INTERACTION_POINTER_UP,
    KIT_UI_INTERACTION_KEY_DOWN,
    KIT_UI_INTERACTION_KEY_UP,
    KIT_UI_INTERACTION_CANCEL
} KitUiInteractionEventType;

typedef enum KitUiInteractionKey {
    KIT_UI_INTERACTION_KEY_NONE = 0,
    KIT_UI_INTERACTION_KEY_TAB,
    KIT_UI_INTERACTION_KEY_ENTER,
    KIT_UI_INTERACTION_KEY_SPACE,
    KIT_UI_INTERACTION_KEY_ESCAPE
} KitUiInteractionKey;

enum {
    KIT_UI_INTERACTION_MOD_SHIFT = 1u << 0,
    KIT_UI_INTERACTION_MOD_CTRL = 1u << 1,
    KIT_UI_INTERACTION_MOD_ALT = 1u << 2,
    KIT_UI_INTERACTION_MOD_GUI = 1u << 3
};

typedef struct KitUiInteractionEvent {
    KitUiInteractionEventType type;
    float x, y;
    KitUiInteractionKey key;
    uint32_t modifiers;
    int repeat;
} KitUiInteractionEvent;

/* Optional retained interaction state, without a retained widget tree. Keep
 * one context per active input scope. Reset when its modal/surface owner changes. */
typedef struct KitUiInteractionContext {
    uint32_t focused_id;
    uint32_t captured_id;
    uint32_t key_owner_id;
    KitUiInteractionKey armed_key;
    float pointer_x, pointer_y;
    int pointer_known;
    int pointer_owned;
} KitUiInteractionContext;

typedef struct KitUiInteractionResult {
    uint32_t activated_id;
    int consumed;
} KitUiInteractionResult;

void kit_ui_interaction_reset(KitUiInteractionContext *ctx);
CoreResult kit_ui_interaction_route(KitUiInteractionContext *ctx,
                                    const KitUiInteractionControl *controls,
                                    uint32_t count,
                                    const KitUiInteractionEvent *event,
                                    KitUiInteractionResult *out_result);
KitUiButtonState kit_ui_interaction_button_state(const KitUiInteractionContext *ctx,
                                                 const KitUiInteractionControl *control,
                                                 int selected);
int kit_ui_interaction_focus_marker(const KitUiInteractionContext *ctx,
                                    const KitUiInteractionControl *controls,
                                    uint32_t count,
                                    KitRenderRect *out_marker);
CoreResult kit_ui_interaction_draw_focus(KitRenderFrame *frame,
                                         const KitUiInteractionContext *ctx,
                                         const KitUiInteractionControl *controls,
                                         uint32_t count,
                                         KitRenderColor color);

#ifdef __cplusplus
}
#endif
#endif
