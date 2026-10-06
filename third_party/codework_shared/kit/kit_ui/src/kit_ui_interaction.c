#include "kit_ui_interaction.h"

#include <math.h>
#include <string.h>

static int contains(KitRenderRect r, float x, float y) {
    return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

static int find_control(const KitUiInteractionControl *controls, uint32_t count, uint32_t id) {
    for (uint32_t i = 0; i < count; ++i) {
        if (controls[i].id == id && controls[i].enabled) return (int)i;
    }
    return -1;
}

void kit_ui_interaction_reset(KitUiInteractionContext *ctx) {
    if (ctx) memset(ctx, 0, sizeof(*ctx));
}

static void cancel_press(KitUiInteractionContext *ctx) {
    ctx->captured_id = 0u;
    ctx->key_owner_id = 0u;
    /* Swallow the matching key-up after cancellation; it must not reach an
     * unrelated shortcut after focus moves. Reset clears it on scope loss. */
}

CoreResult kit_ui_interaction_route(KitUiInteractionContext *ctx,
                                    const KitUiInteractionControl *controls,
                                    uint32_t count,
                                    const KitUiInteractionEvent *event,
                                    KitUiInteractionResult *out_result) {
    int index;
    if (out_result) memset(out_result, 0, sizeof(*out_result));
    if (!ctx || !event || !out_result || (count && !controls) ||
        count > KIT_UI_INTERACTION_CONTROL_MAX ||
        event->type < KIT_UI_INTERACTION_SYNC || event->type > KIT_UI_INTERACTION_CANCEL ||
        event->key < KIT_UI_INTERACTION_KEY_NONE || event->key > KIT_UI_INTERACTION_KEY_ESCAPE) {
        return (CoreResult){CORE_ERR_INVALID_ARG, "invalid interaction event or control list"};
    }
    /* Validate before mutating ownership. Invalid geometry never turns into an
     * implicit click, and callers can repair/retry the same event. */
    for (uint32_t i = 0; i < count; ++i) {
        KitRenderRect r = controls[i].bounds;
        if (!controls[i].id || !isfinite(r.x) || !isfinite(r.y) ||
            !isfinite(r.width) || !isfinite(r.height) || r.width <= 0 || r.height <= 0 ||
            !isfinite(r.x + r.width) || !isfinite(r.y + r.height)) {
            return (CoreResult){CORE_ERR_INVALID_ARG, "invalid interaction control geometry or ID"};
        }
        for (uint32_t j = 0; j < i; ++j) {
            if (controls[i].id == controls[j].id)
                return (CoreResult){CORE_ERR_INVALID_ARG, "duplicate interaction control ID"};
        }
    }
    if (event->type >= KIT_UI_INTERACTION_POINTER_MOVE &&
        event->type <= KIT_UI_INTERACTION_POINTER_UP &&
        (!isfinite(event->x) || !isfinite(event->y))) {
        return (CoreResult){CORE_ERR_INVALID_ARG, "invalid interaction pointer coordinate"};
    }
    if (find_control(controls, count, ctx->focused_id) < 0) ctx->focused_id = 0u;
    if (find_control(controls, count, ctx->captured_id) < 0) ctx->captured_id = 0u;
    if (find_control(controls, count, ctx->key_owner_id) < 0) {
        /* Keep armed_key until key-up to prevent a released removed control
         * from falling through to an app shortcut. */
        ctx->key_owner_id = 0u;
    }
    if (event->type == KIT_UI_INTERACTION_CANCEL) {
        out_result->consumed = ctx->pointer_owned || ctx->armed_key != KIT_UI_INTERACTION_KEY_NONE;
        kit_ui_interaction_reset(ctx);
        return core_result_ok();
    }
    if (event->type >= KIT_UI_INTERACTION_POINTER_MOVE && event->type <= KIT_UI_INTERACTION_POINTER_UP) {
        ctx->pointer_x = event->x;
        ctx->pointer_y = event->y;
        ctx->pointer_known = 1;
    }
    switch (event->type) {
        case KIT_UI_INTERACTION_POINTER_DOWN:
            if (ctx->pointer_owned) {
                out_result->consumed = 1;
                break;
            }
            cancel_press(ctx);
            ctx->focused_id = 0u;
            /* Last painted control wins overlap; a disabled top control blocks
             * click-through, but cannot acquire focus or activate. */
            for (uint32_t i = count; i > 0u; --i) {
                if (!contains(controls[i - 1u].bounds, event->x, event->y)) continue;
                out_result->consumed = 1;
                ctx->pointer_owned = 1;
                if (controls[i - 1u].enabled) {
                    ctx->focused_id = controls[i - 1u].id;
                    ctx->captured_id = controls[i - 1u].id;
                }
                break;
            }
            break;
        case KIT_UI_INTERACTION_POINTER_MOVE:
            out_result->consumed = ctx->pointer_owned;
            break;
        case KIT_UI_INTERACTION_POINTER_UP:
            out_result->consumed = ctx->pointer_owned;
            index = find_control(controls, count, ctx->captured_id);
            if (ctx->pointer_owned && index >= 0 && contains(controls[index].bounds, event->x, event->y))
                out_result->activated_id = ctx->captured_id;
            ctx->pointer_owned = 0;
            ctx->captured_id = 0u;
            break;
        case KIT_UI_INTERACTION_KEY_DOWN:
            if (event->modifiers & (KIT_UI_INTERACTION_MOD_CTRL | KIT_UI_INTERACTION_MOD_ALT | KIT_UI_INTERACTION_MOD_GUI)) break;
            if (event->key == KIT_UI_INTERACTION_KEY_TAB) {
                out_result->consumed = 1;
                if (event->repeat) break;
                cancel_press(ctx);
                index = find_control(controls, count, ctx->focused_id);
                int backwards = (event->modifiers & KIT_UI_INTERACTION_MOD_SHIFT) != 0;
                if (index < 0) index = backwards ? 0 : (int)count - 1;
                for (uint32_t i = 0; i < count; ++i) {
                    index = backwards ? (index + (int)count - 1) % (int)count : (index + 1) % (int)count;
                    if (controls[index].enabled) { ctx->focused_id = controls[index].id; break; }
                }
            } else if (event->key == KIT_UI_INTERACTION_KEY_ESCAPE) {
                out_result->consumed = ctx->pointer_owned || ctx->armed_key != KIT_UI_INTERACTION_KEY_NONE;
                cancel_press(ctx);
            } else if ((event->key == KIT_UI_INTERACTION_KEY_ENTER || event->key == KIT_UI_INTERACTION_KEY_SPACE) &&
                       event->modifiers == 0u && !ctx->pointer_owned && ctx->focused_id) {
                out_result->consumed = 1;
                if (!event->repeat && ctx->armed_key == KIT_UI_INTERACTION_KEY_NONE) {
                    ctx->key_owner_id = ctx->focused_id;
                    ctx->armed_key = event->key;
                }
            }
            break;
        case KIT_UI_INTERACTION_KEY_UP:
            if (ctx->armed_key != KIT_UI_INTERACTION_KEY_NONE && ctx->armed_key == event->key) {
                out_result->consumed = 1;
                if (event->modifiers == 0u && ctx->key_owner_id && ctx->key_owner_id == ctx->focused_id &&
                    find_control(controls, count, ctx->key_owner_id) >= 0)
                    out_result->activated_id = ctx->key_owner_id;
                ctx->key_owner_id = 0u;
                ctx->armed_key = KIT_UI_INTERACTION_KEY_NONE;
            }
            break;
        default: break;
    }
    return core_result_ok();
}

KitUiButtonState kit_ui_interaction_button_state(const KitUiInteractionContext *ctx,
                                                 const KitUiInteractionControl *control,
                                                 int selected) {
    KitUiButtonState state = {0};
    state.selected = selected;
    state.disabled = !control || !control->enabled;
    if (!ctx || state.disabled) return state;
    state.focused = ctx->focused_id == control->id;
    state.hovered = ctx->pointer_known && contains(control->bounds, ctx->pointer_x, ctx->pointer_y) &&
                    (!ctx->pointer_owned || ctx->captured_id == control->id);
    state.pressed = (ctx->captured_id == control->id && state.hovered) ||
                    (ctx->key_owner_id == control->id && ctx->armed_key != KIT_UI_INTERACTION_KEY_NONE);
    return state;
}

int kit_ui_interaction_focus_marker(const KitUiInteractionContext *ctx,
                                    const KitUiInteractionControl *controls,
                                    uint32_t count,
                                    KitRenderRect *out_marker) {
    if (!ctx || !controls || !out_marker || count > KIT_UI_INTERACTION_CONTROL_MAX) return 0;
    int index = find_control(controls, count, ctx->focused_id);
    if (index < 0) return 0;
    KitRenderRect r = controls[index].bounds;
    float inset = fminf(6.0f, r.width * 0.2f);
    float thickness = fminf(2.0f, r.height * 0.1f);
    float bottom_padding = fminf(2.0f, r.height * 0.2f);
    *out_marker = (KitRenderRect){r.x + inset, r.y + r.height - thickness - bottom_padding,
                                 r.width - inset * 2.0f, thickness};
    return 1;
}

CoreResult kit_ui_interaction_draw_focus(KitRenderFrame *frame,
                                         const KitUiInteractionContext *ctx,
                                         const KitUiInteractionControl *controls,
                                         uint32_t count,
                                         KitRenderColor color) {
    KitRenderRect marker;
    if (!frame) return (CoreResult){CORE_ERR_INVALID_ARG, "missing focus draw frame"};
    if (!kit_ui_interaction_focus_marker(ctx, controls, count, &marker)) return core_result_ok();
    return kit_render_push_rect(frame, &(KitRenderRectCommand){marker, 0.0f, color, kit_render_identity_transform()});
}
