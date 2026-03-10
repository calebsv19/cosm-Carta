#include "core_font.h"

#include <stdio.h>
#include <string.h>

static int assert_ok(CoreResult r) {
    return r.code == CORE_OK;
}

static int fake_path_exists(const char *path, void *user) {
    (void)user;
    return strstr(path, "fallback-ok") != NULL;
}

int main(void) {
    CoreFontPreset preset;
    CoreFontPreset daw;
    CoreFontRoleSpec role;
    CoreResult r;
    CoreFontManifest manifest;
    CoreFontManifestEntry entry;
    const char *selected_path = NULL;
    int sized = 0;
    CoreFontTextSizeTier tier = CORE_FONT_TEXT_SIZE_BASIC;

    r = core_font_get_preset(CORE_FONT_PRESET_DAW_DEFAULT, &preset);
    if (!assert_ok(r)) return 1;
    if (strcmp(preset.name, "daw_default") != 0) return 1;

    r = core_font_resolve_role(&preset, CORE_FONT_ROLE_UI_REGULAR, &role);
    if (!assert_ok(r)) return 1;
    if (role.point_size != 9) return 1;
    if (strcmp(role.primary_path, "include/fonts/Montserrat/Montserrat-Regular.ttf") != 0) return 1;

    r = core_font_get_preset_by_name("ide", &preset);
    if (!assert_ok(r)) return 1;
    if (preset.id != CORE_FONT_PRESET_IDE) return 1;

    r = core_font_resolve_role(&preset, CORE_FONT_ROLE_UI_MONO, &role);
    if (!assert_ok(r)) return 1;
    if (role.point_size != 11) return 1;
    if (strcmp(role.fallback_path, "shared/assets/fonts/Lato-Regular.ttf") != 0) return 1;

    if (strcmp(core_font_role_name(CORE_FONT_ROLE_UI_BOLD), "ui_bold") != 0) return 1;
    if (strcmp(core_font_text_size_tier_name(CORE_FONT_TEXT_SIZE_HEADER), "header") != 0) return 1;

    r = core_font_text_size_tier_from_name("paragraph", &tier);
    if (!assert_ok(r)) return 1;
    if (tier != CORE_FONT_TEXT_SIZE_PARAGRAPH) return 1;

    r = core_font_get_preset_by_name("daw_default", &daw);
    if (!assert_ok(r)) return 1;
    if (daw.id != CORE_FONT_PRESET_DAW_DEFAULT) return 1;

    r = core_font_get_preset_by_name("missing", &preset);
    if (r.code == CORE_OK) return 1;

    r = core_font_manifest_parse_file("tests/fixtures/manifest_sample_v1.txt", &manifest);
    if (!assert_ok(r)) return 1;
    if (strcmp(manifest.version, "core_font_manifest_v1") != 0) return 1;
    if (manifest.entry_count != 3) return 1;

    r = core_font_manifest_find_role(&manifest, "ide", CORE_FONT_ROLE_UI_MONO, &entry);
    if (!assert_ok(r)) return 1;
    if (strcmp(entry.fallback_path, "include/fonts/FiraCode/FiraCode-Regular.ttf") != 0) return 1;
    if (strcmp(entry.pack, "extended") != 0) return 1;

    {
        CoreFontRoleSpec choose_role = {
            CORE_FONT_ROLE_UI_MONO,
            "Mono",
            "Regular",
            CORE_FONT_WEIGHT_NORMAL,
            12,
            "missing-primary.ttf",
            "fallback-ok.ttf"
        };

        r = core_font_choose_path(&choose_role, fake_path_exists, NULL, &selected_path);
        if (!assert_ok(r)) return 1;
        if (strcmp(selected_path, "fallback-ok.ttf") != 0) return 1;
    }

    r = core_font_point_size_for_tier(&role, CORE_FONT_TEXT_SIZE_HEADER, &sized);
    if (!assert_ok(r)) return 1;
    if (sized <= role.point_size) return 1;

    r = core_font_point_size_for_tier(&role, CORE_FONT_TEXT_SIZE_CAPTION, &sized);
    if (!assert_ok(r)) return 1;
    if (sized >= role.point_size) return 1;

    puts("core_font tests passed");
    return 0;
}
