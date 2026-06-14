/*
 * kit_ui_button.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "kit_ui.h"

#include <string.h>

static CoreThemeColor kit_ui_button_mix(CoreThemeColor base,
                                        CoreThemeColor tint,
                                        unsigned int tint_weight_255) {
    unsigned int base_weight = 255u - tint_weight_255;
    CoreThemeColor mixed;

    mixed.r = (uint8_t)(((unsigned int)base.r * base_weight +
                         (unsigned int)tint.r * tint_weight_255) /
                        255u);
    mixed.g = (uint8_t)(((unsigned int)base.g * base_weight +
                         (unsigned int)tint.g * tint_weight_255) /
                        255u);
    mixed.b = (uint8_t)(((unsigned int)base.b * base_weight +
                         (unsigned int)tint.b * tint_weight_255) /
                        255u);
    mixed.a = 255u;
    return mixed;
}

static void kit_ui_button_theme_resolve_color(const CoreThemePreset *preset,
                                              CoreThemeColorToken token,
                                              CoreThemeColor fallback,
                                              CoreThemeColor *out_color) {
    CoreThemeColor color;

    if (out_color == 0) {
        return;
    }
    if (preset != 0 &&
        core_theme_get_color(preset, token, &color).code == CORE_OK) {
        *out_color = color;
        return;
    }
    *out_color = fallback;
}

static CoreResult kit_ui_button_push_outline(KitRenderFrame *frame,
                                             KitRenderRect bounds,
                                             KitRenderColor color) {
    KitRenderRectCommand edge_cmd;
    CoreResult result;

    if (frame == 0 || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        CoreResult invalid = { CORE_ERR_INVALID_ARG, "invalid button outline bounds" };
        return invalid;
    }

    edge_cmd.corner_radius = 0.0f;
    edge_cmd.color = color;
    edge_cmd.transform = kit_render_identity_transform();

    edge_cmd.rect = (KitRenderRect){ bounds.x, bounds.y, bounds.width, 1.0f };
    result = kit_render_push_rect(frame, &edge_cmd);
    if (result.code != CORE_OK) {
        return result;
    }

    if (bounds.height > 1.0f) {
        edge_cmd.rect =
            (KitRenderRect){ bounds.x, bounds.y + bounds.height - 1.0f, bounds.width, 1.0f };
        result = kit_render_push_rect(frame, &edge_cmd);
        if (result.code != CORE_OK) {
            return result;
        }
    }

    if (bounds.height > 2.0f) {
        edge_cmd.rect = (KitRenderRect){ bounds.x, bounds.y + 1.0f, 1.0f, bounds.height - 2.0f };
        result = kit_render_push_rect(frame, &edge_cmd);
        if (result.code != CORE_OK) {
            return result;
        }

        if (bounds.width > 1.0f) {
            edge_cmd.rect = (KitRenderRect){
                bounds.x + bounds.width - 1.0f,
                bounds.y + 1.0f,
                1.0f,
                bounds.height - 2.0f
            };
            result = kit_render_push_rect(frame, &edge_cmd);
            if (result.code != CORE_OK) {
                return result;
            }
        }
    }

    return result;
}

static float kit_ui_button_clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static KitRenderRect kit_ui_button_inset_rect(KitRenderRect rect, float inset) {
    if (inset <= 0.0f) {
        return rect;
    }
    rect.x += inset;
    rect.y += inset;
    rect.width -= inset * 2.0f;
    rect.height -= inset * 2.0f;
    if (rect.width < 0.0f) {
        rect.width = 0.0f;
    }
    if (rect.height < 0.0f) {
        rect.height = 0.0f;
    }
    return rect;
}

static CoreResult kit_ui_button_push_appearance_fill(KitRenderFrame *frame,
                                                     KitRenderRect bounds,
                                                     const KitUiButtonAppearance *appearance,
                                                     KitRenderColor fill_color,
                                                     KitRenderColor outline_color) {
    KitRenderRectCommand rect_cmd;
    float border = 0.0f;
    float max_radius = 0.0f;
    float radius = 0.0f;
    CoreResult result;

    if (frame == 0 || appearance == 0 || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        CoreResult invalid = { CORE_ERR_INVALID_ARG, "invalid button appearance bounds" };
        return invalid;
    }

    max_radius = (bounds.width < bounds.height ? bounds.width : bounds.height) * 0.5f;
    radius = kit_ui_button_clampf(appearance->corner_radius, 0.0f, max_radius);
    border = kit_ui_button_clampf(appearance->border_thickness, 0.0f, max_radius);

    rect_cmd.rect = bounds;
    rect_cmd.corner_radius = radius;
    rect_cmd.transform = kit_render_identity_transform();

    if (border > 0.0f) {
        rect_cmd.color = outline_color;
        result = kit_render_push_rect(frame, &rect_cmd);
        if (result.code != CORE_OK) {
            return result;
        }
        rect_cmd.rect = kit_ui_button_inset_rect(bounds, border);
        rect_cmd.corner_radius = kit_ui_button_clampf(radius - border, 0.0f, max_radius);
    }

    rect_cmd.color = fill_color;
    return kit_render_push_rect(frame, &rect_cmd);
}

void kit_ui_button_state_init(KitUiButtonState *state) {
    if (state == 0) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void kit_ui_button_spec_init(KitUiButtonSpec *spec, const char *label) {
    if (spec == 0) {
        return;
    }
    memset(spec, 0, sizeof(*spec));
    spec->label = label;
    spec->variant = KIT_UI_BUTTON_VARIANT_DEFAULT;
}

void kit_ui_button_layout_init(KitUiButtonLayout *layout,
                               float text_offset_x,
                               float text_offset_y) {
    if (layout == 0) {
        return;
    }
    layout->text_offset_x = text_offset_x;
    layout->text_offset_y = text_offset_y;
}

void kit_ui_button_appearance_init(KitUiButtonAppearance *appearance) {
    if (appearance == 0) {
        return;
    }
    appearance->corner_radius = 0.0f;
    appearance->border_thickness = 1.0f;
    appearance->padding_x = 14.0f;
    appearance->padding_y = 9.0f;
}

int kit_ui_button_appearance_preset(KitUiButtonAppearancePreset preset,
                                    KitUiButtonAppearance *out_appearance) {
    if (out_appearance == 0) {
        return 0;
    }
    kit_ui_button_appearance_init(out_appearance);
    switch (preset) {
        case KIT_UI_BUTTON_APPEARANCE_DEFAULT:
            return 1;
        case KIT_UI_BUTTON_APPEARANCE_ROUNDED:
            out_appearance->corner_radius = 8.0f;
            return 1;
        case KIT_UI_BUTTON_APPEARANCE_COMPACT_ROUNDED:
            out_appearance->corner_radius = 7.0f;
            out_appearance->padding_x = 10.0f;
            out_appearance->padding_y = 7.0f;
            return 1;
        case KIT_UI_BUTTON_APPEARANCE_PILL:
            out_appearance->corner_radius = 9999.0f;
            out_appearance->padding_x = 12.0f;
            out_appearance->padding_y = 7.0f;
            return 1;
        default:
            return 0;
    }
}

void kit_ui_button_layout_from_appearance(const KitUiButtonAppearance *appearance,
                                          KitUiButtonLayout *out_layout) {
    if (appearance == 0 || out_layout == 0) {
        return;
    }
    kit_ui_button_layout_init(out_layout, appearance->padding_x, appearance->padding_y);
}

void kit_ui_button_text_origin(const KitUiButtonLayout *layout,
                               float rect_x,
                               float rect_y,
                               float *out_x,
                               float *out_y) {
    if (layout == 0 || out_x == 0 || out_y == 0) {
        return;
    }
    *out_x = rect_x + layout->text_offset_x;
    *out_y = rect_y + layout->text_offset_y;
}

void kit_ui_button_text_origin_i(const KitUiButtonLayout *layout,
                                 int rect_x,
                                 int rect_y,
                                 int *out_x,
                                 int *out_y) {
    if (layout == 0 || out_x == 0 || out_y == 0) {
        return;
    }
    *out_x = rect_x + (int)layout->text_offset_x;
    *out_y = rect_y + (int)layout->text_offset_y;
}

int kit_ui_button_theme_from_preset(const CoreThemePreset *preset,
                                    KitUiButtonTheme *out_theme) {
    CoreThemeColor surface_1 = {35u, 42u, 50u, 255u};
    CoreThemeColor surface_2 = {45u, 53u, 62u, 255u};
    CoreThemeColor text_primary = {234u, 236u, 232u, 255u};
    CoreThemeColor text_muted = {164u, 167u, 164u, 255u};
    CoreThemeColor accent_primary = {92u, 150u, 255u, 255u};
    CoreThemeColor status_ok = {36u, 172u, 86u, 255u};

    if (out_theme == 0) {
        return 0;
    }

    kit_ui_button_theme_resolve_color(preset,
                                      CORE_THEME_COLOR_SURFACE_1,
                                      surface_1,
                                      &surface_1);
    kit_ui_button_theme_resolve_color(preset,
                                      CORE_THEME_COLOR_SURFACE_2,
                                      surface_2,
                                      &surface_2);
    kit_ui_button_theme_resolve_color(preset,
                                      CORE_THEME_COLOR_TEXT_PRIMARY,
                                      text_primary,
                                      &text_primary);
    kit_ui_button_theme_resolve_color(preset,
                                      CORE_THEME_COLOR_TEXT_MUTED,
                                      text_muted,
                                      &text_muted);
    kit_ui_button_theme_resolve_color(preset,
                                      CORE_THEME_COLOR_ACCENT_PRIMARY,
                                      accent_primary,
                                      &accent_primary);
    kit_ui_button_theme_resolve_color(preset,
                                      CORE_THEME_COLOR_STATUS_OK,
                                      status_ok,
                                      &status_ok);

    out_theme->idle_fill = surface_2;
    out_theme->selected_fill = surface_1;
    out_theme->hover_fill = kit_ui_button_mix(surface_1, accent_primary, 28u);
    out_theme->positive_fill = kit_ui_button_mix(surface_1, status_ok, 48u);
    out_theme->outline_idle = text_muted;
    out_theme->outline_highlight = kit_ui_button_mix(surface_1, accent_primary, 140u);
    out_theme->text_primary = text_primary;
    out_theme->text_muted = text_muted;
    return 1;
}

int kit_ui_button_style_resolve(const KitUiButtonTheme *theme,
                                const KitUiButtonSpec *spec,
                                KitUiButtonStyle *out_style) {
    CoreThemeColor fill;
    CoreThemeColor outline;
    CoreThemeColor text;

    if (theme == 0 || spec == 0 || out_style == 0) {
        return 0;
    }

    fill = theme->idle_fill;
    if (spec->state.selected) {
        switch (spec->variant) {
            case KIT_UI_BUTTON_VARIANT_POSITIVE:
                fill = theme->positive_fill;
                break;
            case KIT_UI_BUTTON_VARIANT_PRIMARY:
            case KIT_UI_BUTTON_VARIANT_DEFAULT:
            default:
                fill = theme->selected_fill;
                break;
        }
    }
    if (spec->state.hovered) {
        fill = kit_ui_button_mix(fill, theme->hover_fill, 120u);
    }
    if (spec->state.pressed) {
        fill = kit_ui_button_mix(fill, theme->outline_highlight, 96u);
    }
    if (spec->state.disabled) {
        fill = kit_ui_button_mix(theme->idle_fill, theme->text_muted, 36u);
    }

    outline = (spec->state.hovered || spec->state.selected ||
               spec->state.pressed || spec->state.focused)
                  ? theme->outline_highlight
                  : theme->outline_idle;
    if (spec->state.disabled) {
        outline = kit_ui_button_mix(theme->outline_idle, theme->idle_fill, 96u);
    }

    text = spec->state.disabled ? theme->text_muted : theme->text_primary;

    out_style->fill = fill;
    out_style->outline = outline;
    out_style->text = text;
    return 1;
}

int kit_ui_button_style_resolve_preset(const CoreThemePreset *preset,
                                       const KitUiButtonSpec *spec,
                                       KitUiButtonStyle *out_style) {
    KitUiButtonTheme theme;

    if (spec == 0 || out_style == 0 ||
        !kit_ui_button_theme_from_preset(preset, &theme)) {
        return 0;
    }
    return kit_ui_button_style_resolve(&theme, spec, out_style);
}

CoreResult kit_ui_draw_button_spec(KitUiContext *ctx,
                                   KitRenderFrame *frame,
                                   KitRenderRect bounds,
                                   const KitUiButtonSpec *spec,
                                   const KitUiButtonLayout *layout) {
    return kit_ui_draw_button_spec_custom(ctx,
                                          frame,
                                          bounds,
                                          spec,
                                          layout,
                                          CORE_FONT_ROLE_UI_REGULAR,
                                          CORE_FONT_TEXT_SIZE_BASIC);
}

CoreResult kit_ui_draw_button_spec_custom(KitUiContext *ctx,
                                          KitRenderFrame *frame,
                                          KitRenderRect bounds,
                                          const KitUiButtonSpec *spec,
                                          const KitUiButtonLayout *layout,
                                          CoreFontRoleId font_role,
                                          CoreFontTextSizeTier text_tier) {
    KitUiButtonStyle style;
    KitRenderRectCommand rect_cmd;
    KitRenderTextCommand text_cmd;
    CoreThemeColorToken text_token;
    float text_x;
    float text_y;
    CoreResult result;

    if (!ctx || !ctx->render_ctx || !frame || !spec || !layout || !spec->label) {
        CoreResult invalid = { CORE_ERR_INVALID_ARG, "invalid button spec" };
        return invalid;
    }
    if (!kit_ui_button_style_resolve_preset(&ctx->render_ctx->theme, spec, &style)) {
        CoreResult invalid = { CORE_ERR_INVALID_ARG, "failed button style resolve" };
        return invalid;
    }

    rect_cmd.rect = bounds;
    rect_cmd.corner_radius = 0.0f;
    rect_cmd.color = (KitRenderColor){style.fill.r, style.fill.g, style.fill.b, style.fill.a};
    rect_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_rect(frame, &rect_cmd);
    if (result.code != CORE_OK) {
        return result;
    }

    result = kit_ui_button_push_outline(
        frame,
        bounds,
        (KitRenderColor){ style.outline.r, style.outline.g, style.outline.b, style.outline.a });
    if (result.code != CORE_OK) {
        return result;
    }

    kit_ui_button_text_origin(layout, bounds.x, bounds.y, &text_x, &text_y);
    text_token = spec->state.disabled ? CORE_THEME_COLOR_TEXT_MUTED
                                      : CORE_THEME_COLOR_TEXT_PRIMARY;
    text_cmd.origin.x = text_x;
    text_cmd.origin.y = text_y;
    text_cmd.text = spec->label;
    text_cmd.font_role = font_role;
    text_cmd.text_tier = text_tier;
    text_cmd.color_token = text_token;
    text_cmd.transform = kit_render_identity_transform();
    return kit_render_push_text(frame, &text_cmd);
}

CoreResult kit_ui_draw_button_spec_appearance(KitUiContext *ctx,
                                              KitRenderFrame *frame,
                                              KitRenderRect bounds,
                                              const KitUiButtonSpec *spec,
                                              const KitUiButtonLayout *layout,
                                              const KitUiButtonAppearance *appearance) {
    return kit_ui_draw_button_spec_appearance_custom(ctx,
                                                     frame,
                                                     bounds,
                                                     spec,
                                                     layout,
                                                     appearance,
                                                     CORE_FONT_ROLE_UI_REGULAR,
                                                     CORE_FONT_TEXT_SIZE_BASIC);
}

CoreResult kit_ui_draw_button_spec_appearance_custom(KitUiContext *ctx,
                                                     KitRenderFrame *frame,
                                                     KitRenderRect bounds,
                                                     const KitUiButtonSpec *spec,
                                                     const KitUiButtonLayout *layout,
                                                     const KitUiButtonAppearance *appearance,
                                                     CoreFontRoleId font_role,
                                                     CoreFontTextSizeTier text_tier) {
    KitUiButtonStyle style;
    KitRenderTextCommand text_cmd;
    CoreThemeColorToken text_token;
    float text_x;
    float text_y;
    CoreResult result;

    if (!ctx || !ctx->render_ctx || !frame || !spec || !layout || !appearance || !spec->label) {
        CoreResult invalid = { CORE_ERR_INVALID_ARG, "invalid button appearance spec" };
        return invalid;
    }
    if (appearance->corner_radius < 0.0f ||
        appearance->border_thickness < 0.0f ||
        appearance->padding_x < 0.0f ||
        appearance->padding_y < 0.0f) {
        CoreResult invalid = { CORE_ERR_INVALID_ARG, "invalid button appearance values" };
        return invalid;
    }
    if (!kit_ui_button_style_resolve_preset(&ctx->render_ctx->theme, spec, &style)) {
        CoreResult invalid = { CORE_ERR_INVALID_ARG, "failed button appearance style resolve" };
        return invalid;
    }

    result = kit_ui_button_push_appearance_fill(
        frame,
        bounds,
        appearance,
        (KitRenderColor){ style.fill.r, style.fill.g, style.fill.b, style.fill.a },
        (KitRenderColor){ style.outline.r, style.outline.g, style.outline.b, style.outline.a });
    if (result.code != CORE_OK) {
        return result;
    }

    kit_ui_button_text_origin(layout, bounds.x, bounds.y, &text_x, &text_y);
    text_token = spec->state.disabled ? CORE_THEME_COLOR_TEXT_MUTED
                                      : CORE_THEME_COLOR_TEXT_PRIMARY;
    text_cmd.origin.x = text_x;
    text_cmd.origin.y = text_y;
    text_cmd.text = spec->label;
    text_cmd.font_role = font_role;
    text_cmd.text_tier = text_tier;
    text_cmd.color_token = text_token;
    text_cmd.transform = kit_render_identity_transform();
    return kit_render_push_text(frame, &text_cmd);
}
