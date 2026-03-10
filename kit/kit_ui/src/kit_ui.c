/*
 * kit_ui.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "kit_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static CoreResult kit_ui_invalid(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static CoreThemeColorToken ui_surface_token_for_state(KitUiWidgetState state) {
    switch (state) {
        case KIT_UI_STATE_HOVERED:
            return CORE_THEME_COLOR_SURFACE_1;
        case KIT_UI_STATE_ACTIVE:
            return CORE_THEME_COLOR_ACCENT_PRIMARY;
        case KIT_UI_STATE_DISABLED:
            return CORE_THEME_COLOR_SURFACE_0;
        case KIT_UI_STATE_NORMAL:
        default:
            return CORE_THEME_COLOR_SURFACE_2;
    }
}

static CoreThemeColorToken ui_text_token_for_state(KitUiWidgetState state) {
    if (state == KIT_UI_STATE_DISABLED) {
        return CORE_THEME_COLOR_TEXT_MUTED;
    }
    return CORE_THEME_COLOR_TEXT_PRIMARY;
}

static CoreResult ui_color(KitUiContext *ctx,
                           CoreThemeColorToken token,
                           KitRenderColor *out_color) {
    if (!ctx || !ctx->render_ctx) {
        return kit_ui_invalid("invalid ui context");
    }
    return kit_render_resolve_theme_color(ctx->render_ctx, token, out_color);
}

static float clampf_ui(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static int ui_estimate_char_width_px(CoreFontTextSizeTier text_tier) {
    switch (text_tier) {
        case CORE_FONT_TEXT_SIZE_HEADER:
            return 13;
        case CORE_FONT_TEXT_SIZE_TITLE:
            return 11;
        case CORE_FONT_TEXT_SIZE_PARAGRAPH:
            return 9;
        case CORE_FONT_TEXT_SIZE_CAPTION:
            return 8;
        case CORE_FONT_TEXT_SIZE_BASIC:
        default:
            return 9;
    }
}

static int ui_estimate_line_height_px(CoreFontTextSizeTier text_tier) {
    switch (text_tier) {
        case CORE_FONT_TEXT_SIZE_HEADER:
            return 28;
        case CORE_FONT_TEXT_SIZE_TITLE:
            return 22;
        case CORE_FONT_TEXT_SIZE_PARAGRAPH:
            return 18;
        case CORE_FONT_TEXT_SIZE_CAPTION:
            return 16;
        case CORE_FONT_TEXT_SIZE_BASIC:
        default:
            return 18;
    }
}

static CoreResult ui_measure_text_with_fallback(const KitUiContext *ctx,
                                                CoreFontRoleId font_role,
                                                CoreFontTextSizeTier text_tier,
                                                const char *text,
                                                KitRenderTextMetrics *out_metrics) {
    size_t len;
    int char_width;
    int line_height;
    CoreResult result;

    if (!text || !out_metrics) {
        return kit_ui_invalid("invalid text metrics request");
    }

    if (ctx && ctx->render_ctx) {
        result = kit_render_measure_text(ctx->render_ctx, font_role, text_tier, text, out_metrics);
        if (result.code == CORE_OK) {
            return core_result_ok();
        }
    }

    len = strlen(text);
    char_width = ui_estimate_char_width_px(text_tier);
    line_height = ui_estimate_line_height_px(text_tier);
    if (char_width < 1) {
        char_width = 8;
    }
    if (line_height < 1) {
        line_height = 16;
    }

    out_metrics->width_px = (float)((int)len * char_width);
    out_metrics->height_px = (float)line_height;
    return core_result_ok();
}

static void ui_copy_text(char *out_text, size_t out_text_cap, const char *source_text) {
    if (!out_text || out_text_cap == 0u) {
        return;
    }
    out_text[0] = '\0';
    if (!source_text) {
        return;
    }
    (void)snprintf(out_text, out_text_cap, "%s", source_text);
}

static CoreResult ui_ellipsize_text_to_width(const KitUiContext *ctx,
                                             const char *source_text,
                                             CoreFontRoleId font_role,
                                             CoreFontTextSizeTier text_tier,
                                             float max_width_px,
                                             char *out_text,
                                             size_t out_text_cap,
                                             KitRenderTextMetrics *out_metrics,
                                             int *out_truncated) {
    KitRenderTextMetrics metrics;
    KitRenderTextMetrics ellipsis_metrics;
    const char *ellipsis = "...";
    size_t len;
    size_t lo;
    size_t hi;
    size_t best = 0u;
    CoreResult result;
    char candidate[512];

    if (!source_text || !out_text || out_text_cap == 0u || !out_metrics || !out_truncated) {
        return kit_ui_invalid("invalid ellipsis request");
    }

    out_text[0] = '\0';
    out_metrics->width_px = 0.0f;
    out_metrics->height_px = 0.0f;
    *out_truncated = 0;

    result = ui_measure_text_with_fallback(ctx, font_role, text_tier, source_text, &metrics);
    if (result.code != CORE_OK) {
        return result;
    }
    if (metrics.width_px <= max_width_px) {
        ui_copy_text(out_text, out_text_cap, source_text);
        *out_metrics = metrics;
        return core_result_ok();
    }

    result = ui_measure_text_with_fallback(ctx, font_role, text_tier, ellipsis, &ellipsis_metrics);
    if (result.code != CORE_OK) {
        return result;
    }

    if (max_width_px <= 0.0f || ellipsis_metrics.width_px > max_width_px) {
        out_text[0] = '\0';
        out_metrics->height_px = metrics.height_px;
        *out_truncated = 1;
        return core_result_ok();
    }

    len = strlen(source_text);
    lo = 0u;
    hi = len;

    while (lo <= hi) {
        size_t mid = lo + ((hi - lo) / 2u);
        KitRenderTextMetrics candidate_metrics;
        size_t copy_len = mid;

        if (copy_len > sizeof(candidate) - 4u) {
            copy_len = sizeof(candidate) - 4u;
        }
        memcpy(candidate, source_text, copy_len);
        memcpy(candidate + copy_len, "...", 3u);
        candidate[copy_len + 3u] = '\0';

        result = ui_measure_text_with_fallback(ctx,
                                               font_role,
                                               text_tier,
                                               candidate,
                                               &candidate_metrics);
        if (result.code != CORE_OK) {
            return result;
        }

        if (candidate_metrics.width_px <= max_width_px) {
            best = copy_len;
            *out_metrics = candidate_metrics;
            lo = mid + 1u;
        } else {
            if (mid == 0u) {
                break;
            }
            hi = mid - 1u;
        }
    }

    if (out_text_cap <= 4u) {
        out_text[0] = '\0';
        out_metrics->height_px = metrics.height_px;
        *out_truncated = 1;
        return core_result_ok();
    }
    if (best > out_text_cap - 4u) {
        best = out_text_cap - 4u;
    }
    memcpy(out_text, source_text, best);
    memcpy(out_text + best, "...", 3u);
    out_text[best + 3u] = '\0';
    *out_truncated = 1;
    return core_result_ok();
}

static KitRenderRect ui_intersect_rect(KitRenderRect a, KitRenderRect b) {
    KitRenderRect out;
    float right_a;
    float right_b;
    float bottom_a;
    float bottom_b;
    float right;
    float bottom;

    out.x = a.x > b.x ? a.x : b.x;
    out.y = a.y > b.y ? a.y : b.y;

    right_a = a.x + a.width;
    right_b = b.x + b.width;
    bottom_a = a.y + a.height;
    bottom_b = b.y + b.height;

    right = right_a < right_b ? right_a : right_b;
    bottom = bottom_a < bottom_b ? bottom_a : bottom_b;

    out.width = right - out.x;
    out.height = bottom - out.y;
    if (out.width < 0.0f) out.width = 0.0f;
    if (out.height < 0.0f) out.height = 0.0f;
    return out;
}

static KitRenderRect ui_snap_clip_rect(KitRenderRect rect) {
    KitRenderRect out;
    float x0;
    float y0;
    float x1;
    float y1;

    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        out.x = floorf(rect.x);
        out.y = floorf(rect.y);
        out.width = 0.0f;
        out.height = 0.0f;
        return out;
    }

    x0 = floorf(rect.x);
    y0 = floorf(rect.y);
    x1 = ceilf(rect.x + rect.width);
    y1 = ceilf(rect.y + rect.height);

    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;

    out.x = x0;
    out.y = y0;
    out.width = x1 - x0;
    out.height = y1 - y0;
    return out;
}

void kit_ui_style_default(KitUiStyle *out_style) {
    if (!out_style) {
        return;
    }

    out_style->padding = 8.0f;
    out_style->gap = 8.0f;
    out_style->control_height = 32.0f;
    out_style->checkbox_size = 18.0f;
    out_style->slider_track_height = 6.0f;
    out_style->slider_knob_width = 14.0f;
}

CoreResult kit_ui_style_apply_theme_scale(KitUiContext *ctx) {
    const CoreThemeScale *scale;

    if (!ctx || !ctx->render_ctx) {
        return kit_ui_invalid("invalid ui context");
    }

    scale = &ctx->render_ctx->theme.scale;

    ctx->style.padding = clampf_ui(scale->spacing_sm, 4.0f, 16.0f);
    ctx->style.gap = clampf_ui(scale->spacing_md, 4.0f, 20.0f);
    ctx->style.control_height = clampf_ui(22.0f + scale->spacing_md, 24.0f, 44.0f);
    ctx->style.checkbox_size = clampf_ui(14.0f + scale->spacing_xs, 14.0f, 28.0f);
    ctx->style.slider_track_height = clampf_ui(2.0f + scale->spacing_xs, 4.0f, 10.0f);
    ctx->style.slider_knob_width = clampf_ui(10.0f + scale->spacing_xs, 12.0f, 22.0f);

    return core_result_ok();
}

CoreResult kit_ui_context_init(KitUiContext *ctx, KitRenderContext *render_ctx) {
    CoreResult result;

    if (!ctx || !render_ctx) {
        return kit_ui_invalid("invalid argument");
    }

    ctx->render_ctx = render_ctx;
    kit_ui_style_default(&ctx->style);
    kit_ui_clip_stack_reset(ctx);
    result = kit_ui_style_apply_theme_scale(ctx);
    if (result.code != CORE_OK) {
        return result;
    }
    return core_result_ok();
}

CoreResult kit_ui_stack_begin(KitUiStackLayout *layout,
                              KitUiAxis axis,
                              KitRenderRect bounds,
                              float gap) {
    if (!layout || bounds.width < 0.0f || bounds.height < 0.0f) {
        return kit_ui_invalid("invalid layout");
    }

    layout->axis = axis;
    layout->bounds = bounds;
    layout->cursor = 0.0f;
    layout->gap = gap < 0.0f ? 0.0f : gap;
    return core_result_ok();
}

CoreResult kit_ui_stack_next(KitUiStackLayout *layout,
                             float span,
                             float cross_span,
                             KitRenderRect *out_rect) {
    if (!layout || !out_rect || span < 0.0f || cross_span < 0.0f) {
        return kit_ui_invalid("invalid stack allocation");
    }

    if (layout->axis == KIT_UI_AXIS_VERTICAL) {
        out_rect->x = layout->bounds.x;
        out_rect->y = layout->bounds.y + layout->cursor;
        out_rect->width = cross_span > 0.0f ? cross_span : layout->bounds.width;
        out_rect->height = span;
    } else {
        out_rect->x = layout->bounds.x + layout->cursor;
        out_rect->y = layout->bounds.y;
        out_rect->width = span;
        out_rect->height = cross_span > 0.0f ? cross_span : layout->bounds.height;
    }

    layout->cursor += span + layout->gap;
    return core_result_ok();
}

void kit_ui_clip_stack_reset(KitUiContext *ctx) {
    if (!ctx) {
        return;
    }
    ctx->clip_depth = 0;
}

CoreResult kit_ui_clip_push(KitUiContext *ctx,
                            KitRenderFrame *frame,
                            KitRenderRect clip_rect) {
    KitRenderRect resolved;

    if (!ctx || !frame || clip_rect.width < 0.0f || clip_rect.height < 0.0f) {
        return kit_ui_invalid("invalid clip push");
    }
    if (ctx->clip_depth >= KIT_UI_CLIP_STACK_MAX) {
        return kit_ui_invalid("clip stack overflow");
    }

    resolved = clip_rect;
    if (ctx->clip_depth > 0) {
        resolved = ui_intersect_rect(ctx->clip_stack[ctx->clip_depth - 1], resolved);
    }
    resolved = ui_snap_clip_rect(resolved);

    ctx->clip_stack[ctx->clip_depth] = resolved;
    ctx->clip_depth += 1;
    return kit_render_push_set_clip(frame, resolved);
}

CoreResult kit_ui_clip_pop(KitUiContext *ctx, KitRenderFrame *frame) {
    if (!ctx || !frame) {
        return kit_ui_invalid("invalid clip pop");
    }
    if (ctx->clip_depth <= 0) {
        return kit_ui_invalid("clip stack underflow");
    }

    ctx->clip_depth -= 1;
    if (ctx->clip_depth <= 0) {
        ctx->clip_depth = 0;
        return kit_render_push_clear_clip(frame);
    }

    return kit_render_push_set_clip(frame, ctx->clip_stack[ctx->clip_depth - 1]);
}

int kit_ui_point_in_rect(KitRenderRect bounds, float x, float y) {
    if (x < bounds.x || y < bounds.y) return 0;
    if (x > bounds.x + bounds.width) return 0;
    if (y > bounds.y + bounds.height) return 0;
    return 1;
}

KitUiWidgetState kit_ui_state_for_input(KitRenderRect bounds,
                                        const KitUiInputState *input,
                                        int enabled) {
    if (!enabled) {
        return KIT_UI_STATE_DISABLED;
    }
    if (!input) {
        return KIT_UI_STATE_NORMAL;
    }
    if (!kit_ui_point_in_rect(bounds, input->mouse_x, input->mouse_y)) {
        return KIT_UI_STATE_NORMAL;
    }
    if (input->mouse_down) {
        return KIT_UI_STATE_ACTIVE;
    }
    return KIT_UI_STATE_HOVERED;
}

KitUiButtonResult kit_ui_eval_button(KitRenderRect bounds,
                                     const KitUiInputState *input,
                                     int enabled) {
    KitUiButtonResult result;

    result.state = kit_ui_state_for_input(bounds, input, enabled);
    result.clicked = 0;
    if (!enabled || !input) {
        return result;
    }
    if (input->mouse_released && kit_ui_point_in_rect(bounds, input->mouse_x, input->mouse_y)) {
        result.clicked = 1;
    }
    return result;
}

KitUiCheckboxResult kit_ui_eval_checkbox(KitRenderRect bounds,
                                         const KitUiInputState *input,
                                         int enabled,
                                         int checked) {
    KitUiCheckboxResult result;

    result.state = kit_ui_state_for_input(bounds, input, enabled);
    result.toggled = 0;
    result.checked = checked ? 1 : 0;
    if (!enabled || !input) {
        return result;
    }
    if (input->mouse_released && kit_ui_point_in_rect(bounds, input->mouse_x, input->mouse_y)) {
        result.toggled = 1;
        result.checked = result.checked ? 0 : 1;
    }
    return result;
}

KitUiSliderResult kit_ui_eval_slider(KitRenderRect bounds,
                                     const KitUiInputState *input,
                                     int enabled,
                                     float current_value_01) {
    KitUiSliderResult result;
    float value = current_value_01;

    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    result.state = kit_ui_state_for_input(bounds, input, enabled);
    result.changed = 0;
    result.value_01 = value;
    if (!enabled || !input || !input->mouse_down) {
        return result;
    }
    if (!kit_ui_point_in_rect(bounds, input->mouse_x, input->mouse_y)) {
        return result;
    }

    if (bounds.width <= 0.0f) {
        return result;
    }

    value = (input->mouse_x - bounds.x) / bounds.width;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    result.changed = 1;
    result.value_01 = value;
    return result;
}

KitUiScrollResult kit_ui_eval_scroll(KitRenderRect viewport_bounds,
                                     float current_offset_y,
                                     float content_height,
                                     float wheel_delta_y) {
    KitUiScrollResult result;
    float max_offset;

    result.changed = 0;
    result.offset_y = current_offset_y;

    if (content_height <= viewport_bounds.height || wheel_delta_y == 0.0f) {
        if (result.offset_y < 0.0f) result.offset_y = 0.0f;
        return result;
    }

    max_offset = content_height - viewport_bounds.height;
    result.offset_y -= wheel_delta_y * 24.0f;
    if (result.offset_y < 0.0f) result.offset_y = 0.0f;
    if (result.offset_y > max_offset) result.offset_y = max_offset;
    result.changed = 1;
    return result;
}

float kit_ui_scroll_content_height_top_anchor(int row_count,
                                              float row_height,
                                              float viewport_height) {
    float content_height;

    if (row_count <= 0 || row_height <= 0.0f) {
        return 0.0f;
    }

    content_height = (float)row_count * row_height;
    if (viewport_height > row_height) {
        content_height += viewport_height - row_height;
    }

    return content_height;
}

KitUiSegmentedResult kit_ui_eval_segmented(KitRenderRect bounds,
                                           const KitUiInputState *input,
                                           int enabled,
                                           int segment_count,
                                           int selected_index) {
    KitUiSegmentedResult result;
    float seg_width;
    int index;

    result.changed = 0;
    result.selected_index = selected_index;
    result.clicked_index = -1;

    if (!enabled || !input || segment_count <= 0 || bounds.width <= 0.0f) {
        return result;
    }
    if (!kit_ui_point_in_rect(bounds, input->mouse_x, input->mouse_y)) {
        return result;
    }

    seg_width = bounds.width / (float)segment_count;
    if (seg_width <= 0.0f) {
        return result;
    }

    index = (int)((input->mouse_x - bounds.x) / seg_width);
    if (index < 0) index = 0;
    if (index >= segment_count) index = segment_count - 1;

    if (input->mouse_released) {
        result.changed = (index != selected_index);
        result.selected_index = index;
        result.clicked_index = index;
    }
    return result;
}

CoreResult kit_ui_draw_label(KitUiContext *ctx,
                             KitRenderFrame *frame,
                             KitRenderRect bounds,
                             const char *text,
                             CoreThemeColorToken color_token) {
    return kit_ui_draw_label_custom(ctx,
                                    frame,
                                    bounds,
                                    text,
                                    color_token,
                                    CORE_FONT_ROLE_UI_REGULAR,
                                    CORE_FONT_TEXT_SIZE_BASIC);
}

CoreResult kit_ui_draw_label_custom(KitUiContext *ctx,
                                    KitRenderFrame *frame,
                                    KitRenderRect bounds,
                                    const char *text,
                                    CoreThemeColorToken color_token,
                                    CoreFontRoleId font_role,
                                    CoreFontTextSizeTier text_tier) {
    KitRenderTextCommand text_cmd;

    if (!ctx || !frame || !text) {
        return kit_ui_invalid("invalid label");
    }

    text_cmd.origin.x = bounds.x + ctx->style.padding;
    text_cmd.origin.y = bounds.y + (bounds.height * 0.5f);
    text_cmd.text = text;
    text_cmd.font_role = font_role;
    text_cmd.text_tier = text_tier;
    text_cmd.color_token = color_token;
    text_cmd.transform = kit_render_identity_transform();
    return kit_render_push_text(frame, &text_cmd);
}

CoreResult kit_ui_draw_button(KitUiContext *ctx,
                              KitRenderFrame *frame,
                              KitRenderRect bounds,
                              const char *label,
                              KitUiWidgetState state) {
    return kit_ui_draw_button_custom(ctx,
                                     frame,
                                     bounds,
                                     label,
                                     state,
                                     CORE_FONT_ROLE_UI_REGULAR,
                                     CORE_FONT_TEXT_SIZE_BASIC);
}

CoreResult kit_ui_draw_button_custom(KitUiContext *ctx,
                                     KitRenderFrame *frame,
                                     KitRenderRect bounds,
                                     const char *label,
                                     KitUiWidgetState state,
                                     CoreFontRoleId font_role,
                                     CoreFontTextSizeTier text_tier) {
    KitRenderColor color;
    CoreResult result;
    KitRenderRectCommand rect_cmd;

    if (!ctx || !frame || !label) {
        return kit_ui_invalid("invalid button");
    }

    result = ui_color(ctx, ui_surface_token_for_state(state), &color);
    if (result.code != CORE_OK) {
        return result;
    }

    rect_cmd.rect = bounds;
    rect_cmd.corner_radius = 6.0f;
    rect_cmd.color = color;
    rect_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_rect(frame, &rect_cmd);
    if (result.code != CORE_OK) {
        return result;
    }

    return kit_ui_draw_label_custom(ctx,
                                    frame,
                                    bounds,
                                    label,
                                    ui_text_token_for_state(state),
                                    font_role,
                                    text_tier);
}

CoreResult kit_ui_measure_text(const KitUiContext *ctx,
                               CoreFontRoleId font_role,
                               CoreFontTextSizeTier text_tier,
                               const char *text,
                               KitRenderTextMetrics *out_metrics) {
    return ui_measure_text_with_fallback(ctx, font_role, text_tier, text, out_metrics);
}

CoreResult kit_ui_fit_text_to_rect(const KitUiContext *ctx,
                                   const char *source_text,
                                   CoreFontRoleId font_role,
                                   const CoreFontTextSizeTier *tier_candidates,
                                   uint32_t tier_candidate_count,
                                   float max_width_px,
                                   float max_height_px,
                                   char *out_text,
                                   size_t out_text_cap,
                                   KitUiTextFitResult *out_fit) {
    static const CoreFontTextSizeTier k_default_tiers[] = {
        CORE_FONT_TEXT_SIZE_TITLE,
        CORE_FONT_TEXT_SIZE_PARAGRAPH,
        CORE_FONT_TEXT_SIZE_BASIC,
        CORE_FONT_TEXT_SIZE_CAPTION
    };
    const CoreFontTextSizeTier *tiers = tier_candidates;
    uint32_t tier_count = tier_candidate_count;
    uint32_t i;
    CoreResult result;
    KitRenderTextMetrics metrics;

    if (!source_text || !out_text || out_text_cap == 0u) {
        return kit_ui_invalid("invalid text fit request");
    }

    if (!tiers || tier_count == 0u) {
        tiers = k_default_tiers;
        tier_count = (uint32_t)(sizeof(k_default_tiers) / sizeof(k_default_tiers[0]));
    }

    if (max_width_px < 0.0f) {
        max_width_px = 0.0f;
    }
    if (max_height_px < 0.0f) {
        max_height_px = 0.0f;
    }

    ui_copy_text(out_text, out_text_cap, "");
    if (out_fit) {
        out_fit->text_tier = tiers[tier_count - 1u];
        out_fit->width_px = 0.0f;
        out_fit->height_px = 0.0f;
        out_fit->truncated = 0;
    }

    /* Prefer the largest candidate tier that fits without truncation. */
    for (i = 0u; i < tier_count; ++i) {
        CoreFontTextSizeTier tier = tiers[i];
        result = ui_measure_text_with_fallback(ctx, font_role, tier, source_text, &metrics);
        if (result.code != CORE_OK) {
            return result;
        }
        if (metrics.height_px <= max_height_px && metrics.width_px <= max_width_px) {
            ui_copy_text(out_text, out_text_cap, source_text);
            if (out_fit) {
                out_fit->text_tier = tier;
                out_fit->width_px = metrics.width_px;
                out_fit->height_px = metrics.height_px;
                out_fit->truncated = 0;
            }
            return core_result_ok();
        }
    }

    /* If no full fit exists, pick the smallest tier that fits height and ellipsize width.
       This avoids zoom-driven visual jumps where text appears larger after truncation. */
    for (i = tier_count; i > 0u; --i) {
        uint32_t tier_index = i - 1u;
        CoreFontTextSizeTier tier = tiers[tier_index];
        int truncated = 0;
        result = ui_measure_text_with_fallback(ctx, font_role, tier, "Ag", &metrics);
        if (result.code != CORE_OK) {
            return result;
        }
        if (metrics.height_px > max_height_px) {
            continue;
        }
        result = ui_ellipsize_text_to_width(ctx,
                                            source_text,
                                            font_role,
                                            tier,
                                            max_width_px,
                                            out_text,
                                            out_text_cap,
                                            &metrics,
                                            &truncated);
        if (result.code != CORE_OK) {
            return result;
        }
        if (out_fit) {
            out_fit->text_tier = tier;
            out_fit->width_px = metrics.width_px;
            out_fit->height_px = metrics.height_px;
            out_fit->truncated = truncated;
        }
        return core_result_ok();
    }

    /* Final fallback: smallest tier, width fit best effort. */
    {
        CoreFontTextSizeTier tier = tiers[tier_count - 1u];
        int truncated = 0;
        result = ui_ellipsize_text_to_width(ctx,
                                            source_text,
                                            font_role,
                                            tier,
                                            max_width_px,
                                            out_text,
                                            out_text_cap,
                                            &metrics,
                                            &truncated);
        if (result.code != CORE_OK) {
            return result;
        }
        if (out_fit) {
            out_fit->text_tier = tier;
            out_fit->width_px = metrics.width_px;
            out_fit->height_px = metrics.height_px;
            out_fit->truncated = truncated;
        }
    }

    return core_result_ok();
}

CoreResult kit_ui_draw_checkbox(KitUiContext *ctx,
                                KitRenderFrame *frame,
                                KitRenderRect bounds,
                                const char *label,
                                int checked,
                                KitUiWidgetState state) {
    KitRenderColor box_color;
    KitRenderColor mark_color;
    CoreResult result;
    KitRenderRect box_rect;
    KitRenderRectCommand rect_cmd;
    KitRenderLineCommand mark_cmd;

    if (!ctx || !frame || !label) {
        return kit_ui_invalid("invalid checkbox");
    }

    box_rect.x = bounds.x;
    box_rect.y = bounds.y + ((bounds.height - ctx->style.checkbox_size) * 0.5f);
    box_rect.width = ctx->style.checkbox_size;
    box_rect.height = ctx->style.checkbox_size;

    result = ui_color(ctx, ui_surface_token_for_state(state), &box_color);
    if (result.code != CORE_OK) return result;
    result = ui_color(ctx, CORE_THEME_COLOR_STATUS_OK, &mark_color);
    if (result.code != CORE_OK) return result;

    rect_cmd.rect = box_rect;
    rect_cmd.corner_radius = 4.0f;
    rect_cmd.color = box_color;
    rect_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_rect(frame, &rect_cmd);
    if (result.code != CORE_OK) return result;

    if (checked) {
        mark_cmd.p0.x = box_rect.x + 4.0f;
        mark_cmd.p0.y = box_rect.y + (box_rect.height * 0.5f);
        mark_cmd.p1.x = box_rect.x + box_rect.width - 4.0f;
        mark_cmd.p1.y = box_rect.y + (box_rect.height * 0.5f);
        mark_cmd.thickness = 2.0f;
        mark_cmd.color = mark_color;
        mark_cmd.transform = kit_render_identity_transform();
        result = kit_render_push_line(frame, &mark_cmd);
        if (result.code != CORE_OK) return result;
    }

    bounds.x += ctx->style.checkbox_size + ctx->style.gap;
    bounds.width -= ctx->style.checkbox_size + ctx->style.gap;
    return kit_ui_draw_label(ctx, frame, bounds, label, ui_text_token_for_state(state));
}

CoreResult kit_ui_draw_slider(KitUiContext *ctx,
                              KitRenderFrame *frame,
                              KitRenderRect bounds,
                              float value_01,
                              KitUiWidgetState state) {
    KitRenderColor track_color;
    KitRenderColor knob_color;
    CoreResult result;
    KitRenderRect track_rect;
    KitRenderRect knob_rect;
    KitRenderRectCommand rect_cmd;
    float clamped = value_01;

    if (!ctx || !frame) {
        return kit_ui_invalid("invalid slider");
    }

    if (clamped < 0.0f) clamped = 0.0f;
    if (clamped > 1.0f) clamped = 1.0f;

    result = ui_color(ctx, CORE_THEME_COLOR_SURFACE_1, &track_color);
    if (result.code != CORE_OK) return result;
    result = ui_color(ctx, ui_surface_token_for_state(state), &knob_color);
    if (result.code != CORE_OK) return result;

    track_rect.x = bounds.x;
    track_rect.y = bounds.y + ((bounds.height - ctx->style.slider_track_height) * 0.5f);
    track_rect.width = bounds.width;
    track_rect.height = ctx->style.slider_track_height;

    rect_cmd.rect = track_rect;
    rect_cmd.corner_radius = ctx->style.slider_track_height * 0.5f;
    rect_cmd.color = track_color;
    rect_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_rect(frame, &rect_cmd);
    if (result.code != CORE_OK) return result;

    knob_rect.width = ctx->style.slider_knob_width;
    knob_rect.height = bounds.height;
    knob_rect.x = bounds.x + ((bounds.width - knob_rect.width) * clamped);
    knob_rect.y = bounds.y;

    rect_cmd.rect = knob_rect;
    rect_cmd.corner_radius = 4.0f;
    rect_cmd.color = knob_color;
    rect_cmd.transform = kit_render_identity_transform();
    return kit_render_push_rect(frame, &rect_cmd);
}

CoreResult kit_ui_draw_scrollbar(KitUiContext *ctx,
                                 KitRenderFrame *frame,
                                 KitRenderRect viewport_bounds,
                                 float offset_y,
                                 float content_height) {
    KitRenderColor track_color;
    KitRenderColor thumb_color;
    KitRenderRect track;
    KitRenderRect thumb;
    KitRenderRectCommand rect_cmd;
    CoreResult result;
    float ratio;
    float max_offset;
    float thumb_h;

    if (!ctx || !frame || viewport_bounds.height <= 0.0f) {
        return kit_ui_invalid("invalid scrollbar");
    }
    if (content_height <= viewport_bounds.height) {
        return core_result_ok();
    }

    result = ui_color(ctx, CORE_THEME_COLOR_SURFACE_1, &track_color);
    if (result.code != CORE_OK) return result;
    result = ui_color(ctx, CORE_THEME_COLOR_SURFACE_2, &thumb_color);
    if (result.code != CORE_OK) return result;

    track.x = viewport_bounds.x + viewport_bounds.width - 8.0f;
    track.y = viewport_bounds.y;
    track.width = 6.0f;
    track.height = viewport_bounds.height;

    ratio = viewport_bounds.height / content_height;
    if (ratio < 0.1f) ratio = 0.1f;
    thumb_h = track.height * ratio;
    max_offset = content_height - viewport_bounds.height;
    if (offset_y < 0.0f) offset_y = 0.0f;
    if (offset_y > max_offset) offset_y = max_offset;

    thumb = track;
    thumb.height = thumb_h;
    thumb.y = track.y + ((track.height - thumb.height) * (offset_y / max_offset));

    rect_cmd.rect = track;
    rect_cmd.corner_radius = 3.0f;
    rect_cmd.color = track_color;
    rect_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_rect(frame, &rect_cmd);
    if (result.code != CORE_OK) return result;

    rect_cmd.rect = thumb;
    rect_cmd.color = thumb_color;
    return kit_render_push_rect(frame, &rect_cmd);
}

CoreResult kit_ui_draw_segmented(KitUiContext *ctx,
                                 KitRenderFrame *frame,
                                 KitRenderRect bounds,
                                 const char *const *labels,
                                 int segment_count,
                                 int selected_index,
                                 int hovered_index,
                                 int enabled) {
    float seg_width;
    int i;
    CoreResult result;

    if (!ctx || !frame || !labels || segment_count <= 0) {
        return kit_ui_invalid("invalid segmented control");
    }

    seg_width = bounds.width / (float)segment_count;
    if (seg_width <= 0.0f) {
        return kit_ui_invalid("invalid segmented width");
    }

    for (i = 0; i < segment_count; ++i) {
        KitRenderRect seg = {
            bounds.x + ((float)i * seg_width),
            bounds.y,
            seg_width,
            bounds.height
        };
        KitUiWidgetState state = KIT_UI_STATE_NORMAL;

        if (!enabled) {
            state = KIT_UI_STATE_DISABLED;
        } else if (i == selected_index) {
            state = KIT_UI_STATE_ACTIVE;
        } else if (i == hovered_index) {
            state = KIT_UI_STATE_HOVERED;
        }

        result = kit_ui_draw_button(ctx, frame, seg, labels[i], state);
        if (result.code != CORE_OK) {
            return result;
        }
    }

    return core_result_ok();
}
