/*
 * kit_pane.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "kit_pane.h"

#include <stdio.h>

static CoreResult kit_pane_invalid(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static CoreResult kit_pane_not_found(const char *message) {
    CoreResult r = { CORE_ERR_NOT_FOUND, message };
    return r;
}

static float pane_clampf(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static void pane_clear_splitter_hit(CorePaneSplitterHit *hit) {
    if (!hit) {
        return;
    }
    hit->active = false;
    hit->node_index = 0u;
    hit->axis = CORE_PANE_AXIS_HORIZONTAL;
    hit->splitter_bounds = (CorePaneRect){ 0.0f, 0.0f, 0.0f, 0.0f };
    hit->ratio_01 = 0.0f;
    hit->parent_span = 0.0f;
    hit->min_ratio_01 = 0.0f;
    hit->max_ratio_01 = 0.0f;
}

static void pane_shift_splitter_hit(CorePaneSplitterHit *hit, float before_ratio, float after_ratio) {
    float moved = 0.0f;

    if (!hit || !hit->active || hit->parent_span <= 0.0f) {
        return;
    }

    moved = (after_ratio - before_ratio) * hit->parent_span;
    if (hit->axis == CORE_PANE_AXIS_HORIZONTAL) {
        hit->splitter_bounds.x += moved;
    } else {
        hit->splitter_bounds.y += moved;
    }
    hit->ratio_01 = after_ratio;
}

static CoreResult pane_color(KitRenderContext *render_ctx,
                             CoreThemeColorToken token,
                             KitRenderColor *out_color) {
    if (!render_ctx || !out_color) {
        return kit_pane_invalid("invalid pane color request");
    }
    return kit_render_resolve_theme_color(render_ctx, token, out_color);
}

static const char *pane_stable_id_label(uint32_t pane_id) {
    enum { ID_LABEL_SLOTS = 256 };
    static char labels[ID_LABEL_SLOTS][32];
    static uint32_t cursor = 0u;
    uint32_t slot = cursor++ % ID_LABEL_SLOTS;

    (void)snprintf(labels[slot], sizeof(labels[slot]), "#%u", pane_id);
    return labels[slot];
}

static CoreThemeColorToken pane_surface_token_for_state(KitPaneVisualState state) {
    switch (state) {
        case KIT_PANE_STATE_HOVERED:
            return CORE_THEME_COLOR_SURFACE_2;
        case KIT_PANE_STATE_ACTIVE:
            return CORE_THEME_COLOR_SURFACE_1;
        case KIT_PANE_STATE_FOCUSED:
            return CORE_THEME_COLOR_SURFACE_1;
        case KIT_PANE_STATE_DISABLED:
            return CORE_THEME_COLOR_SURFACE_0;
        case KIT_PANE_STATE_NORMAL:
        default:
            return CORE_THEME_COLOR_SURFACE_0;
    }
}

static CoreThemeColorToken pane_border_token_for_state(KitPaneVisualState state,
                                                        int authoring_selected) {
    if (authoring_selected) {
        return CORE_THEME_COLOR_ACCENT_PRIMARY;
    }

    switch (state) {
        case KIT_PANE_STATE_HOVERED:
            return CORE_THEME_COLOR_TEXT_MUTED;
        case KIT_PANE_STATE_ACTIVE:
            return CORE_THEME_COLOR_ACCENT_PRIMARY;
        case KIT_PANE_STATE_FOCUSED:
            return CORE_THEME_COLOR_ACCENT_PRIMARY;
        case KIT_PANE_STATE_DISABLED:
            return CORE_THEME_COLOR_TEXT_MUTED;
        case KIT_PANE_STATE_NORMAL:
        default:
            return CORE_THEME_COLOR_TEXT_MUTED;
    }
}

void kit_pane_style_default(KitPaneStyle *out_style) {
    if (!out_style) {
        return;
    }

    out_style->border_thickness = 1.0f;
    out_style->header_height = 26.0f;
    out_style->corner_radius = 4.0f;
    out_style->title_padding = 8.0f;
    out_style->splitter_thickness = 8.0f;
}

void kit_pane_splitter_interaction_init(KitPaneSplitterInteraction *out_interaction,
                                        float handle_thickness) {
    if (!out_interaction) {
        return;
    }

    out_interaction->handle_thickness = (handle_thickness > 0.0f) ? handle_thickness : 8.0f;
    out_interaction->hover_active = 0;
    out_interaction->drag_active = 0;
    pane_clear_splitter_hit(&out_interaction->hover_hit);
    pane_clear_splitter_hit(&out_interaction->drag_hit);
    out_interaction->drag_last_x = 0.0f;
    out_interaction->drag_last_y = 0.0f;
}

CoreResult kit_pane_splitter_interaction_set_hover(KitPaneSplitterInteraction *interaction,
                                                   const CorePaneNode *nodes,
                                                   uint32_t node_count,
                                                   uint32_t root_index,
                                                   CorePaneRect bounds,
                                                   float point_x,
                                                   float point_y) {
    CorePaneSplitterHit hit = {0};

    if (!interaction || !nodes) {
        return kit_pane_invalid("invalid splitter hover request");
    }
    if (interaction->drag_active) {
        return core_result_ok();
    }

    if (!core_pane_hit_test_splitter(nodes,
                                     node_count,
                                     root_index,
                                     bounds,
                                     interaction->handle_thickness,
                                     point_x,
                                     point_y,
                                     &hit)) {
        interaction->hover_active = 0;
        pane_clear_splitter_hit(&interaction->hover_hit);
        return kit_pane_not_found("splitter not found at point");
    }

    interaction->hover_active = 1;
    interaction->hover_hit = hit;
    return core_result_ok();
}

CoreResult kit_pane_splitter_interaction_set_hover_from_hits(KitPaneSplitterInteraction *interaction,
                                                             const CorePaneSplitterHit *hits,
                                                             uint32_t hit_count,
                                                             float point_x,
                                                             float point_y) {
    CorePaneSplitterHit hit = {0};

    if (!interaction || !hits) {
        return kit_pane_invalid("invalid cached splitter hover request");
    }
    if (interaction->drag_active) {
        return core_result_ok();
    }

    if (!core_pane_hit_test_splitter_hits(hits, hit_count, point_x, point_y, &hit)) {
        interaction->hover_active = 0;
        pane_clear_splitter_hit(&interaction->hover_hit);
        return kit_pane_not_found("cached splitter not found at point");
    }

    interaction->hover_active = 1;
    interaction->hover_hit = hit;
    return core_result_ok();
}

CoreResult kit_pane_splitter_interaction_begin_drag(KitPaneSplitterInteraction *interaction,
                                                    const CorePaneNode *nodes,
                                                    uint32_t node_count,
                                                    uint32_t root_index,
                                                    CorePaneRect bounds,
                                                    float point_x,
                                                    float point_y) {
    CorePaneSplitterHit hit = {0};

    if (!interaction || !nodes) {
        return kit_pane_invalid("invalid splitter drag begin request");
    }

    if (!core_pane_hit_test_splitter(nodes,
                                     node_count,
                                     root_index,
                                     bounds,
                                     interaction->handle_thickness,
                                     point_x,
                                     point_y,
                                     &hit)) {
        return kit_pane_not_found("splitter not found at drag begin point");
    }

    interaction->hover_active = 1;
    interaction->drag_active = 1;
    interaction->hover_hit = hit;
    interaction->drag_hit = hit;
    interaction->drag_last_x = point_x;
    interaction->drag_last_y = point_y;
    return core_result_ok();
}

CoreResult kit_pane_splitter_interaction_begin_drag_from_hits(KitPaneSplitterInteraction *interaction,
                                                              const CorePaneSplitterHit *hits,
                                                              uint32_t hit_count,
                                                              float point_x,
                                                              float point_y) {
    CorePaneSplitterHit hit = {0};

    if (!interaction || !hits) {
        return kit_pane_invalid("invalid cached splitter drag begin request");
    }

    if (!core_pane_hit_test_splitter_hits(hits, hit_count, point_x, point_y, &hit)) {
        return kit_pane_not_found("cached splitter not found at drag begin point");
    }

    interaction->hover_active = 1;
    interaction->drag_active = 1;
    interaction->hover_hit = hit;
    interaction->drag_hit = hit;
    interaction->drag_last_x = point_x;
    interaction->drag_last_y = point_y;
    return core_result_ok();
}

CoreResult kit_pane_splitter_interaction_update_drag(KitPaneSplitterInteraction *interaction,
                                                     CorePaneNode *nodes,
                                                     uint32_t node_count,
                                                     float point_x,
                                                     float point_y,
                                                     int *out_changed) {
    CorePaneNode *node = 0;
    float delta_x = 0.0f;
    float delta_y = 0.0f;
    float before_ratio = 0.0f;
    int changed = 0;

    if (!interaction || !nodes || !out_changed) {
        return kit_pane_invalid("invalid splitter drag update request");
    }
    if (!interaction->drag_active || !interaction->drag_hit.active) {
        return kit_pane_invalid("splitter drag update requires active drag");
    }
    if (interaction->drag_hit.node_index >= node_count) {
        return kit_pane_invalid("splitter drag hit node out of range");
    }

    node = &nodes[interaction->drag_hit.node_index];
    before_ratio = node->ratio_01;
    delta_x = point_x - interaction->drag_last_x;
    delta_y = point_y - interaction->drag_last_y;
    changed = core_pane_apply_splitter_drag(nodes,
                                            node_count,
                                            &interaction->drag_hit,
                                            delta_x,
                                            delta_y) ? 1 : 0;
    interaction->drag_last_x = point_x;
    interaction->drag_last_y = point_y;
    if (changed) {
        pane_shift_splitter_hit(&interaction->drag_hit, before_ratio, node->ratio_01);
        interaction->hover_active = 1;
        interaction->hover_hit = interaction->drag_hit;
    }
    *out_changed = changed;
    return core_result_ok();
}

void kit_pane_splitter_interaction_end_drag(KitPaneSplitterInteraction *interaction) {
    if (!interaction) {
        return;
    }

    interaction->drag_active = 0;
    interaction->hover_active = 0;
    pane_clear_splitter_hit(&interaction->hover_hit);
    pane_clear_splitter_hit(&interaction->drag_hit);
}

int kit_pane_splitter_interaction_current(const KitPaneSplitterInteraction *interaction,
                                          CorePaneRect *out_bounds,
                                          int *out_hovered,
                                          int *out_active) {
    const CorePaneSplitterHit *hit = 0;

    if (!interaction) {
        return 0;
    }

    if (interaction->drag_active && interaction->hover_active) {
        hit = &interaction->drag_hit;
    } else if (interaction->hover_active) {
        hit = &interaction->hover_hit;
    } else {
        return 0;
    }

    if (!hit->active) {
        return 0;
    }
    if (out_bounds) {
        *out_bounds = hit->splitter_bounds;
    }
    if (out_hovered) {
        *out_hovered = interaction->hover_active ? 1 : 0;
    }
    if (out_active) {
        *out_active = interaction->drag_active ? 1 : 0;
    }
    return 1;
}

CoreResult kit_pane_draw_chrome(KitRenderContext *render_ctx,
                                KitRenderFrame *frame,
                                const KitPaneStyle *style,
                                const KitPaneChrome *chrome) {
    KitPaneStyle local_style;
    KitRenderColor border_color;
    KitRenderColor surface_color;
    KitRenderColor header_color;
    KitRenderColor title_color;
    KitRenderRect inner_rect;
    KitRenderRect header_rect;
    KitRenderRectCommand rect_cmd;
    KitRenderTextCommand text_cmd;
    CoreResult result;
    const char *title_text;

    if (!render_ctx || !frame || !chrome) {
        return kit_pane_invalid("invalid pane chrome request");
    }
    if (chrome->bounds.width <= 0.0f || chrome->bounds.height <= 0.0f) {
        return kit_pane_invalid("invalid pane bounds");
    }

    if (style) {
        local_style = *style;
    } else {
        kit_pane_style_default(&local_style);
    }

    local_style.border_thickness = pane_clampf(local_style.border_thickness, 1.0f, 8.0f);
    local_style.header_height = pane_clampf(local_style.header_height, 14.0f, chrome->bounds.height);
    local_style.corner_radius = pane_clampf(local_style.corner_radius, 0.0f, 12.0f);
    local_style.title_padding = pane_clampf(local_style.title_padding, 2.0f, 20.0f);

    result = pane_color(render_ctx,
                        pane_border_token_for_state(chrome->state, chrome->authoring_selected),
                        &border_color);
    if (result.code != CORE_OK) {
        return result;
    }
    result = pane_color(render_ctx, pane_surface_token_for_state(chrome->state), &surface_color);
    if (result.code != CORE_OK) {
        return result;
    }
    result = pane_color(render_ctx, CORE_THEME_COLOR_SURFACE_1, &header_color);
    if (result.code != CORE_OK) {
        return result;
    }
    result = pane_color(render_ctx, CORE_THEME_COLOR_TEXT_PRIMARY, &title_color);
    if (result.code != CORE_OK) {
        return result;
    }

    rect_cmd.rect = chrome->bounds;
    rect_cmd.corner_radius = local_style.corner_radius;
    rect_cmd.color = border_color;
    rect_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_rect(frame, &rect_cmd);
    if (result.code != CORE_OK) {
        return result;
    }

    inner_rect = chrome->bounds;
    inner_rect.x += local_style.border_thickness;
    inner_rect.y += local_style.border_thickness;
    inner_rect.width -= local_style.border_thickness * 2.0f;
    inner_rect.height -= local_style.border_thickness * 2.0f;
    if (inner_rect.width <= 0.0f || inner_rect.height <= 0.0f) {
        return core_result_ok();
    }

    rect_cmd.rect = inner_rect;
    rect_cmd.corner_radius = local_style.corner_radius;
    rect_cmd.color = surface_color;
    result = kit_render_push_rect(frame, &rect_cmd);
    if (result.code != CORE_OK) {
        return result;
    }

    if (!chrome->show_header) {
        return core_result_ok();
    }

    header_rect = inner_rect;
    header_rect.height = local_style.header_height;
    rect_cmd.rect = header_rect;
    rect_cmd.corner_radius = local_style.corner_radius;
    rect_cmd.color = header_color;
    result = kit_render_push_rect(frame, &rect_cmd);
    if (result.code != CORE_OK) {
        return result;
    }

    title_text = chrome->title ? chrome->title : "PANE";
    text_cmd.origin.x = header_rect.x + local_style.title_padding;
    text_cmd.origin.y = header_rect.y + (local_style.title_padding * 0.5f);
    text_cmd.text = title_text;
    text_cmd.font_role = CORE_FONT_ROLE_UI_MEDIUM;
    text_cmd.text_tier = CORE_FONT_TEXT_SIZE_CAPTION;
    text_cmd.color_token = CORE_THEME_COLOR_TEXT_PRIMARY;
    text_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_text(frame, &text_cmd);
    if (result.code != CORE_OK) {
        return result;
    }

    if (chrome->show_id) {
        text_cmd.origin.x = header_rect.x + header_rect.width - 48.0f;
        text_cmd.origin.y = header_rect.y + (local_style.title_padding * 0.5f);
        text_cmd.text = pane_stable_id_label(chrome->pane_id);
        text_cmd.font_role = CORE_FONT_ROLE_UI_REGULAR;
        text_cmd.text_tier = CORE_FONT_TEXT_SIZE_CAPTION;
        text_cmd.color_token = CORE_THEME_COLOR_TEXT_MUTED;
        result = kit_render_push_text(frame, &text_cmd);
        if (result.code != CORE_OK) {
            return result;
        }
    }

    return core_result_ok();
}

CoreResult kit_pane_draw_splitter(KitRenderContext *render_ctx,
                                  KitRenderFrame *frame,
                                  KitRenderRect bounds,
                                  int hovered,
                                  int active) {
    KitRenderColor color;
    KitRenderRectCommand rect_cmd;
    CoreThemeColorToken token = CORE_THEME_COLOR_TEXT_MUTED;
    CoreResult result;

    if (!render_ctx || !frame) {
        return kit_pane_invalid("invalid splitter draw request");
    }
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return kit_pane_invalid("invalid splitter bounds");
    }

    if (active) {
        token = CORE_THEME_COLOR_ACCENT_PRIMARY;
    } else if (hovered) {
        token = CORE_THEME_COLOR_TEXT_PRIMARY;
    }

    result = pane_color(render_ctx, token, &color);
    if (result.code != CORE_OK) {
        return result;
    }

    rect_cmd.rect = bounds;
    rect_cmd.corner_radius = 2.0f;
    rect_cmd.color = color;
    rect_cmd.transform = kit_render_identity_transform();
    return kit_render_push_rect(frame, &rect_cmd);
}
