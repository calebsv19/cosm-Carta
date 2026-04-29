#include "kit_workspace_authoring_ui.h"

#include <math.h>

static int kit_workspace_authoring_ui_point_in_rect(CorePaneRect rect, float x, float y) {
    if (x < rect.x || y < rect.y) return 0;
    if (x > rect.x + rect.width || y > rect.y + rect.height) return 0;
    return 1;
}

uint32_t kit_workspace_authoring_ui_build_overlay_buttons(int viewport_width,
                                                          int authoring_active,
                                                          int pane_overlay_active,
                                                          KitWorkspaceAuthoringOverlayButton *out_buttons,
                                                          uint32_t cap) {
    uint32_t count = 0u;
    float x = (float)viewport_width - 14.0f;
    const float y = 6.0f;
    const float h = 18.0f;
    const float gap = 6.0f;
    float mode_w = 118.0f;

    if (!out_buttons || cap == 0u) {
        return 0u;
    }

    if (count < cap) {
        KitWorkspaceAuthoringOverlayButton b = {0};
        b.id = KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE;
        b.label = "Enter Authoring";
        if (authoring_active) {
            b.label = pane_overlay_active ? "Overlay: Pane" : "Overlay: Font/Theme";
            mode_w = 132.0f;
        }
        b.visible = 1u;
        b.enabled = 1u;
        b.rect = (CorePaneRect){ x - mode_w, y, mode_w, h };
        x = b.rect.x - gap;
        out_buttons[count++] = b;
    }

    if (authoring_active && count < cap) {
        KitWorkspaceAuthoringOverlayButton b = {0};
        b.id = KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY;
        b.label = "Apply";
        b.visible = 1u;
        b.enabled = 1u;
        b.rect = (CorePaneRect){ x - 50.0f, y, 50.0f, h };
        x = b.rect.x - gap;
        out_buttons[count++] = b;
    }

    if (authoring_active && count < cap) {
        KitWorkspaceAuthoringOverlayButton b = {0};
        b.id = KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL;
        b.label = "Cancel";
        b.visible = 1u;
        b.enabled = 1u;
        b.rect = (CorePaneRect){ x - 56.0f, y, 56.0f, h };
        x = b.rect.x - gap;
        out_buttons[count++] = b;
    }

    if (authoring_active && pane_overlay_active && count < cap) {
        KitWorkspaceAuthoringOverlayButton b = {0};
        b.id = KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD;
        b.label = "+Pane";
        b.visible = 1u;
        b.enabled = 1u;
        b.rect = (CorePaneRect){ x - 56.0f, y, 56.0f, h };
        out_buttons[count++] = b;
    }

    return count;
}

KitWorkspaceAuthoringOverlayButtonId kit_workspace_authoring_ui_overlay_hit_test(
    const KitWorkspaceAuthoringOverlayButton *buttons,
    uint32_t count,
    float x,
    float y) {
    uint32_t i;
    if (!buttons) {
        return KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE;
    }
    for (i = 0u; i < count; ++i) {
        if (!buttons[i].visible || !buttons[i].enabled) {
            continue;
        }
        if (kit_workspace_authoring_ui_point_in_rect(buttons[i].rect, x, y)) {
            return buttons[i].id;
        }
    }
    return KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE;
}

const char *kit_workspace_authoring_ui_drop_intent_label(KitWorkspaceAuthoringDropIntent intent) {
    switch (intent) {
        case KIT_WORKSPACE_AUTHORING_DROP_INTENT_LEFT:
            return "LEFT";
        case KIT_WORKSPACE_AUTHORING_DROP_INTENT_RIGHT:
            return "RIGHT";
        case KIT_WORKSPACE_AUTHORING_DROP_INTENT_TOP:
            return "TOP";
        case KIT_WORKSPACE_AUTHORING_DROP_INTENT_BOTTOM:
            return "BOTTOM";
        case KIT_WORKSPACE_AUTHORING_DROP_INTENT_NONE:
        default:
            return "NONE";
    }
}

KitWorkspaceAuthoringDropIntent kit_workspace_authoring_ui_drop_intent_from_point(CorePaneRect rect,
                                                                                   float x,
                                                                                   float y,
                                                                                   CorePaneRect *out_ghost_rect) {
    float inset = 1.0f;
    float edge_band = 0.0f;
    float left_d = 0.0f;
    float right_d = 0.0f;
    float top_d = 0.0f;
    float bottom_d = 0.0f;
    float best = 0.0f;
    KitWorkspaceAuthoringDropIntent intent = KIT_WORKSPACE_AUTHORING_DROP_INTENT_NONE;

    if (!kit_workspace_authoring_ui_point_in_rect(rect, x, y)) {
        return KIT_WORKSPACE_AUTHORING_DROP_INTENT_NONE;
    }
    edge_band = fminf(fmaxf(fminf(rect.width, rect.height) * 0.25f, 24.0f), 96.0f);
    left_d = x - rect.x;
    right_d = (rect.x + rect.width) - x;
    top_d = y - rect.y;
    bottom_d = (rect.y + rect.height) - y;

    if (left_d <= edge_band) {
        intent = KIT_WORKSPACE_AUTHORING_DROP_INTENT_LEFT;
        best = left_d;
    }
    if (right_d <= edge_band && (intent == KIT_WORKSPACE_AUTHORING_DROP_INTENT_NONE || right_d < best)) {
        intent = KIT_WORKSPACE_AUTHORING_DROP_INTENT_RIGHT;
        best = right_d;
    }
    if (top_d <= edge_band && (intent == KIT_WORKSPACE_AUTHORING_DROP_INTENT_NONE || top_d < best)) {
        intent = KIT_WORKSPACE_AUTHORING_DROP_INTENT_TOP;
        best = top_d;
    }
    if (bottom_d <= edge_band && (intent == KIT_WORKSPACE_AUTHORING_DROP_INTENT_NONE || bottom_d < best)) {
        intent = KIT_WORKSPACE_AUTHORING_DROP_INTENT_BOTTOM;
    }

    if (!out_ghost_rect) {
        return intent;
    }
    *out_ghost_rect = (CorePaneRect){
        rect.x + inset, rect.y + inset, rect.width - (inset * 2.0f), rect.height - (inset * 2.0f)
    };
    switch (intent) {
        case KIT_WORKSPACE_AUTHORING_DROP_INTENT_LEFT:
            out_ghost_rect->width *= 0.5f;
            break;
        case KIT_WORKSPACE_AUTHORING_DROP_INTENT_RIGHT:
            out_ghost_rect->x += out_ghost_rect->width * 0.5f;
            out_ghost_rect->width *= 0.5f;
            break;
        case KIT_WORKSPACE_AUTHORING_DROP_INTENT_TOP:
            out_ghost_rect->height *= 0.5f;
            break;
        case KIT_WORKSPACE_AUTHORING_DROP_INTENT_BOTTOM:
            out_ghost_rect->y += out_ghost_rect->height * 0.5f;
            out_ghost_rect->height *= 0.5f;
            break;
        case KIT_WORKSPACE_AUTHORING_DROP_INTENT_NONE:
        default:
            break;
    }
    return intent;
}
