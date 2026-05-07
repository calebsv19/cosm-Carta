#include "kit_workspace_authoring_ui.h"

#include <math.h>
#include <string.h>

typedef struct KitWorkspaceAuthoringThemePresetButtonDef {
    KitWorkspaceAuthoringFontThemeButtonId button_id;
    CoreThemePresetId theme_id;
    const char *label;
} KitWorkspaceAuthoringThemePresetButtonDef;

typedef struct KitWorkspaceAuthoringFontPresetButtonDef {
    KitWorkspaceAuthoringFontThemeButtonId button_id;
    CoreFontPresetId font_id;
    const char *label;
    uint8_t enabled;
} KitWorkspaceAuthoringFontPresetButtonDef;

typedef struct KitWorkspaceAuthoringCustomThemeActionDef {
    KitWorkspaceAuthoringFontThemeButtonId button_id;
    const char *label;
    const char *status_text;
} KitWorkspaceAuthoringCustomThemeActionDef;

static const KitWorkspaceAuthoringFontPresetButtonDef k_font_preset_buttons
    [KIT_WORKSPACE_AUTHORING_FONT_THEME_FONT_PRESET_BUTTON_COUNT] = {
        { KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT,
          CORE_FONT_PRESET_DAW_DEFAULT,
          "daw_default",
          1u },
        { KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_IDE,
          CORE_FONT_PRESET_IDE,
          "ide",
          1u },
        { KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_CUSTOM_STUB,
          CORE_FONT_PRESET_IDE,
          "custom slot (coming soon)",
          0u }
    };

static const KitWorkspaceAuthoringThemePresetButtonDef k_theme_preset_buttons
    [KIT_WORKSPACE_AUTHORING_FONT_THEME_THEME_PRESET_BUTTON_COUNT] = {
        { KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_DAW_DEFAULT,
          CORE_THEME_PRESET_DAW_DEFAULT,
          "daw_default" },
        { KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_STANDARD_GREY,
          CORE_THEME_PRESET_IDE_GRAY,
          "standard_grey" },
        { KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_MIDNIGHT_CONTRAST,
          CORE_THEME_PRESET_DARK_DEFAULT,
          "midnight_contrast" },
        { KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_SOFT_LIGHT,
          CORE_THEME_PRESET_LIGHT_DEFAULT,
          "soft_light" },
        { KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_GREYSCALE,
          CORE_THEME_PRESET_GREYSCALE,
          "greyscale" }
    };

static const KitWorkspaceAuthoringCustomThemeActionDef k_custom_theme_actions
    [KIT_WORKSPACE_AUTHORING_FONT_THEME_CUSTOM_THEME_BUTTON_COUNT] = {
        { KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_CREATE_STUB,
          "Create (stub)",
          "Create requested. Custom theme editor is not implemented yet." },
        { KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_EDIT_STUB,
          "Edit (stub)",
          "Edit requested. Custom theme editor is not implemented yet." }
    };

static int font_theme_point_in_rect(KitRenderRect rect, float x, float y) {
    if (x < rect.x || y < rect.y) return 0;
    if (x > rect.x + rect.width || y > rect.y + rect.height) return 0;
    return 1;
}

static void font_theme_measure_caption_text(const KitRenderContext *render_ctx,
                                            const char *text,
                                            KitRenderTextMetrics *out_metrics) {
    KitRenderTextMetrics metrics = {0};
    if (!out_metrics) {
        return;
    }
    if (render_ctx && text && text[0] &&
        kit_render_measure_text(render_ctx,
                                CORE_FONT_ROLE_UI_MEDIUM,
                                CORE_FONT_TEXT_SIZE_CAPTION,
                                text,
                                &metrics)
                .code == CORE_OK) {
        *out_metrics = metrics;
        return;
    }
    metrics.width_px = (float)((text && text[0]) ? (int)strlen(text) * 7 : 0);
    metrics.height_px = 12.0f;
    *out_metrics = metrics;
}

static const KitWorkspaceAuthoringFontPresetButtonDef *font_preset_button_def_by_id(
    KitWorkspaceAuthoringFontThemeButtonId button_id) {
    uint32_t i;
    for (i = 0u; i < KIT_WORKSPACE_AUTHORING_FONT_THEME_FONT_PRESET_BUTTON_COUNT; ++i) {
        if (k_font_preset_buttons[i].button_id == button_id) {
            return &k_font_preset_buttons[i];
        }
    }
    return 0;
}

static const KitWorkspaceAuthoringThemePresetButtonDef *theme_preset_button_def_by_id(
    KitWorkspaceAuthoringFontThemeButtonId button_id) {
    uint32_t i;
    for (i = 0u; i < KIT_WORKSPACE_AUTHORING_FONT_THEME_THEME_PRESET_BUTTON_COUNT; ++i) {
        if (k_theme_preset_buttons[i].button_id == button_id) {
            return &k_theme_preset_buttons[i];
        }
    }
    return 0;
}

static const KitWorkspaceAuthoringCustomThemeActionDef *custom_theme_action_def_by_id(
    KitWorkspaceAuthoringFontThemeButtonId button_id) {
    uint32_t i;
    for (i = 0u; i < KIT_WORKSPACE_AUTHORING_FONT_THEME_CUSTOM_THEME_BUTTON_COUNT; ++i) {
        if (k_custom_theme_actions[i].button_id == button_id) {
            return &k_custom_theme_actions[i];
        }
    }
    return 0;
}

int kit_workspace_authoring_ui_font_theme_build_layout(const KitRenderContext *render_ctx,
                                                       int width,
                                                       int height,
                                                       KitWorkspaceAuthoringFontThemeLayout *out_layout) {
    KitWorkspaceAuthoringFontThemeLayout layout = {0};
    float zoom_scale = 1.0f;
    float section_height = 0.0f;
    float section_min_height = 0.0f;
    float section_gap_y = 0.0f;
    float section_top_offset = 0.0f;
    float section_bottom_pad = 0.0f;
    float section_inset_x = 0.0f;
    float content_inset_x = 0.0f;
    float content_bottom_pad = 0.0f;
    float controls_x = 0.0f;
    float controls_y = 0.0f;
    float dec_w = 0.0f;
    float inc_w = 0.0f;
    float button_h = 0.0f;
    float reset_w = 0.0f;
    float reset_floor_w = 0.0f;
    float gap = 0.0f;
    float chip_w = 0.0f;
    float chip_pref_w = 0.0f;
    float chip_min_w = 0.0f;
    float inner_w = 0.0f;
    float panel_margin_x = 28.0f;
    float panel_top_margin = 46.0f;
    float panel_bottom_margin = 46.0f;
    float text_pad_x = 0.0f;
    float text_pad_y = 0.0f;
    float available_for_chip = 0.0f;
    float controls_total_w = 0.0f;
    float font_label_max_w = 0.0f;
    float font_label_max_h = 0.0f;
    float theme_label_max_w = 0.0f;
    float custom_label_max_w = 0.0f;
    float font_cols_threshold = 460.0f;
    float theme_cols_threshold = 460.0f;
    float custom_cols_threshold = 360.0f;
    KitRenderTextMetrics metrics = {0};
    KitRenderTextMetrics metrics_plus = {0};
    KitRenderTextMetrics metrics_minus = {0};
    KitRenderTextMetrics metrics_reset = {0};
    KitRenderTextMetrics metrics_chip = {0};
    float text_height_max = 0.0f;
    uint32_t i = 0u;

    if (!out_layout || width <= 0 || height <= 0) {
        return 0;
    }

    if (render_ctx) {
        zoom_scale = (float)kit_render_text_zoom_percent(render_ctx) / 100.0f;
    }
    if (zoom_scale < 0.75f) {
        zoom_scale = 0.75f;
    } else if (zoom_scale > 2.5f) {
        zoom_scale = 2.5f;
    }

    section_inset_x = fmaxf(12.0f, 12.0f * zoom_scale);
    section_top_offset = fmaxf(42.0f, 42.0f * zoom_scale);
    section_bottom_pad = fmaxf(14.0f, 14.0f * zoom_scale);
    section_gap_y = fmaxf(8.0f, 8.0f * zoom_scale);
    content_inset_x = fmaxf(14.0f, 14.0f * zoom_scale);
    content_bottom_pad = fmaxf(10.0f, 10.0f * zoom_scale);
    text_pad_x = fmaxf(8.0f, 8.0f * zoom_scale);
    text_pad_y = fmaxf(5.0f, 5.0f * zoom_scale);
    gap = fmaxf(6.0f, 6.0f * zoom_scale);
    font_cols_threshold *= zoom_scale;
    theme_cols_threshold *= zoom_scale;
    custom_cols_threshold *= zoom_scale;

    font_theme_measure_caption_text(render_ctx, "-", &metrics_minus);
    font_theme_measure_caption_text(render_ctx, "+", &metrics_plus);
    font_theme_measure_caption_text(render_ctx, "Reset", &metrics_reset);
    font_theme_measure_caption_text(render_ctx, "step -24  (250%)", &metrics_chip);
    text_height_max = fmaxf(fmaxf(metrics_minus.height_px, metrics_plus.height_px),
                            metrics_reset.height_px);
    button_h = ceilf(fmaxf(22.0f * zoom_scale, text_height_max + (text_pad_y * 2.0f)));
    dec_w = ceilf(fmaxf(button_h, metrics_minus.width_px + (text_pad_x * 2.0f)));
    inc_w = ceilf(fmaxf(button_h, metrics_plus.width_px + (text_pad_x * 2.0f)));
    reset_floor_w = ceilf(fmaxf(40.0f * zoom_scale, metrics_reset.width_px + (text_pad_x * 1.5f)));
    reset_w = ceilf(fmaxf(58.0f * zoom_scale, metrics_reset.width_px + (text_pad_x * 2.0f)));
    chip_pref_w = ceilf(fmaxf(84.0f * zoom_scale, metrics_chip.width_px + (text_pad_x * 2.0f)));
    chip_min_w = ceilf(fmaxf(52.0f * zoom_scale, 36.0f));

    for (i = 0u; i < KIT_WORKSPACE_AUTHORING_FONT_THEME_FONT_PRESET_BUTTON_COUNT; ++i) {
        font_theme_measure_caption_text(render_ctx, k_font_preset_buttons[i].label, &metrics);
        if (metrics.width_px > font_label_max_w) font_label_max_w = metrics.width_px;
        if (metrics.height_px > font_label_max_h) font_label_max_h = metrics.height_px;
    }
    for (i = 0u; i < KIT_WORKSPACE_AUTHORING_FONT_THEME_THEME_PRESET_BUTTON_COUNT; ++i) {
        font_theme_measure_caption_text(render_ctx, k_theme_preset_buttons[i].label, &metrics);
        if (metrics.width_px > theme_label_max_w) theme_label_max_w = metrics.width_px;
    }
    for (i = 0u; i < KIT_WORKSPACE_AUTHORING_FONT_THEME_CUSTOM_THEME_BUTTON_COUNT; ++i) {
        font_theme_measure_caption_text(render_ctx, k_custom_theme_actions[i].label, &metrics);
        if (metrics.width_px > custom_label_max_w) custom_label_max_w = metrics.width_px;
    }

    layout.panel = (KitRenderRect){ panel_margin_x,
                                    panel_top_margin,
                                    (float)width - (panel_margin_x * 2.0f),
                                    (float)height - (panel_top_margin + panel_bottom_margin) };
    if (layout.panel.width < 40.0f || layout.panel.height < 40.0f) {
        *out_layout = layout;
        return 0;
    }

    section_min_height = fmaxf(64.0f * zoom_scale,
                               fmaxf(button_h, text_height_max + (text_pad_y * 2.0f)) +
                                   content_bottom_pad +
                                   fmaxf(52.0f * zoom_scale, font_label_max_h + 34.0f));
    section_height = (layout.panel.height - (section_top_offset + section_bottom_pad +
                                             (section_gap_y * 3.0f))) /
                     4.0f;
    if (section_height < section_min_height) {
        section_height = section_min_height;
    }

    layout.font_preset_section = (KitRenderRect){ layout.panel.x + section_inset_x,
                                                  layout.panel.y + section_top_offset,
                                                  layout.panel.width - (section_inset_x * 2.0f),
                                                  section_height };
    layout.text_size_section = layout.font_preset_section;
    layout.text_size_section.y += section_height + section_gap_y;
    layout.theme_preset_section = layout.text_size_section;
    layout.theme_preset_section.y += section_height + section_gap_y;
    layout.custom_theme_section = layout.theme_preset_section;
    layout.custom_theme_section.y += section_height + section_gap_y;

    inner_w = layout.text_size_section.width - (content_inset_x * 2.0f);
    if (inner_w < 180.0f) {
        inner_w = 180.0f;
    }
    available_for_chip = inner_w - (dec_w + inc_w + reset_w + (gap * 3.0f));
    chip_w = chip_pref_w;
    if (chip_w > available_for_chip) chip_w = available_for_chip;
    if (chip_w < chip_min_w) {
        float deficit = chip_min_w - chip_w;
        float can_reduce_reset = reset_w - reset_floor_w;
        if (can_reduce_reset > 0.0f) {
            float reduce = deficit < can_reduce_reset ? deficit : can_reduce_reset;
            reset_w -= reduce;
        }
        available_for_chip = inner_w - (dec_w + inc_w + reset_w + (gap * 3.0f));
        chip_w = available_for_chip;
    }
    if (chip_w < 36.0f) chip_w = 36.0f;
    controls_total_w = dec_w + inc_w + chip_w + reset_w + (gap * 3.0f);
    if (controls_total_w > inner_w) {
        float overflow = controls_total_w - inner_w;
        float chip_reduce_cap = chip_w - 36.0f;
        if (chip_reduce_cap > 0.0f) {
            float reduce = overflow < chip_reduce_cap ? overflow : chip_reduce_cap;
            chip_w -= reduce;
            overflow -= reduce;
        }
        if (overflow > 0.0f) {
            float reset_reduce_cap = reset_w - reset_floor_w;
            if (reset_reduce_cap > 0.0f) {
                float reduce = overflow < reset_reduce_cap ? overflow : reset_reduce_cap;
                reset_w -= reduce;
            }
        }
    }

    controls_x = layout.text_size_section.x + content_inset_x;
    controls_y = layout.text_size_section.y + layout.text_size_section.height -
                 button_h - content_bottom_pad;
    layout.text_size_dec_button = (KitRenderRect){ controls_x, controls_y, dec_w, button_h };
    controls_x += dec_w + gap;
    layout.text_size_inc_button = (KitRenderRect){ controls_x, controls_y, inc_w, button_h };
    controls_x += inc_w + gap;
    layout.text_size_value_chip = (KitRenderRect){ controls_x, controls_y, chip_w, button_h };
    controls_x += chip_w + gap;
    layout.text_size_reset_button = (KitRenderRect){ controls_x, controls_y, reset_w, button_h };

    layout.font_preset_button_count = KIT_WORKSPACE_AUTHORING_FONT_THEME_FONT_PRESET_BUTTON_COUNT;
    layout.theme_preset_button_count = KIT_WORKSPACE_AUTHORING_FONT_THEME_THEME_PRESET_BUTTON_COUNT;
    layout.custom_theme_button_count = KIT_WORKSPACE_AUTHORING_FONT_THEME_CUSTOM_THEME_BUTTON_COUNT;

    {
        float font_inner_w = layout.font_preset_section.width - (content_inset_x * 2.0f);
        float font_button_min_w = ceilf(fmaxf(72.0f * zoom_scale, font_label_max_w + (text_pad_x * 2.0f)));
        float font_button_h = button_h;
        float font_button_w = 0.0f;
        float font_x = layout.font_preset_section.x + content_inset_x;
        float font_y = 0.0f;
        int cols = font_inner_w < font_cols_threshold ? 2 : 3;
        int rows = 1;
        if (font_inner_w < 220.0f) font_inner_w = 220.0f;
        if (cols > (int)layout.font_preset_button_count) cols = (int)layout.font_preset_button_count;
        if (cols < 1) cols = 1;
        rows = ((int)layout.font_preset_button_count + cols - 1) / cols;
        font_button_w = (font_inner_w - ((float)(cols - 1) * gap)) / (float)cols;
        if (font_button_w < font_button_min_w) font_button_w = font_button_min_w;
        font_y = layout.font_preset_section.y + layout.font_preset_section.height -
                 (((float)rows * font_button_h) + ((float)(rows - 1) * gap)) - content_bottom_pad;
        for (i = 0u; i < layout.font_preset_button_count; ++i) {
            int row = (int)(i / (uint32_t)cols);
            int col = (int)(i % (uint32_t)cols);
            layout.font_preset_buttons[i] = (KitRenderRect){
                font_x + ((font_button_w + gap) * (float)col),
                font_y + ((font_button_h + gap) * (float)row),
                font_button_w,
                font_button_h
            };
        }
    }

    {
        float theme_inner_w = layout.theme_preset_section.width - (content_inset_x * 2.0f);
        float theme_button_min_w = ceilf(fmaxf(72.0f * zoom_scale, theme_label_max_w + (text_pad_x * 2.0f)));
        float theme_button_h = button_h;
        float theme_button_w = 0.0f;
        float theme_x = layout.theme_preset_section.x + content_inset_x;
        float theme_y = 0.0f;
        int cols = theme_inner_w < theme_cols_threshold ? 3 : 5;
        int rows = 1;
        if (theme_inner_w < 220.0f) theme_inner_w = 220.0f;
        if (cols > (int)layout.theme_preset_button_count) cols = (int)layout.theme_preset_button_count;
        if (cols < 1) cols = 1;
        rows = ((int)layout.theme_preset_button_count + cols - 1) / cols;
        theme_button_w = (theme_inner_w - ((float)(cols - 1) * gap)) / (float)cols;
        if (theme_button_w < theme_button_min_w) theme_button_w = theme_button_min_w;
        theme_y = layout.theme_preset_section.y + layout.theme_preset_section.height -
                  (((float)rows * theme_button_h) + ((float)(rows - 1) * gap)) - content_bottom_pad;
        for (i = 0u; i < layout.theme_preset_button_count; ++i) {
            int row = (int)(i / (uint32_t)cols);
            int col = (int)(i % (uint32_t)cols);
            layout.theme_preset_buttons[i] = (KitRenderRect){
                theme_x + ((theme_button_w + gap) * (float)col),
                theme_y + ((theme_button_h + gap) * (float)row),
                theme_button_w,
                theme_button_h
            };
        }
    }

    {
        float custom_inner_w = layout.custom_theme_section.width - (content_inset_x * 2.0f);
        float custom_button_min_w = ceilf(fmaxf(72.0f * zoom_scale, custom_label_max_w + (text_pad_x * 2.0f)));
        float custom_button_h = button_h;
        float custom_button_w = 0.0f;
        float custom_x = layout.custom_theme_section.x + content_inset_x;
        float custom_y = 0.0f;
        int cols = custom_inner_w < custom_cols_threshold ? 1 : 2;
        int rows = 1;
        if (custom_inner_w < 220.0f) custom_inner_w = 220.0f;
        if (cols > (int)layout.custom_theme_button_count) cols = (int)layout.custom_theme_button_count;
        if (cols < 1) cols = 1;
        rows = ((int)layout.custom_theme_button_count + cols - 1) / cols;
        custom_button_w = (custom_inner_w - ((float)(cols - 1) * gap)) / (float)cols;
        if (custom_button_w < custom_button_min_w) custom_button_w = custom_button_min_w;
        custom_y = layout.custom_theme_section.y + layout.custom_theme_section.height -
                   (((float)rows * custom_button_h) + ((float)(rows - 1) * gap)) - content_bottom_pad;
        for (i = 0u; i < layout.custom_theme_button_count; ++i) {
            int row = (int)(i / (uint32_t)cols);
            int col = (int)(i % (uint32_t)cols);
            layout.custom_theme_buttons[i] = (KitRenderRect){
                custom_x + ((custom_button_w + gap) * (float)col),
                custom_y + ((custom_button_h + gap) * (float)row),
                custom_button_w,
                custom_button_h
            };
        }
    }

    *out_layout = layout;
    return 1;
}

KitWorkspaceAuthoringFontThemeButtonId kit_workspace_authoring_ui_font_theme_hit_button(
    const KitWorkspaceAuthoringFontThemeLayout *layout,
    float x,
    float y) {
    uint32_t i;
    if (!layout) {
        return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE;
    }
    if (font_theme_point_in_rect(layout->text_size_dec_button, x, y)) {
        return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC;
    }
    if (font_theme_point_in_rect(layout->text_size_inc_button, x, y)) {
        return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC;
    }
    if (font_theme_point_in_rect(layout->text_size_reset_button, x, y)) {
        return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET;
    }
    for (i = 0u; i < layout->font_preset_button_count &&
                i < KIT_WORKSPACE_AUTHORING_FONT_THEME_FONT_PRESET_BUTTON_COUNT; ++i) {
        if (font_theme_point_in_rect(layout->font_preset_buttons[i], x, y)) {
            return k_font_preset_buttons[i].button_id;
        }
    }
    for (i = 0u; i < layout->theme_preset_button_count &&
                i < KIT_WORKSPACE_AUTHORING_FONT_THEME_THEME_PRESET_BUTTON_COUNT; ++i) {
        if (font_theme_point_in_rect(layout->theme_preset_buttons[i], x, y)) {
            return k_theme_preset_buttons[i].button_id;
        }
    }
    for (i = 0u; i < layout->custom_theme_button_count &&
                i < KIT_WORKSPACE_AUTHORING_FONT_THEME_CUSTOM_THEME_BUTTON_COUNT; ++i) {
        if (font_theme_point_in_rect(layout->custom_theme_buttons[i], x, y)) {
            return k_custom_theme_actions[i].button_id;
        }
    }
    return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE;
}

const char *kit_workspace_authoring_ui_font_theme_button_label(
    KitWorkspaceAuthoringFontThemeButtonId button_id) {
    const KitWorkspaceAuthoringFontPresetButtonDef *font_def = font_preset_button_def_by_id(button_id);
    const KitWorkspaceAuthoringThemePresetButtonDef *theme_def = theme_preset_button_def_by_id(button_id);
    const KitWorkspaceAuthoringCustomThemeActionDef *custom_def = custom_theme_action_def_by_id(button_id);
    if (font_def) return font_def->label;
    if (theme_def) return theme_def->label;
    if (custom_def) return custom_def->label;
    switch (button_id) {
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC:
            return "-";
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC:
            return "+";
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET:
            return "Reset";
        default:
            return "";
    }
}

uint8_t kit_workspace_authoring_ui_font_theme_button_enabled(
    KitWorkspaceAuthoringFontThemeButtonId button_id) {
    const KitWorkspaceAuthoringFontPresetButtonDef *font_def = font_preset_button_def_by_id(button_id);
    if (font_def) return font_def->enabled;
    return button_id == KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE ? 0u : 1u;
}

int kit_workspace_authoring_ui_font_theme_button_font_preset_id(
    KitWorkspaceAuthoringFontThemeButtonId button_id,
    CoreFontPresetId *out_font_id) {
    const KitWorkspaceAuthoringFontPresetButtonDef *def = font_preset_button_def_by_id(button_id);
    if (!def || !out_font_id || !def->enabled) {
        return 0;
    }
    *out_font_id = def->font_id;
    return 1;
}

int kit_workspace_authoring_ui_font_theme_button_theme_preset_id(
    KitWorkspaceAuthoringFontThemeButtonId button_id,
    CoreThemePresetId *out_theme_id) {
    const KitWorkspaceAuthoringThemePresetButtonDef *def = theme_preset_button_def_by_id(button_id);
    if (!def || !out_theme_id) {
        return 0;
    }
    *out_theme_id = def->theme_id;
    return 1;
}

int kit_workspace_authoring_ui_font_theme_button_custom_theme_status(
    KitWorkspaceAuthoringFontThemeButtonId button_id,
    const char **out_status_text) {
    const KitWorkspaceAuthoringCustomThemeActionDef *def = custom_theme_action_def_by_id(button_id);
    if (!def || !out_status_text) {
        return 0;
    }
    *out_status_text = def->status_text;
    return 1;
}

KitWorkspaceAuthoringFontThemeAction kit_workspace_authoring_ui_font_theme_action_for_button(
    KitWorkspaceAuthoringFontThemeButtonId button_id) {
    KitWorkspaceAuthoringFontThemeAction action = {0};
    action.button_id = button_id;
    switch (button_id) {
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC:
            action.type = KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_DEC;
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC:
            action.type = KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_INC;
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET:
            action.type = KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_RESET;
            break;
        default:
            if (kit_workspace_authoring_ui_font_theme_button_font_preset_id(button_id,
                                                                            &action.font_preset_id)) {
                action.type = KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_FONT_PRESET;
            } else if (kit_workspace_authoring_ui_font_theme_button_theme_preset_id(button_id,
                                                                                   &action.theme_preset_id)) {
                action.type = KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_THEME_PRESET;
            } else if (kit_workspace_authoring_ui_font_theme_button_custom_theme_status(
                           button_id,
                           &action.custom_status_text)) {
                action.type = KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_CUSTOM_THEME_STATUS;
            } else {
                action.type = KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_NONE;
            }
            break;
    }
    return action;
}
