#include "kit_workspace_authoring_ui.h"

#include "kit_render.h"

static uint8_t kit_workspace_authoring_ui_u8_clamp(int value) {
    if (value < 0) {
        return 0u;
    }
    if (value > 255) {
        return 255u;
    }
    return (uint8_t)value;
}

static KitRenderColor kit_workspace_authoring_ui_color_with_alpha(KitRenderColor color, uint8_t alpha) {
    color.a = alpha;
    return color;
}

static KitRenderColor kit_workspace_authoring_ui_mix_color(KitRenderColor a, KitRenderColor b, float mix_01) {
    float t = mix_01;
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    return (KitRenderColor){
        kit_workspace_authoring_ui_u8_clamp((int)((a.r * (1.0f - t)) + (b.r * t))),
        kit_workspace_authoring_ui_u8_clamp((int)((a.g * (1.0f - t)) + (b.g * t))),
        kit_workspace_authoring_ui_u8_clamp((int)((a.b * (1.0f - t)) + (b.b * t))),
        kit_workspace_authoring_ui_u8_clamp((int)((a.a * (1.0f - t)) + (b.a * t)))
    };
}

static KitRenderColor kit_workspace_authoring_ui_theme_color(const KitRenderContext *render_ctx,
                                                             CoreThemeColorToken token,
                                                             KitRenderColor fallback) {
    KitRenderColor color = fallback;
    if (render_ctx) {
        (void)kit_render_resolve_theme_color(render_ctx, token, &color);
    }
    return color;
}

CoreResult kit_workspace_authoring_ui_draw_overlay_buttons(KitRenderContext *render_ctx,
                                                           KitRenderFrame *frame,
                                                           const KitWorkspaceAuthoringOverlayButton *buttons,
                                                           uint32_t button_count,
                                                           KitWorkspaceAuthoringOverlayButtonId hover_id,
                                                           KitWorkspaceAuthoringOverlayButtonId pressed_id) {
    CoreResult result = core_result_ok();
    uint32_t i;

    if (!render_ctx || !frame || !buttons) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid hud overlay draw request" };
    }

    for (i = 0u; i < button_count; ++i) {
        const KitWorkspaceAuthoringOverlayButton *b = &buttons[i];
        KitRenderTextMetrics text_metrics = {0};
        float text_x = 0.0f;
        float text_y = 0.0f;
        KitRenderColor surface_0 =
            kit_workspace_authoring_ui_theme_color(render_ctx, CORE_THEME_COLOR_SURFACE_0, (KitRenderColor){ 20, 24, 34, 255 });
        KitRenderColor surface_1 =
            kit_workspace_authoring_ui_theme_color(render_ctx, CORE_THEME_COLOR_SURFACE_1, (KitRenderColor){ 28, 34, 46, 255 });
        KitRenderColor surface_2 =
            kit_workspace_authoring_ui_theme_color(render_ctx, CORE_THEME_COLOR_SURFACE_2, (KitRenderColor){ 36, 48, 70, 255 });
        KitRenderColor accent =
            kit_workspace_authoring_ui_theme_color(render_ctx, CORE_THEME_COLOR_ACCENT_PRIMARY, (KitRenderColor){ 120, 160, 220, 255 });
        KitRenderColor fill = kit_workspace_authoring_ui_color_with_alpha(
            kit_workspace_authoring_ui_mix_color(surface_1, surface_2, 0.56f),
            222u);
        KitRenderColor border = kit_workspace_authoring_ui_color_with_alpha(
            kit_workspace_authoring_ui_mix_color(surface_2, accent, 0.20f),
            232u);
        CoreThemeColorToken label_color = CORE_THEME_COLOR_TEXT_PRIMARY;
        const float bw = 1.0f;
        KitRenderRect rect = { b->rect.x, b->rect.y, b->rect.width, b->rect.height };
        if (!b->visible) {
            continue;
        }
        if (!b->enabled) {
            fill = kit_workspace_authoring_ui_color_with_alpha(surface_0, 190u);
            border = kit_workspace_authoring_ui_color_with_alpha(surface_2, 200u);
            label_color = CORE_THEME_COLOR_TEXT_MUTED;
        } else if (pressed_id == b->id) {
            fill = kit_workspace_authoring_ui_color_with_alpha(
                kit_workspace_authoring_ui_mix_color(surface_2, accent, 0.62f),
                238u);
            border = kit_workspace_authoring_ui_color_with_alpha(accent, 246u);
        } else if (hover_id == b->id) {
            fill = kit_workspace_authoring_ui_color_with_alpha(
                kit_workspace_authoring_ui_mix_color(surface_2, accent, 0.40f),
                232u);
            border = kit_workspace_authoring_ui_color_with_alpha(
                kit_workspace_authoring_ui_mix_color(surface_2, accent, 0.70f),
                242u);
        }
        result = kit_render_push_rect(frame,
                                      &(KitRenderRectCommand){
                                          rect,
                                          3.0f,
                                          fill,
                                          kit_render_identity_transform()
                                      });
        if (result.code != CORE_OK) return result;
        result = kit_render_push_rect(frame,
                                      &(KitRenderRectCommand){
                                          (KitRenderRect){ rect.x, rect.y, rect.width, bw },
                                          0.0f,
                                          border,
                                          kit_render_identity_transform()
                                      });
        if (result.code != CORE_OK) return result;
        result = kit_render_push_rect(frame,
                                      &(KitRenderRectCommand){
                                          (KitRenderRect){ rect.x, rect.y + rect.height - bw, rect.width, bw },
                                          0.0f,
                                          border,
                                          kit_render_identity_transform()
                                      });
        if (result.code != CORE_OK) return result;
        result = kit_render_push_rect(frame,
                                      &(KitRenderRectCommand){
                                          (KitRenderRect){ rect.x, rect.y, bw, rect.height },
                                          0.0f,
                                          border,
                                          kit_render_identity_transform()
                                      });
        if (result.code != CORE_OK) return result;
        result = kit_render_push_rect(frame,
                                      &(KitRenderRectCommand){
                                          (KitRenderRect){ rect.x + rect.width - bw, rect.y, bw, rect.height },
                                          0.0f,
                                          border,
                                          kit_render_identity_transform()
                                      });
        if (result.code != CORE_OK) return result;
        result = kit_render_measure_text(render_ctx,
                                         CORE_FONT_ROLE_UI_MEDIUM,
                                         CORE_FONT_TEXT_SIZE_CAPTION,
                                         b->label,
                                         &text_metrics);
        if (result.code != CORE_OK) {
            text_metrics.width_px = 0.0f;
            text_metrics.height_px = 0.0f;
        }
        text_x = rect.x + ((rect.width - text_metrics.width_px) * 0.5f);
        text_y = rect.y + ((rect.height - text_metrics.height_px) * 0.5f);
        if (text_x < rect.x + 4.0f) {
            text_x = rect.x + 4.0f;
        }
        if (text_y < rect.y + 1.0f) {
            text_y = rect.y + 1.0f;
        }
        result = kit_render_push_text(
            frame,
            &(KitRenderTextCommand){
                (KitRenderVec2){ text_x, text_y },
                b->label,
                CORE_FONT_ROLE_UI_MEDIUM,
                CORE_FONT_TEXT_SIZE_CAPTION,
                label_color,
                kit_render_identity_transform()
            });
        if (result.code != CORE_OK) return result;
    }

    return core_result_ok();
}
