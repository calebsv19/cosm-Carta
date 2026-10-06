#include "kit_workspace_authoring_ui.h"

#include "kit_ui.h"

#include <math.h>

CoreResult kit_workspace_authoring_ui_draw_overlay_buttons(KitRenderContext *render_ctx,
                                                           KitRenderFrame *frame,
                                                           const KitWorkspaceAuthoringOverlayButton *buttons,
                                                           uint32_t button_count,
                                                           KitWorkspaceAuthoringOverlayButtonId hover_id,
                                                           KitWorkspaceAuthoringOverlayButtonId pressed_id) {
    KitUiContext ui_ctx;
    KitUiButtonAppearance appearance;
    CoreResult result;
    uint32_t i;

    if (!render_ctx || !frame || !buttons) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid hud overlay draw request" };
    }
    result = kit_ui_context_init(&ui_ctx, render_ctx);
    if (result.code != CORE_OK) return result;
    (void)kit_ui_button_appearance_preset(KIT_UI_BUTTON_APPEARANCE_COMPACT_ROUNDED,
                                         &appearance);

    for (i = 0u; i < button_count; ++i) {
        const KitWorkspaceAuthoringOverlayButton *b = &buttons[i];
        KitUiButtonSpec spec;
        KitUiButtonLayout text_layout;
        KitRenderTextMetrics text_metrics = {0};
        KitRenderRect rect = { b->rect.x, b->rect.y, b->rect.width, b->rect.height };
        float text_offset_x;

        if (!b->visible) continue;
        if (!b->label ||
            !isfinite(rect.x) || !isfinite(rect.y) ||
            !isfinite(rect.width) || !isfinite(rect.height) ||
            rect.width <= 0.0f || rect.height <= 0.0f) {
            return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid hud overlay draw request" };
        }

        kit_ui_button_spec_init(&spec, b->label);
        spec.state.disabled = !b->enabled;
        spec.state.hovered = hover_id == b->id;
        spec.state.pressed = pressed_id == b->id;

        result = kit_render_measure_text(render_ctx,
                                         CORE_FONT_ROLE_UI_MEDIUM,
                                         CORE_FONT_TEXT_SIZE_CAPTION,
                                         b->label,
                                         &text_metrics);
        if (result.code != CORE_OK) text_metrics.width_px = 0.0f;
        text_offset_x = fmaxf((rect.width - text_metrics.width_px) * 0.5f, 4.0f);
        /* KitRender text uses a vertical midpoint, not a top-left glyph origin. */
        kit_ui_button_layout_init(&text_layout, text_offset_x, rect.height * 0.5f);
        result = kit_ui_draw_button_spec_appearance_custom(&ui_ctx, frame, rect, &spec,
                                                           &text_layout, &appearance,
                                                           CORE_FONT_ROLE_UI_MEDIUM,
                                                           CORE_FONT_TEXT_SIZE_CAPTION);
        if (result.code != CORE_OK) return result;
    }

    return core_result_ok();
}
