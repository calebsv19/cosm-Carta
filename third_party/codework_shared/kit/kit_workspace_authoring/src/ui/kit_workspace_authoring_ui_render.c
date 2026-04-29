#include "kit_workspace_authoring_ui.h"

#define KIT_WORKSPACE_AUTHORING_SPLITTER_PREVIEW_THICKNESS_PX 3.0f

static KitRenderColor kit_workspace_authoring_ui_color_with_alpha(KitRenderColor color, uint8_t alpha) {
    color.a = alpha;
    return color;
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

CoreResult kit_workspace_authoring_ui_push_surface_clear(KitRenderContext *render_ctx,
                                                         KitRenderFrame *frame,
                                                         CoreThemeColorToken token,
                                                         KitRenderColor fallback) {
    KitRenderColor clear_color = fallback;
    if (!frame) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid frame clear request" };
    }
    clear_color = kit_workspace_authoring_ui_theme_color(render_ctx, token, clear_color);
    clear_color.a = 255u;
    return kit_render_push_clear(frame, clear_color);
}

CoreResult kit_workspace_authoring_ui_draw_splitter_preview(KitRenderContext *render_ctx,
                                                            KitRenderFrame *frame,
                                                            int active,
                                                            int vertical_line,
                                                            float axis_coord,
                                                            float range_start,
                                                            float range_end) {
    KitRenderRect preview_rect = {0};
    float span = range_end - range_start;

    if (!frame) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid splitter preview draw request" };
    }
    if (!active || span <= 0.0f) {
        return core_result_ok();
    }

    if (vertical_line) {
        preview_rect.x = axis_coord - (KIT_WORKSPACE_AUTHORING_SPLITTER_PREVIEW_THICKNESS_PX * 0.5f);
        preview_rect.y = range_start;
        preview_rect.width = KIT_WORKSPACE_AUTHORING_SPLITTER_PREVIEW_THICKNESS_PX;
        preview_rect.height = span;
    } else {
        preview_rect.x = range_start;
        preview_rect.y = axis_coord - (KIT_WORKSPACE_AUTHORING_SPLITTER_PREVIEW_THICKNESS_PX * 0.5f);
        preview_rect.width = span;
        preview_rect.height = KIT_WORKSPACE_AUTHORING_SPLITTER_PREVIEW_THICKNESS_PX;
    }

    return kit_render_push_rect(
        frame,
        &(KitRenderRectCommand){
            preview_rect,
            0.0f,
            kit_workspace_authoring_ui_color_with_alpha(
                kit_workspace_authoring_ui_theme_color(render_ctx,
                                                       CORE_THEME_COLOR_ACCENT_PRIMARY,
                                                       (KitRenderColor){ 196, 211, 255, 255 }),
                230u),
            kit_render_identity_transform()
        });
}
