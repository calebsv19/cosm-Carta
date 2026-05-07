#include "app/app_internal.h"
#include "app/app_ui_internal.h"

#include "core_font.h"
#include "core_theme.h"
#include "kit_workspace_authoring_ui.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"

#include <stdio.h>
#include <string.h>

static SDL_FRect map_forge_authoring_rect_from_core(CorePaneRect rect) {
    return (SDL_FRect){rect.x, rect.y, rect.width, rect.height};
}

static SDL_FRect map_forge_authoring_rect_from_kit(KitRenderRect rect) {
    return (SDL_FRect){rect.x, rect.y, rect.width, rect.height};
}

static SDL_Color map_forge_authoring_alpha(SDL_Color color, uint8_t alpha) {
    color.a = alpha;
    return color;
}

static int map_forge_authoring_rect_visible(const SDL_FRect *rect) {
    return rect && rect->w > 1.0f && rect->h > 1.0f;
}

static void map_forge_authoring_draw_rect(Renderer *renderer,
                                          const SDL_FRect *rect,
                                          SDL_Color fill,
                                          SDL_Color border) {
    if (!renderer || !map_forge_authoring_rect_visible(rect)) return;
    renderer_set_draw_color(renderer, fill.r, fill.g, fill.b, fill.a);
    renderer_fill_rect(renderer, rect);
    renderer_set_draw_color(renderer, border.r, border.g, border.b, border.a);
    renderer_draw_rect(renderer, rect);
}

static void map_forge_authoring_draw_label(AppState *app,
                                           const SDL_FRect *host_rect,
                                           const char *label,
                                           const char *detail,
                                           const MapForgeThemePalette *palette) {
    SDL_FRect label_rect;
    SDL_Color fill;
    SDL_Color border;
    SDL_Color text;
    SDL_Color muted;
    int label_w;
    if (!app || !host_rect || !palette || !label || !label[0]) return;
    if (!map_forge_authoring_rect_visible(host_rect)) return;

    label_w = ui_measure_text_width(label, 1.0f);
    if (label_w < 0) label_w = 0;
    label_rect = (SDL_FRect){
        host_rect->x + 8.0f,
        host_rect->y + 8.0f,
        (float)label_w + 18.0f,
        24.0f
    };
    if (label_rect.w < 128.0f) label_rect.w = 128.0f;
    if (label_rect.w > host_rect->w - 16.0f) label_rect.w = host_rect->w - 16.0f;
    if (label_rect.w <= 1.0f) return;

    fill = map_forge_authoring_alpha(palette->overlay_fill, 238u);
    border = map_forge_authoring_alpha(palette->overlay_accent, 232u);
    text = palette->text_primary;
    muted = palette->text_muted;
    map_forge_authoring_draw_rect(&app->renderer, &label_rect, fill, border);
    ui_draw_text(&app->renderer,
                 (int)(label_rect.x + 8.0f),
                 (int)(label_rect.y + 4.0f),
                 label,
                 text,
                 1.0f);
    if (detail && detail[0] && host_rect->w > 220.0f && host_rect->h > 70.0f) {
        ui_draw_text_clipped(&app->renderer,
                             (int)(host_rect->x + 14.0f),
                             (int)(host_rect->y + 40.0f),
                             detail,
                             muted,
                             1.0f,
                             (int)(host_rect->w - 28.0f));
    }
}

static SDL_FRect map_forge_authoring_fallback_route_rect(const AppState *app) {
    float width = 320.0f;
    float height = 130.0f;
    if (!app) return (SDL_FRect){0};
    if ((float)app->width < width + 24.0f) width = (float)app->width - 24.0f;
    return (SDL_FRect){
        (float)app->width - width - 12.0f,
        APP_HEADER_HEIGHT + 12.0f,
        width,
        height
    };
}

static SDL_FRect map_forge_authoring_fallback_ingest_rect(const AppState *app) {
    float width = 360.0f;
    float height = 170.0f;
    if (!app) return (SDL_FRect){0};
    if ((float)app->width < width + 24.0f) width = (float)app->width - 24.0f;
    return (SDL_FRect){
        12.0f,
        (float)app->height - height - 12.0f,
        width,
        height
    };
}

static SDL_FRect map_forge_authoring_fallback_diagnostics_rect(const AppState *app) {
    float width = 360.0f;
    float height = 140.0f;
    if (!app) return (SDL_FRect){0};
    if ((float)app->width < width + 24.0f) width = (float)app->width - 24.0f;
    return (SDL_FRect){
        (float)app->width - width - 12.0f,
        (float)app->height - height - 12.0f,
        width,
        height
    };
}

static void map_forge_authoring_draw_surface_inventory(AppState *app,
                                                       const MapForgeThemePalette *palette) {
    SDL_Color border;
    SDL_Color fill;
    SDL_FRect header;
    SDL_FRect map_view;
    SDL_FRect route_panel;
    SDL_FRect ingest_panel;
    SDL_FRect diagnostics;
    if (!app || !palette || app->width <= 0 || app->height <= 0) return;

    border = map_forge_authoring_alpha(palette->overlay_accent, 230u);
    fill = map_forge_authoring_alpha(palette->overlay_fill, 36u);

    header = (SDL_FRect){0.0f, 0.0f, (float)app->width, APP_HEADER_HEIGHT};
    map_view = (SDL_FRect){
        8.0f,
        APP_HEADER_HEIGHT + 8.0f,
        (float)app->width - 16.0f,
        (float)app->height - APP_HEADER_HEIGHT - 16.0f
    };
    route_panel = app->ui_state_bridge.hud_route_panel_collapsed
        ? app->ui_state_bridge.hud_route_panel_handle_rect
        : app->ui_state_bridge.hud_route_panel_rect;
    ingest_panel = app->ui_state_bridge.hud_ingest_panel_collapsed
        ? app->ui_state_bridge.hud_ingest_handle_rect
        : app->ui_state_bridge.hud_ingest_panel_rect;
    diagnostics = app->ui_state_bridge.hud_layer_debug_collapsed
        ? app->ui_state_bridge.hud_layer_debug_handle_rect
        : app->ui_state_bridge.hud_layer_debug_panel_rect;

    if (!map_forge_authoring_rect_visible(&route_panel)) {
        route_panel = map_forge_authoring_fallback_route_rect(app);
    }
    if (!map_forge_authoring_rect_visible(&ingest_panel)) {
        ingest_panel = map_forge_authoring_fallback_ingest_rect(app);
    }
    if (!map_forge_authoring_rect_visible(&diagnostics)) {
        diagnostics = map_forge_authoring_fallback_diagnostics_rect(app);
    }

    map_forge_authoring_draw_rect(&app->renderer, &map_view, fill, border);
    map_forge_authoring_draw_label(app, &map_view, "P2 Map Viewport", "module: map canvas / runtime camera", palette);

    map_forge_authoring_draw_rect(&app->renderer, &header, fill, border);
    map_forge_authoring_draw_label(app, &header, "P1 Runtime Header", "module: route controls / layer strip", palette);

    map_forge_authoring_draw_rect(&app->renderer, &route_panel, fill, border);
    map_forge_authoring_draw_label(app, &route_panel, "P3 Route Panel", "module: route comparison / playback", palette);

    map_forge_authoring_draw_rect(&app->renderer, &ingest_panel, fill, border);
    map_forge_authoring_draw_label(app, &ingest_panel, "P4 Ingest Panel", "module: region source manager", palette);

    map_forge_authoring_draw_rect(&app->renderer, &diagnostics, fill, border);
    map_forge_authoring_draw_label(app, &diagnostics, "P5 Diagnostics", "module: tile/runtime readout", palette);
}

static void map_forge_authoring_draw_button(AppState *app,
                                            const KitWorkspaceAuthoringOverlayButton *button,
                                            const MapForgeThemePalette *palette) {
    SDL_FRect rect;
    SDL_Color fill;
    SDL_Color border;
    SDL_Color text;
    if (!app || !button || !palette || !button->visible) return;

    rect = map_forge_authoring_rect_from_core(button->rect);
    fill = button->id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY
        ? map_forge_authoring_alpha(palette->button_active_success, 236u)
        : map_forge_authoring_alpha(palette->button_fill, 236u);
    if (button->id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL) {
        fill = map_forge_authoring_alpha(palette->badge_fill, 238u);
    } else if (button->id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE) {
        fill = map_forge_authoring_alpha(palette->button_active_primary, 230u);
    }
    border = map_forge_authoring_alpha(palette->button_outline, 244u);
    text = palette->text_primary;
    if (!button->enabled) {
        fill.a = 130u;
        border.a = 140u;
        text.a = 150u;
    }

    map_forge_authoring_draw_rect(&app->renderer, &rect, fill, border);
    ui_draw_text_clipped(&app->renderer,
                         (int)(rect.x + 7.0f),
                         (int)(rect.y + 3.0f),
                         button->label ? button->label : "",
                         text,
                         1.0f,
                         (int)(rect.w - 12.0f));
}

static int map_forge_authoring_font_theme_button_selected(
    KitWorkspaceAuthoringFontThemeButtonId button_id,
    const char *current_font_preset,
    const char *current_theme_preset) {
    CoreFontPresetId font_id;
    CoreThemePresetId theme_id;
    const char *name;
    if (kit_workspace_authoring_ui_font_theme_button_font_preset_id(button_id, &font_id)) {
        name = core_font_preset_name(font_id);
        return name && current_font_preset && strcmp(name, current_font_preset) == 0;
    }
    if (kit_workspace_authoring_ui_font_theme_button_theme_preset_id(button_id, &theme_id)) {
        name = core_theme_preset_name(theme_id);
        return name && current_theme_preset && strcmp(name, current_theme_preset) == 0;
    }
    return 0;
}

static void map_forge_authoring_draw_font_theme_button(
    AppState *app,
    KitWorkspaceAuthoringFontThemeButtonId button_id,
    KitRenderRect kit_rect,
    const MapForgeThemePalette *palette,
    const char *current_font_preset,
    const char *current_theme_preset) {
    SDL_FRect rect;
    SDL_Color fill;
    SDL_Color border;
    SDL_Color text;
    int selected;
    uint8_t enabled;
    if (!app || !palette || button_id == KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE) return;

    rect = map_forge_authoring_rect_from_kit(kit_rect);
    enabled = kit_workspace_authoring_ui_font_theme_button_enabled(button_id);
    selected = map_forge_authoring_font_theme_button_selected(button_id,
                                                             current_font_preset,
                                                             current_theme_preset);
    fill = selected
        ? map_forge_authoring_alpha(palette->button_active_primary, 236u)
        : map_forge_authoring_alpha(palette->button_fill, 226u);
    border = map_forge_authoring_alpha(selected ? palette->overlay_accent : palette->button_outline, 244u);
    text = palette->text_primary;
    if (!enabled) {
        fill.a = 96u;
        border.a = 110u;
        text.a = 135u;
    }

    map_forge_authoring_draw_rect(&app->renderer, &rect, fill, border);
    ui_draw_text_clipped(&app->renderer,
                         (int)(rect.x + 8.0f),
                         (int)(rect.y + 4.0f),
                         kit_workspace_authoring_ui_font_theme_button_label(button_id),
                         text,
                         1.0f,
                         (int)(rect.w - 14.0f));
}

static void map_forge_authoring_draw_section_text(AppState *app,
                                                  KitRenderRect kit_rect,
                                                  const char *title,
                                                  const char *detail,
                                                  const MapForgeThemePalette *palette) {
    SDL_FRect rect;
    if (!app || !palette || !title) return;
    rect = map_forge_authoring_rect_from_kit(kit_rect);
    ui_draw_text_clipped(&app->renderer,
                         (int)(rect.x + 14.0f),
                         (int)(rect.y + 12.0f),
                         title,
                         palette->text_primary,
                         1.0f,
                         (int)(rect.w - 28.0f));
    if (detail && detail[0]) {
        ui_draw_text_clipped(&app->renderer,
                             (int)(rect.x + 14.0f),
                             (int)(rect.y + 36.0f),
                             detail,
                             palette->text_muted,
                             1.0f,
                             (int)(rect.w - 28.0f));
    }
}

static void map_forge_authoring_draw_font_theme_overlay(AppState *app,
                                                        const MapForgeThemePalette *palette) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    SDL_FRect screen;
    SDL_FRect panel;
    SDL_FRect section;
    SDL_Color screen_fill;
    SDL_Color section_fill;
    SDL_Color border;
    char font_preset[64] = "unknown";
    char theme_preset[64] = "unknown";
    char detail[192];
    int step = 0;
    int pct = 0;
    uint32_t i;
    const MapForgeWorkspaceAuthoringHostState *host;
    if (!app || !palette || app->width <= 0 || app->height <= 0) return;
    if (!kit_workspace_authoring_ui_font_theme_build_layout(NULL, app->width, app->height, &layout)) return;

    host = &app->ui_state_bridge.workspace_authoring;
    (void)mapforge_shared_font_current_preset(font_preset, sizeof(font_preset));
    (void)mapforge_shared_theme_current_preset(theme_preset, sizeof(theme_preset));
    step = mapforge_shared_font_zoom_step();
    pct = 100 + (step * 10);
    if (pct < 60) pct = 60;
    if (pct > 180) pct = 180;

    screen = (SDL_FRect){0.0f, 0.0f, (float)app->width, (float)app->height};
    panel = map_forge_authoring_rect_from_kit(layout.panel);
    screen_fill = map_forge_authoring_alpha(palette->overlay_fill, 250u);
    section_fill = map_forge_authoring_alpha(palette->button_fill, 208u);
    border = map_forge_authoring_alpha(palette->overlay_outline, 236u);

    renderer_set_draw_color(&app->renderer, screen_fill.r, screen_fill.g, screen_fill.b, screen_fill.a);
    renderer_fill_rect(&app->renderer, &screen);
    map_forge_authoring_draw_rect(&app->renderer, &panel, screen_fill, border);

    ui_draw_text_clipped(&app->renderer,
                         (int)(panel.x + 10.0f),
                         (int)(panel.y + 10.0f),
                         "Font/Theme Overlay",
                         palette->text_primary,
                         1.0f,
                         (int)(panel.w - 20.0f));

    section = map_forge_authoring_rect_from_kit(layout.font_preset_section);
    map_forge_authoring_draw_rect(&app->renderer, &section, section_fill, border);
    snprintf(detail, sizeof(detail), "Font Preset: %s", font_preset);
    map_forge_authoring_draw_section_text(app, layout.font_preset_section, detail, "", palette);
    for (i = 0u; i < layout.font_preset_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT + i);
        map_forge_authoring_draw_font_theme_button(app,
                                                   id,
                                                   layout.font_preset_buttons[i],
                                                   palette,
                                                   font_preset,
                                                   theme_preset);
    }

    section = map_forge_authoring_rect_from_kit(layout.text_size_section);
    map_forge_authoring_draw_rect(&app->renderer, &section, section_fill, border);
    snprintf(detail, sizeof(detail), "Text Size step:%d (%d%%)", step, pct);
    map_forge_authoring_draw_section_text(app, layout.text_size_section, detail, "", palette);
    map_forge_authoring_draw_font_theme_button(app,
                                               KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC,
                                               layout.text_size_dec_button,
                                               palette,
                                               font_preset,
                                               theme_preset);
    map_forge_authoring_draw_font_theme_button(app,
                                               KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC,
                                               layout.text_size_inc_button,
                                               palette,
                                               font_preset,
                                               theme_preset);
    map_forge_authoring_draw_font_theme_button(app,
                                               KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET,
                                               layout.text_size_reset_button,
                                               palette,
                                               font_preset,
                                               theme_preset);
    map_forge_authoring_draw_rect(&app->renderer,
                                  &(SDL_FRect){layout.text_size_value_chip.x,
                                               layout.text_size_value_chip.y,
                                               layout.text_size_value_chip.width,
                                               layout.text_size_value_chip.height},
                                  map_forge_authoring_alpha(palette->button_active_primary, 226u),
                                  map_forge_authoring_alpha(palette->button_outline, 244u));
    ui_draw_text_clipped(&app->renderer,
                         (int)(layout.text_size_value_chip.x + 8.0f),
                         (int)(layout.text_size_value_chip.y + 4.0f),
                         detail,
                         palette->text_primary,
                         1.0f,
                         (int)(layout.text_size_value_chip.width - 14.0f));

    section = map_forge_authoring_rect_from_kit(layout.theme_preset_section);
    map_forge_authoring_draw_rect(&app->renderer, &section, section_fill, border);
    snprintf(detail, sizeof(detail), "Theme Preset: %s", theme_preset);
    map_forge_authoring_draw_section_text(app,
                                          layout.theme_preset_section,
                                          detail,
                                          "Click a preset to preview live; Apply persists in the next slice.",
                                          palette);
    for (i = 0u; i < layout.theme_preset_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_DAW_DEFAULT + i);
        map_forge_authoring_draw_font_theme_button(app,
                                                   id,
                                                   layout.theme_preset_buttons[i],
                                                   palette,
                                                   font_preset,
                                                   theme_preset);
    }

    section = map_forge_authoring_rect_from_kit(layout.custom_theme_section);
    map_forge_authoring_draw_rect(&app->renderer, &section, section_fill, border);
    map_forge_authoring_draw_section_text(
        app,
        layout.custom_theme_section,
        "Custom Presets",
        host->font_theme_status[0]
            ? host->font_theme_status
            : "MapForge exposes custom theme slots as stubs until the theme editor is promoted.",
        palette);
    for (i = 0u; i < layout.custom_theme_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_CREATE_STUB + i);
        map_forge_authoring_draw_font_theme_button(app,
                                                   id,
                                                   layout.custom_theme_buttons[i],
                                                   palette,
                                                   font_preset,
                                                   theme_preset);
    }
}

static void map_forge_authoring_draw_controls(AppState *app,
                                              const MapForgeThemePalette *palette) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count = 0u;
    uint32_t i;
    if (!app || !palette || app->width <= 0) return;

    count = kit_workspace_authoring_ui_build_overlay_buttons(
        app->width,
        map_forge_workspace_authoring_host_active(&app->ui_state_bridge.workspace_authoring),
        map_forge_workspace_authoring_host_surface_overlay_active(&app->ui_state_bridge.workspace_authoring),
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    for (i = 0u; i < count; ++i) {
        map_forge_authoring_draw_button(app, &buttons[i], palette);
    }
}

void app_draw_workspace_authoring_overlay(AppState *app) {
    MapForgeThemePalette palette;
    if (!app ||
        !map_forge_workspace_authoring_host_active(&app->ui_state_bridge.workspace_authoring)) {
        return;
    }

    palette = app_ui_theme_palette();
    if (map_forge_workspace_authoring_host_surface_overlay_active(
            &app->ui_state_bridge.workspace_authoring)) {
        map_forge_authoring_draw_surface_inventory(app, &palette);
    } else if (map_forge_workspace_authoring_host_font_theme_overlay_active(
                   &app->ui_state_bridge.workspace_authoring)) {
        map_forge_authoring_draw_font_theme_overlay(app, &palette);
    }
    map_forge_authoring_draw_controls(app, &palette);
}
