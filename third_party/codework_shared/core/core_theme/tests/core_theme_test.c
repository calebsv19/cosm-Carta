#define _POSIX_C_SOURCE 200809L

#include "core_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ExpectedThemePreset {
    CoreThemePresetId id;
    const char *name;
    const char *legacy_alias;
    CoreThemeColor colors[CORE_THEME_COLOR_COUNT];
    CoreThemeScale scale;
} ExpectedThemePreset;

static const ExpectedThemePreset k_expected_presets[CORE_THEME_PRESET_COUNT] = {
    {
        CORE_THEME_PRESET_DAW_DEFAULT,
        "studio_blue",
        "daw_default",
        {
            {24, 24, 32, 255},
            {26, 26, 34, 255},
            {32, 32, 40, 255},
            {220, 220, 230, 255},
            {150, 150, 160, 255},
            {120, 160, 220, 255},
            {130, 170, 100, 255},
            {255, 190, 140, 255},
            {180, 60, 60, 220}
        },
        {4.0f, 6.0f, 10.0f, 14.0f, 3.0f, 6.0f, 0.9f, 1.0f, 1.2f}
    },
    {
        CORE_THEME_PRESET_IDE_GRAY,
        "standard_grey",
        "ide_gray",
        {
            {20, 20, 20, 255},
            {30, 30, 30, 255},
            {40, 40, 40, 255},
            {255, 255, 255, 255},
            {160, 160, 160, 255},
            {235, 235, 235, 255},
            {120, 185, 135, 255},
            {212, 178, 98, 255},
            {196, 96, 96, 255}
        },
        {4.0f, 6.0f, 10.0f, 14.0f, 2.0f, 5.0f, 0.9f, 1.0f, 1.2f}
    },
    {
        CORE_THEME_PRESET_DARK_DEFAULT,
        "midnight_contrast",
        "dark_default",
        {
            {8, 10, 12, 255},
            {14, 16, 19, 255},
            {24, 27, 31, 255},
            {242, 245, 248, 255},
            {185, 190, 198, 255},
            {236, 240, 246, 255},
            {170, 196, 182, 255},
            {214, 196, 146, 255},
            {210, 148, 148, 255}
        },
        {4.0f, 6.0f, 10.0f, 14.0f, 3.0f, 6.0f, 0.9f, 1.0f, 1.2f}
    },
    {
        CORE_THEME_PRESET_LIGHT_DEFAULT,
        "soft_light",
        "light_default",
        {
            {236, 239, 243, 255},
            {226, 231, 237, 255},
            {212, 219, 228, 255},
            {24, 28, 35, 255},
            {82, 92, 108, 255},
            {39, 101, 214, 255},
            {44, 150, 90, 255},
            {204, 139, 36, 255},
            {186, 63, 63, 255}
        },
        {4.0f, 6.0f, 10.0f, 14.0f, 3.0f, 6.0f, 0.9f, 1.0f, 1.2f}
    },
    {
        CORE_THEME_PRESET_MAP_FORGE_DEFAULT,
        "harbor_blue",
        "map_forge_default",
        {
            {20, 20, 28, 255},
            {30, 35, 46, 255},
            {80, 90, 110, 255},
            {225, 230, 240, 255},
            {205, 212, 225, 255},
            {60, 140, 220, 255},
            {60, 200, 130, 255},
            {110, 180, 255, 255},
            {192, 92, 92, 255}
        },
        {4.0f, 6.0f, 10.0f, 14.0f, 3.0f, 6.0f, 0.9f, 1.0f, 1.2f}
    },
    {
        CORE_THEME_PRESET_GREYSCALE,
        "greyscale",
        NULL,
        {
            {20, 20, 20, 255},
            {30, 30, 30, 255},
            {44, 44, 44, 255},
            {238, 238, 238, 255},
            {164, 164, 164, 255},
            {210, 210, 210, 255},
            {176, 176, 176, 255},
            {196, 196, 196, 255},
            {132, 132, 132, 255}
        },
        {4.0f, 6.0f, 10.0f, 14.0f, 2.0f, 5.0f, 0.9f, 1.0f, 1.2f}
    }
};

static int assert_ok(CoreResult r) {
    return r.code == CORE_OK;
}

static int colors_equal(CoreThemeColor a, CoreThemeColor b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static int scales_equal(CoreThemeScale a, CoreThemeScale b) {
    return a.spacing_xs == b.spacing_xs &&
           a.spacing_sm == b.spacing_sm &&
           a.spacing_md == b.spacing_md &&
           a.spacing_lg == b.spacing_lg &&
           a.radius_sm == b.radius_sm &&
           a.radius_md == b.radius_md &&
           a.text_small == b.text_small &&
           a.text_normal == b.text_normal &&
           a.text_large == b.text_large;
}

static void restore_env(const char *name, const char *old_value, int had_old_value) {
    if (had_old_value) {
        (void)setenv(name, old_value, 1);
    } else {
        (void)unsetenv(name);
    }
}

static int test_preset_color_and_scale_matrix(void) {
    int preset_index;

    for (preset_index = 0; preset_index < CORE_THEME_PRESET_COUNT; ++preset_index) {
        const ExpectedThemePreset *expected = &k_expected_presets[preset_index];
        CoreThemePreset preset;
        CoreThemePreset by_name;
        CoreThemePresetId id = CORE_THEME_PRESET_DAW_DEFAULT;
        int token_index;

        if (strcmp(core_theme_preset_name(expected->id), expected->name) != 0) return 1;

        if (!assert_ok(core_theme_get_preset(expected->id, &preset))) return 1;
        if (preset.id != expected->id) return 1;
        if (strcmp(preset.name, expected->name) != 0) return 1;
        if (!scales_equal(preset.scale, expected->scale)) return 1;

        if (!assert_ok(core_theme_get_preset_by_name(expected->name, &by_name))) return 1;
        if (by_name.id != expected->id) return 1;

        if (!assert_ok(core_theme_preset_id_from_name(expected->name, &id))) return 1;
        if (id != expected->id) return 1;

        if (expected->legacy_alias) {
            if (!assert_ok(core_theme_preset_id_from_name(expected->legacy_alias, &id))) return 1;
            if (id != expected->id) return 1;
        }

        for (token_index = 0; token_index < CORE_THEME_COLOR_COUNT; ++token_index) {
            CoreThemeColor actual;
            if (!assert_ok(core_theme_get_color(&preset, (CoreThemeColorToken)token_index, &actual))) return 1;
            if (!colors_equal(actual, expected->colors[token_index])) return 1;
        }
    }

    return 0;
}

static int test_selected_state_and_env_override(void) {
    const char *default_var = "CORE_THEME_PRESET";
    const char *custom_var = "CORE_THEME_TEST_OVERRIDE";
    const char *old_default = getenv(default_var);
    const char *old_custom = getenv(custom_var);
    char old_default_buf[64];
    char old_custom_buf[64];
    int had_old_default = old_default != NULL;
    int had_old_custom = old_custom != NULL;
    CoreThemePresetId id = CORE_THEME_PRESET_DAW_DEFAULT;
    CoreThemePreset preset;

    if (had_old_default) {
        strncpy(old_default_buf, old_default, sizeof(old_default_buf) - 1u);
        old_default_buf[sizeof(old_default_buf) - 1u] = '\0';
    }
    if (had_old_custom) {
        strncpy(old_custom_buf, old_custom, sizeof(old_custom_buf) - 1u);
        old_custom_buf[sizeof(old_custom_buf) - 1u] = '\0';
    }

    if (!assert_ok(core_theme_select_preset(CORE_THEME_PRESET_DAW_DEFAULT))) goto fail;
    if (core_theme_selected_preset_id() != CORE_THEME_PRESET_DAW_DEFAULT) goto fail;

    if (!assert_ok(core_theme_select_preset_by_name("standard_grey"))) goto fail;
    if (core_theme_selected_preset_id() != CORE_THEME_PRESET_IDE_GRAY) goto fail;
    if (!assert_ok(core_theme_selected_preset(&preset))) goto fail;
    if (preset.id != CORE_THEME_PRESET_IDE_GRAY) goto fail;

    if (setenv(custom_var, "harbor_blue", 1) != 0) goto fail;
    if (!assert_ok(core_theme_apply_env_override(custom_var, &id))) goto fail;
    if (id != CORE_THEME_PRESET_MAP_FORGE_DEFAULT) goto fail;
    if (core_theme_selected_preset_id() != CORE_THEME_PRESET_MAP_FORGE_DEFAULT) goto fail;

    if (setenv(default_var, "greyscale", 1) != 0) goto fail;
    if (!assert_ok(core_theme_apply_env_override(NULL, &id))) goto fail;
    if (id != CORE_THEME_PRESET_GREYSCALE) goto fail;
    if (core_theme_selected_preset_id() != CORE_THEME_PRESET_GREYSCALE) goto fail;

    if (setenv(custom_var, "missing_preset", 1) != 0) goto fail;
    if (core_theme_apply_env_override(custom_var, &id).code == CORE_OK) goto fail;

    if (unsetenv(custom_var) != 0) goto fail;
    if (core_theme_apply_env_override(custom_var, &id).code != CORE_ERR_NOT_FOUND) goto fail;

    restore_env(default_var, had_old_default ? old_default_buf : NULL, had_old_default);
    restore_env(custom_var, had_old_custom ? old_custom_buf : NULL, had_old_custom);
    return 0;

fail:
    restore_env(default_var, had_old_default ? old_default_buf : NULL, had_old_default);
    restore_env(custom_var, had_old_custom ? old_custom_buf : NULL, had_old_custom);
    return 1;
}

static int test_invalid_inputs(void) {
    CoreThemePreset preset;
    CoreThemeColor color;
    CoreThemePresetId id = CORE_THEME_PRESET_DAW_DEFAULT;
    CoreResult r;

    r = core_theme_get_preset((CoreThemePresetId)-1, &preset);
    if (r.code == CORE_OK) return 1;
    r = core_theme_get_preset(CORE_THEME_PRESET_COUNT, &preset);
    if (r.code == CORE_OK) return 1;

    r = core_theme_get_preset(CORE_THEME_PRESET_DAW_DEFAULT, NULL);
    if (r.code == CORE_OK) return 1;

    r = core_theme_get_preset_by_name(NULL, &preset);
    if (r.code == CORE_OK) return 1;
    r = core_theme_get_preset_by_name("", &preset);
    if (r.code == CORE_OK) return 1;
    r = core_theme_get_preset_by_name("missing_preset", &preset);
    if (r.code == CORE_OK) return 1;

    r = core_theme_preset_id_from_name(NULL, &id);
    if (r.code == CORE_OK) return 1;
    r = core_theme_preset_id_from_name("", &id);
    if (r.code == CORE_OK) return 1;
    r = core_theme_preset_id_from_name("missing_preset", &id);
    if (r.code == CORE_OK) return 1;

    if (!assert_ok(core_theme_get_preset(CORE_THEME_PRESET_DAW_DEFAULT, &preset))) return 1;
    r = core_theme_get_color(NULL, CORE_THEME_COLOR_SURFACE_0, &color);
    if (r.code == CORE_OK) return 1;
    r = core_theme_get_color(&preset, CORE_THEME_COLOR_SURFACE_0, NULL);
    if (r.code == CORE_OK) return 1;
    r = core_theme_get_color(&preset, (CoreThemeColorToken)-1, &color);
    if (r.code == CORE_OK) return 1;
    r = core_theme_get_color(&preset, CORE_THEME_COLOR_COUNT, &color);
    if (r.code == CORE_OK) return 1;

    r = core_theme_select_preset((CoreThemePresetId)-1);
    if (r.code == CORE_OK) return 1;
    r = core_theme_select_preset(CORE_THEME_PRESET_COUNT);
    if (r.code == CORE_OK) return 1;
    r = core_theme_select_preset_by_name(NULL);
    if (r.code == CORE_OK) return 1;
    r = core_theme_select_preset_by_name("");
    if (r.code == CORE_OK) return 1;

    r = core_theme_selected_preset(NULL);
    if (r.code == CORE_OK) return 1;

    return 0;
}

int main(void) {
    if (test_preset_color_and_scale_matrix() != 0) return 1;
    if (test_selected_state_and_env_override() != 0) return 1;
    if (test_invalid_inputs() != 0) return 1;
    puts("core_theme tests passed");
    return 0;
}
