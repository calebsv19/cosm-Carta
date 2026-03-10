#include "ui/shared_theme_font_adapter.h"

#include "core_font.h"
#include "core_theme.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool g_theme_runtime_initialized = false;
static CoreThemePresetId g_theme_runtime_preset = CORE_THEME_PRESET_DARK_DEFAULT;
static const char *k_theme_persist_path = "config/theme_preset.txt";
static const CoreThemePresetId k_theme_cycle_order[] = {
    CORE_THEME_PRESET_DAW_DEFAULT,
    CORE_THEME_PRESET_MAP_FORGE_DEFAULT,
    CORE_THEME_PRESET_DARK_DEFAULT,
    CORE_THEME_PRESET_LIGHT_DEFAULT,
    CORE_THEME_PRESET_IDE_GRAY,
    CORE_THEME_PRESET_GREYSCALE
};

static bool parse_bool_env(const char *value, bool *out_value) {
    char lowered[16];
    size_t i = 0;
    if (!value || !value[0] || !out_value) {
        return false;
    }
    for (; value[i] && i < sizeof(lowered) - 1; ++i) {
        lowered[i] = (char)tolower((unsigned char)value[i]);
    }
    lowered[i] = '\0';

    if (strcmp(lowered, "1") == 0 || strcmp(lowered, "true") == 0 || strcmp(lowered, "yes") == 0 ||
        strcmp(lowered, "on") == 0) {
        *out_value = true;
        return true;
    }
    if (strcmp(lowered, "0") == 0 || strcmp(lowered, "false") == 0 || strcmp(lowered, "no") == 0 ||
        strcmp(lowered, "off") == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

static uint8_t add_u8_saturating(uint8_t value, uint8_t delta) {
    unsigned int sum = (unsigned int)value + (unsigned int)delta;
    return (uint8_t)(sum > 255u ? 255u : sum);
}

static bool is_shared_toggle_enabled(const char *override_var_name) {
    const char *override = getenv(override_var_name);
    bool out_value = false;
    if (parse_bool_env(override, &out_value)) {
        return out_value;
    }
    if (parse_bool_env(getenv("MAPFORGE_USE_SHARED_THEME_FONT"), &out_value)) {
        return out_value;
    }
    return true;
}

static SDL_Color theme_color_or_default(const CoreThemePreset *preset,
                                        CoreThemeColorToken token,
                                        SDL_Color fallback) {
    CoreThemeColor raw = {0};
    CoreResult r = core_theme_get_color(preset, token, &raw);
    if (r.code != CORE_OK) {
        return fallback;
    }
    return (SDL_Color){raw.r, raw.g, raw.b, raw.a};
}

static int stat_path_exists(const char *path, void *user) {
    struct stat st;
    (void)user;
    return path && stat(path, &st) == 0;
}

static void trim_trailing_whitespace(char *text) {
    size_t len;
    if (!text) {
        return;
    }
    len = strlen(text);
    while (len > 0) {
        char c = text[len - 1];
        if (c != '\n' && c != '\r' && c != '\t' && c != ' ') {
            break;
        }
        text[len - 1] = '\0';
        --len;
    }
}

static void theme_runtime_init_if_needed(void) {
    const char *preset_name;
    CoreThemePresetId resolved_id;
    if (g_theme_runtime_initialized) {
        return;
    }
    preset_name = getenv("MAPFORGE_THEME_PRESET");
    if (preset_name && preset_name[0] &&
        core_theme_preset_id_from_name(preset_name, &resolved_id).code == CORE_OK) {
        g_theme_runtime_preset = resolved_id;
    } else {
        g_theme_runtime_preset = CORE_THEME_PRESET_DARK_DEFAULT;
    }
    g_theme_runtime_initialized = true;
}

static bool resolve_theme_preset(CoreThemePreset *out_preset) {
    CoreResult r;
    theme_runtime_init_if_needed();
    r = core_theme_get_preset(g_theme_runtime_preset, out_preset);
    if (r.code == CORE_OK) {
        return true;
    }
    r = core_theme_get_preset(CORE_THEME_PRESET_DARK_DEFAULT, out_preset);
    return r.code == CORE_OK;
}

static bool copy_existing_path(char *out_path, size_t out_path_size, const char *candidate) {
    if (!out_path || out_path_size == 0 || !candidate || !candidate[0]) {
        return false;
    }
    if (stat_path_exists(candidate, NULL)) {
        strncpy(out_path, candidate, out_path_size - 1);
        out_path[out_path_size - 1] = '\0';
        return true;
    }
    return false;
}

static bool resolve_existing_font_path(const CoreFontRoleSpec *role,
                                       char *out_path,
                                       size_t out_path_size) {
    char adjusted[384];
    const char *candidates[2];
    size_t i;
    candidates[0] = role ? role->primary_path : NULL;
    candidates[1] = role ? role->fallback_path : NULL;

    for (i = 0; i < 2; ++i) {
        const char *raw = candidates[i];
        const char *base = NULL;
        if (!raw || !raw[0]) {
            continue;
        }

        if (copy_existing_path(out_path, out_path_size, raw)) {
            return true;
        }

        if (strncmp(raw, "shared/", 7) == 0) {
            snprintf(adjusted, sizeof(adjusted), "../%s", raw);
            if (copy_existing_path(out_path, out_path_size, adjusted)) {
                return true;
            }
        }

        if (strncmp(raw, "include/fonts/Montserrat/", 25) == 0 && strstr(raw, "/static/") == NULL) {
            base = strrchr(raw, '/');
            if (base && base[1]) {
                snprintf(adjusted, sizeof(adjusted), "../line_drawing/include/fonts/Montserrat/static/%s", base + 1);
                if (copy_existing_path(out_path, out_path_size, adjusted)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool mapforge_shared_theme_resolve_palette(MapForgeThemePalette *out_palette) {
    CoreThemePreset preset = {0};
    SDL_Color surface0;
    SDL_Color surface1;
    SDL_Color surface2;
    SDL_Color text_primary;
    SDL_Color text_muted;
    SDL_Color accent_primary;
    SDL_Color status_ok;
    SDL_Color status_warn;

    if (!out_palette || !is_shared_toggle_enabled("MAPFORGE_USE_SHARED_THEME")) {
        return false;
    }
    if (!resolve_theme_preset(&preset)) {
        return false;
    }

    surface0 = theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_0, (SDL_Color){20, 20, 28, 255});
    surface1 = theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_1, (SDL_Color){30, 35, 46, 230});
    surface2 = theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_2, (SDL_Color){80, 90, 110, 220});
    text_primary = theme_color_or_default(&preset, CORE_THEME_COLOR_TEXT_PRIMARY, (SDL_Color){225, 230, 240, 255});
    text_muted = theme_color_or_default(&preset, CORE_THEME_COLOR_TEXT_MUTED, (SDL_Color){205, 212, 225, 255});
    accent_primary = theme_color_or_default(&preset, CORE_THEME_COLOR_ACCENT_PRIMARY, (SDL_Color){60, 140, 220, 220});
    status_ok = theme_color_or_default(&preset, CORE_THEME_COLOR_STATUS_OK, (SDL_Color){60, 200, 130, 220});
    status_warn = theme_color_or_default(&preset, CORE_THEME_COLOR_STATUS_WARN, (SDL_Color){110, 180, 255, 230});

    out_palette->background_clear = surface0;
    out_palette->header_fill = surface0;
    out_palette->header_outline = surface2;
    out_palette->button_fill = surface1;
    out_palette->button_outline = surface2;
    out_palette->button_active_primary = accent_primary;
    out_palette->button_active_success = status_ok;
    out_palette->badge_fill = surface1;
    out_palette->badge_outline = surface2;
    out_palette->text_primary = text_primary;
    out_palette->text_muted = text_muted;
    out_palette->progress_fill = status_warn;
    out_palette->progress_bg = (SDL_Color){surface1.r, surface1.g, surface1.b, 220};
    out_palette->chip_idle_fill = (SDL_Color){surface1.r, surface1.g, surface1.b, 200};
    out_palette->chip_idle_outline = surface2;
    out_palette->chip_loading_fill = (SDL_Color){surface0.r, add_u8_saturating(surface0.g, 10u), add_u8_saturating(surface0.b, 24u), 220};
    out_palette->chip_loading_outline = status_warn;
    out_palette->chip_ready_fill = (SDL_Color){surface0.r, add_u8_saturating(surface0.g, 24u), add_u8_saturating(surface0.b, 16u), 220};
    out_palette->chip_ready_outline = status_ok;
    out_palette->overlay_fill = (SDL_Color){surface0.r, surface0.g, surface0.b, 224};
    out_palette->overlay_outline = surface2;
    out_palette->overlay_accent = status_warn;
    out_palette->route_panel_fill = (SDL_Color){surface0.r, surface0.g, surface0.b, 210};
    out_palette->route_panel_outline = surface2;
    out_palette->route_progress_fill = status_warn;
    out_palette->playback_marker_fill = accent_primary;
    return true;
}

bool mapforge_shared_theme_set_preset(const char *preset_name) {
    CoreThemePresetId id;
    if (!preset_name || !preset_name[0]) {
        return false;
    }
    if (core_theme_preset_id_from_name(preset_name, &id).code != CORE_OK) {
        return false;
    }
    g_theme_runtime_preset = id;
    g_theme_runtime_initialized = true;
    return true;
}

bool mapforge_shared_theme_current_preset(char *out_name, size_t out_name_size) {
    const char *name;
    if (!out_name || out_name_size == 0) {
        return false;
    }
    theme_runtime_init_if_needed();
    name = core_theme_preset_name(g_theme_runtime_preset);
    if (!name || !name[0]) {
        return false;
    }
    strncpy(out_name, name, out_name_size - 1);
    out_name[out_name_size - 1] = '\0';
    return true;
}

bool mapforge_shared_theme_load_persisted(void) {
    FILE *file;
    char preset_name[64];
    if (!stat_path_exists(k_theme_persist_path, NULL)) {
        return false;
    }
    file = fopen(k_theme_persist_path, "rb");
    if (!file) {
        return false;
    }
    preset_name[0] = '\0';
    if (!fgets(preset_name, (int)sizeof(preset_name), file)) {
        fclose(file);
        return false;
    }
    fclose(file);
    trim_trailing_whitespace(preset_name);
    if (!preset_name[0]) {
        return false;
    }
    return mapforge_shared_theme_set_preset(preset_name);
}

bool mapforge_shared_theme_save_persisted(void) {
    FILE *file;
    char preset_name[64];
    if (!mapforge_shared_theme_current_preset(preset_name, sizeof(preset_name))) {
        return false;
    }
    file = fopen(k_theme_persist_path, "wb");
    if (!file) {
        return false;
    }
    if (fputs(preset_name, file) < 0 || fputc('\n', file) == EOF) {
        fclose(file);
        return false;
    }
    if (fclose(file) != 0) {
        return false;
    }
    return true;
}

bool mapforge_shared_theme_cycle_next(void) {
    size_t i;
    size_t n = sizeof(k_theme_cycle_order) / sizeof(k_theme_cycle_order[0]);
    theme_runtime_init_if_needed();
    for (i = 0; i < n; ++i) {
        if (k_theme_cycle_order[i] == g_theme_runtime_preset) {
            g_theme_runtime_preset = k_theme_cycle_order[(i + 1u) % n];
            return true;
        }
    }
    g_theme_runtime_preset = k_theme_cycle_order[0];
    return true;
}

bool mapforge_shared_theme_cycle_prev(void) {
    size_t i;
    size_t n = sizeof(k_theme_cycle_order) / sizeof(k_theme_cycle_order[0]);
    theme_runtime_init_if_needed();
    for (i = 0; i < n; ++i) {
        if (k_theme_cycle_order[i] == g_theme_runtime_preset) {
            g_theme_runtime_preset = k_theme_cycle_order[(i + n - 1u) % n];
            return true;
        }
    }
    g_theme_runtime_preset = k_theme_cycle_order[0];
    return true;
}

bool mapforge_shared_font_resolve_ui_regular(char *out_path, size_t out_path_size, int *out_point_size) {
    const char *preset_name;
    CoreFontPreset preset = {0};
    CoreFontRoleSpec role = {0};
    int tier_size = 0;
    CoreResult r;

    if (!out_path || out_path_size == 0 || !out_point_size || !is_shared_toggle_enabled("MAPFORGE_USE_SHARED_FONT")) {
        return false;
    }

    preset_name = getenv("MAPFORGE_FONT_PRESET");
    if (!preset_name || !preset_name[0]) {
        preset_name = "daw_default";
    }

    r = core_font_get_preset_by_name(preset_name, &preset);
    if (r.code != CORE_OK) {
        r = core_font_get_preset(CORE_FONT_PRESET_DAW_DEFAULT, &preset);
        if (r.code != CORE_OK) {
            return false;
        }
    }

    r = core_font_resolve_role(&preset, CORE_FONT_ROLE_UI_REGULAR, &role);
    if (r.code != CORE_OK) {
        return false;
    }

    if (!resolve_existing_font_path(&role, out_path, out_path_size) &&
        !copy_existing_path(out_path, out_path_size, "assets/fonts/Montserrat-Regular.ttf")) {
        return false;
    }

    r = core_font_point_size_for_tier(&role, CORE_FONT_TEXT_SIZE_PARAGRAPH, &tier_size);
    if (r.code == CORE_OK && tier_size > 0) {
        *out_point_size = tier_size;
    } else {
        *out_point_size = role.point_size > 0 ? role.point_size : 10;
    }
    return true;
}
