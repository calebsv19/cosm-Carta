#include "core_theme.h"

#include <stdio.h>
#include <string.h>

static int assert_ok(CoreResult r) {
    return r.code == CORE_OK;
}

int main(void) {
    CoreThemePreset daw;
    CoreThemePreset ide;
    CoreThemePreset map_forge;
    CoreThemeColor accent;
    CoreThemeColor surface0;
    CoreThemeColor text_primary;
    CoreThemeColor text_muted;
    CoreThemePresetId id;
    CoreResult r;

    r = core_theme_get_preset(CORE_THEME_PRESET_DAW_DEFAULT, &daw);
    if (!assert_ok(r)) return 1;
    if (strcmp(daw.name, "studio_blue") != 0) return 1;

    r = core_theme_get_preset_by_name("standard_grey", &ide);
    if (!assert_ok(r)) return 1;
    if (ide.id != CORE_THEME_PRESET_IDE_GRAY) return 1;

    r = core_theme_get_preset_by_name("harbor_blue", &map_forge);
    if (!assert_ok(r)) return 1;
    if (map_forge.id != CORE_THEME_PRESET_MAP_FORGE_DEFAULT) return 1;

    r = core_theme_get_color(&daw, CORE_THEME_COLOR_ACCENT_PRIMARY, &accent);
    if (!assert_ok(r)) return 1;
    if (accent.r != 120 || accent.g != 160 || accent.b != 220 || accent.a != 255) return 1;

    r = core_theme_get_color(&daw, CORE_THEME_COLOR_SURFACE_0, &surface0);
    if (!assert_ok(r)) return 1;
    if (surface0.r != 24 || surface0.g != 24 || surface0.b != 32 || surface0.a != 255) return 1;

    r = core_theme_get_color(&daw, CORE_THEME_COLOR_TEXT_PRIMARY, &text_primary);
    if (!assert_ok(r)) return 1;
    if (text_primary.r != 220 || text_primary.g != 220 || text_primary.b != 230 || text_primary.a != 255) return 1;

    r = core_theme_get_color(&daw, CORE_THEME_COLOR_TEXT_MUTED, &text_muted);
    if (!assert_ok(r)) return 1;
    if (text_muted.r != 150 || text_muted.g != 150 || text_muted.b != 160 || text_muted.a != 255) return 1;

    r = core_theme_get_color(&ide, CORE_THEME_COLOR_SURFACE_0, &surface0);
    if (!assert_ok(r)) return 1;
    if (surface0.r != 20 || surface0.g != 20 || surface0.b != 20 || surface0.a != 255) return 1;

    r = core_theme_get_color(&ide, CORE_THEME_COLOR_TEXT_MUTED, &text_muted);
    if (!assert_ok(r)) return 1;
    if (text_muted.r != 160 || text_muted.g != 160 || text_muted.b != 160 || text_muted.a != 255) return 1;

    r = core_theme_get_color(&map_forge, CORE_THEME_COLOR_SURFACE_2, &accent);
    if (!assert_ok(r)) return 1;
    if (accent.r != 80 || accent.g != 90 || accent.b != 110 || accent.a != 255) return 1;

    r = core_theme_preset_id_from_name("standard_grey", &id);
    if (!assert_ok(r)) return 1;
    if (id != CORE_THEME_PRESET_IDE_GRAY) return 1;

    r = core_theme_select_preset_by_name("standard_grey");
    if (!assert_ok(r)) return 1;
    if (core_theme_selected_preset_id() != CORE_THEME_PRESET_IDE_GRAY) return 1;

    r = core_theme_selected_preset(&ide);
    if (!assert_ok(r)) return 1;
    if (ide.id != CORE_THEME_PRESET_IDE_GRAY) return 1;

    r = core_theme_apply_env_override("CORE_THEME_TEST_MISSING", &id);
    if (r.code != CORE_ERR_NOT_FOUND) return 1;

    r = core_theme_get_preset_by_name("missing_preset", &ide);
    if (r.code == CORE_OK) return 1;

    if (strcmp(core_theme_preset_name(CORE_THEME_PRESET_LIGHT_DEFAULT), "soft_light") != 0) {
        return 1;
    }
    if (strcmp(core_theme_preset_name(CORE_THEME_PRESET_MAP_FORGE_DEFAULT), "harbor_blue") != 0) {
        return 1;
    }
    if (strcmp(core_theme_preset_name(CORE_THEME_PRESET_GREYSCALE), "greyscale") != 0) {
        return 1;
    }

    r = core_theme_preset_id_from_name("daw_default", &id);
    if (!assert_ok(r) || id != CORE_THEME_PRESET_DAW_DEFAULT) return 1;
    r = core_theme_preset_id_from_name("map_forge_default", &id);
    if (!assert_ok(r) || id != CORE_THEME_PRESET_MAP_FORGE_DEFAULT) return 1;
    r = core_theme_preset_id_from_name("ide_gray", &id);
    if (!assert_ok(r) || id != CORE_THEME_PRESET_IDE_GRAY) return 1;
    r = core_theme_preset_id_from_name("dark_default", &id);
    if (!assert_ok(r) || id != CORE_THEME_PRESET_DARK_DEFAULT) return 1;
    r = core_theme_preset_id_from_name("light_default", &id);
    if (!assert_ok(r) || id != CORE_THEME_PRESET_LIGHT_DEFAULT) return 1;

    puts("core_theme tests passed");
    return 0;
}
