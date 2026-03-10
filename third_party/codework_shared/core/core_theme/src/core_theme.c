/*
 * core_theme.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_theme.h"

#include <stdlib.h>
#include <string.h>

static const CoreThemePreset k_theme_presets[CORE_THEME_PRESET_COUNT] = {
    {
        CORE_THEME_PRESET_DAW_DEFAULT,
        "studio_blue",
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

static CoreThemePresetId g_selected_preset_id = CORE_THEME_PRESET_DAW_DEFAULT;

const char *core_theme_preset_name(CoreThemePresetId id) {
    if ((int)id < 0 || id >= CORE_THEME_PRESET_COUNT) {
        return "";
    }
    return k_theme_presets[id].name;
}

CoreResult core_theme_preset_id_from_name(const char *name, CoreThemePresetId *out_id) {
    int i;
    if (!name || !name[0] || !out_id) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }
    for (i = 0; i < CORE_THEME_PRESET_COUNT; ++i) {
        if (strcmp(name, k_theme_presets[i].name) == 0) {
            *out_id = k_theme_presets[i].id;
            return core_result_ok();
        }
    }
    if (strcmp(name, "daw_default") == 0) {
        *out_id = CORE_THEME_PRESET_DAW_DEFAULT;
        return core_result_ok();
    }
    if (strcmp(name, "map_forge_default") == 0) {
        *out_id = CORE_THEME_PRESET_MAP_FORGE_DEFAULT;
        return core_result_ok();
    }
    if (strcmp(name, "ide_gray") == 0) {
        *out_id = CORE_THEME_PRESET_IDE_GRAY;
        return core_result_ok();
    }
    if (strcmp(name, "dark_default") == 0) {
        *out_id = CORE_THEME_PRESET_DARK_DEFAULT;
        return core_result_ok();
    }
    if (strcmp(name, "light_default") == 0) {
        *out_id = CORE_THEME_PRESET_LIGHT_DEFAULT;
        return core_result_ok();
    }
    {
        CoreResult r = {CORE_ERR_NOT_FOUND, "preset not found"};
        return r;
    }
}

CoreResult core_theme_get_preset(CoreThemePresetId id, CoreThemePreset *out_preset) {
    if (!out_preset) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "out_preset is null"};
        return r;
    }
    if ((int)id < 0 || id >= CORE_THEME_PRESET_COUNT) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid preset id"};
        return r;
    }

    *out_preset = k_theme_presets[id];
    return core_result_ok();
}

CoreResult core_theme_get_preset_by_name(const char *name, CoreThemePreset *out_preset) {
    CoreThemePresetId id;
    CoreResult r = core_theme_preset_id_from_name(name, &id);
    if (r.code != CORE_OK) {
        return r;
    }
    return core_theme_get_preset(id, out_preset);
}

CoreResult core_theme_get_color(const CoreThemePreset *preset,
                                CoreThemeColorToken token,
                                CoreThemeColor *out_color) {
    if (!preset || !out_color) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }
    if ((int)token < 0 || token >= CORE_THEME_COLOR_COUNT) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid color token"};
        return r;
    }
    *out_color = preset->colors[token];
    return core_result_ok();
}

CoreResult core_theme_select_preset(CoreThemePresetId id) {
    if ((int)id < 0 || id >= CORE_THEME_PRESET_COUNT) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid preset id"};
        return r;
    }
    g_selected_preset_id = id;
    return core_result_ok();
}

CoreResult core_theme_select_preset_by_name(const char *name) {
    CoreThemePresetId id;
    CoreResult r = core_theme_preset_id_from_name(name, &id);
    if (r.code != CORE_OK) {
        return r;
    }
    return core_theme_select_preset(id);
}

CoreThemePresetId core_theme_selected_preset_id(void) {
    return g_selected_preset_id;
}

CoreResult core_theme_selected_preset(CoreThemePreset *out_preset) {
    return core_theme_get_preset(g_selected_preset_id, out_preset);
}

CoreResult core_theme_apply_env_override(const char *env_var_name,
                                         CoreThemePresetId *out_selected_id) {
    const char *var_name = env_var_name;
    const char *value;
    CoreResult r;

    if (!var_name || !var_name[0]) {
        var_name = "CORE_THEME_PRESET";
    }

    value = getenv(var_name);
    if (!value || !value[0]) {
        CoreResult miss = {CORE_ERR_NOT_FOUND, "override not set"};
        return miss;
    }

    r = core_theme_select_preset_by_name(value);
    if (r.code != CORE_OK) {
        return r;
    }

    if (out_selected_id) {
        *out_selected_id = g_selected_preset_id;
    }
    return core_result_ok();
}
