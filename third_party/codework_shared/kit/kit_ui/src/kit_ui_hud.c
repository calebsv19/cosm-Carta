#include "kit_ui.h"

#include <string.h>

static float kit_ui_hud_clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

KitRenderColor kit_ui_color_with_alpha(KitRenderColor color, uint8_t alpha) {
    color.a = alpha;
    return color;
}

float kit_ui_corner_radius_clamp(float radius, float width, float height) {
    float max_radius = 0.0f;
    if (width <= 0.0f || height <= 0.0f || radius <= 0.0f) {
        return 0.0f;
    }
    max_radius = (width < height ? width : height) * 0.5f;
    return kit_ui_hud_clampf(radius, 0.0f, max_radius);
}

float kit_ui_corner_radius_for_inset(float outer_radius, float inset) {
    if (outer_radius <= 0.0f) {
        return 0.0f;
    }
    if (inset <= 0.0f) {
        return outer_radius;
    }
    if (inset >= outer_radius) {
        return 0.0f;
    }
    return outer_radius - inset;
}

KitRenderRect kit_ui_rect_inset(KitRenderRect rect, float inset) {
    if (inset <= 0.0f) {
        return rect;
    }
    rect.x += inset;
    rect.y += inset;
    rect.width -= inset * 2.0f;
    rect.height -= inset * 2.0f;
    if (rect.width < 0.0f) rect.width = 0.0f;
    if (rect.height < 0.0f) rect.height = 0.0f;
    return rect;
}

void kit_ui_hud_style_scale(KitUiHudStyle *style, float scale) {
    if (style == 0 || scale <= 0.0f) {
        return;
    }
    style->panel_corner_radius *= scale;
    style->button_corner_radius *= scale;
}

void kit_ui_hud_style_dark_floating(KitUiHudStyle *out_style) {
    if (out_style == 0) {
        return;
    }
    out_style->panel_fill = (KitRenderColor){12u, 14u, 20u, 218u};
    out_style->button_fill = (KitRenderColor){34u, 40u, 54u, 224u};
    out_style->button_active_fill = (KitRenderColor){72u, 96u, 136u, 224u};
    out_style->button_disabled_fill = (KitRenderColor){24u, 27u, 34u, 224u};
    out_style->readout_fill = (KitRenderColor){20u, 24u, 34u, 210u};
    out_style->text = (KitRenderColor){226u, 232u, 244u, 255u};
    out_style->text_disabled = (KitRenderColor){112u, 118u, 126u, 255u};
    out_style->panel_corner_radius = 14.0f;
    out_style->button_corner_radius = 6.0f;
}

void kit_ui_hud_button_row_config_init(KitUiHudButtonRowConfig *config) {
    if (config == 0) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->max_width = 0.0f;
    config->pad = 8.0f;
    config->gap = 5.0f;
    config->button_height = 28.0f;
    config->bottom_margin = 12.0f;
    config->min_top_y = 80.0f;
    config->readout_min_width = 80.0f;
    config->readout_max_width = 260.0f;
}

float kit_ui_hud_button_row_control_corner_radius(const KitUiHudStyle *style,
                                                  const KitUiHudButtonRowConfig *config) {
    float matched_radius = 0.0f;
    float min_width = 0.0f;
    uint32_t i = 0u;
    if (style == 0 || config == 0) {
        return 0.0f;
    }
    matched_radius = kit_ui_corner_radius_for_inset(style->panel_corner_radius, config->pad);
    if (matched_radius <= 0.0f) {
        matched_radius = style->button_corner_radius;
    }
    for (i = 0u; i < config->button_count && i < KIT_UI_HUD_BUTTON_ROW_MAX; ++i) {
        if (config->button_widths[i] > 0.0f && (min_width <= 0.0f || config->button_widths[i] < min_width)) {
            min_width = config->button_widths[i];
        }
    }
    if (config->readout_min_width > 0.0f &&
        (min_width <= 0.0f || config->readout_min_width < min_width)) {
        min_width = config->readout_min_width;
    }
    if (min_width <= 0.0f) {
        min_width = config->button_height;
    }
    return kit_ui_corner_radius_clamp(matched_radius, min_width, config->button_height);
}

int kit_ui_hud_button_row_layout(const KitUiHudButtonRowConfig *config,
                                 KitUiHudButtonRowLayout *out_layout) {
    float controls_width = 0.0f;
    float max_panel_width = 0.0f;
    float readout_width = 0.0f;
    float panel_width = 0.0f;
    float x = 0.0f;
    uint32_t i = 0u;

    if (config == 0 || out_layout == 0 ||
        config->viewport_width <= 0.0f ||
        config->viewport_height <= 0.0f ||
        config->pad < 0.0f ||
        config->gap < 0.0f ||
        config->button_height <= 0.0f ||
        config->button_count > KIT_UI_HUD_BUTTON_ROW_MAX) {
        return 0;
    }

    memset(out_layout, 0, sizeof(*out_layout));
    out_layout->button_count = config->button_count;

    for (i = 0u; i < config->button_count; ++i) {
        if (config->button_widths[i] <= 0.0f) {
            return 0;
        }
        controls_width += config->button_widths[i];
        if (i > 0u) {
            controls_width += config->gap;
        }
    }

    max_panel_width = config->max_width;
    if (max_panel_width <= 0.0f || max_panel_width > config->viewport_width) {
        max_panel_width = config->viewport_width - 20.0f;
    }
    if (max_panel_width < controls_width + (config->pad * 2.0f)) {
        max_panel_width = controls_width + (config->pad * 2.0f);
    }

    readout_width = max_panel_width - controls_width - (config->pad * 2.0f) - config->gap;
    if (readout_width > config->readout_max_width) {
        readout_width = config->readout_max_width;
    }
    if (readout_width < config->readout_min_width) {
        readout_width = 0.0f;
    }

    panel_width = (config->pad * 2.0f) + controls_width;
    if (readout_width > 0.0f) {
        panel_width += config->gap + readout_width;
        out_layout->has_readout = 1;
    }

    out_layout->panel_rect = (KitRenderRect){
        (config->viewport_width - panel_width) * 0.5f,
        config->viewport_height - config->button_height - (config->pad * 2.0f) -
            config->bottom_margin,
        panel_width,
        config->button_height + (config->pad * 2.0f)
    };
    if (out_layout->panel_rect.y < config->min_top_y) {
        out_layout->panel_rect.y = config->viewport_height - out_layout->panel_rect.height;
    }

    x = out_layout->panel_rect.x + config->pad;
    for (i = 0u; i < config->button_count; ++i) {
        out_layout->button_rects[i] = (KitRenderRect){
            x,
            out_layout->panel_rect.y + config->pad,
            config->button_widths[i],
            config->button_height
        };
        x += config->button_widths[i] + config->gap;
    }

    if (out_layout->has_readout) {
        out_layout->readout_rect = (KitRenderRect){
            x,
            out_layout->panel_rect.y + config->pad,
            kit_ui_hud_clampf(readout_width, 0.0f, config->readout_max_width),
            config->button_height
        };
    }

    return 1;
}
