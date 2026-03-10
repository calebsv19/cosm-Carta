#ifndef CORE_THEME_H
#define CORE_THEME_H

#include "core_base.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CoreThemePresetId {
    CORE_THEME_PRESET_DAW_DEFAULT = 0,
    CORE_THEME_PRESET_IDE_GRAY = 1,
    CORE_THEME_PRESET_DARK_DEFAULT = 2,
    CORE_THEME_PRESET_LIGHT_DEFAULT = 3,
    CORE_THEME_PRESET_MAP_FORGE_DEFAULT = 4,
    CORE_THEME_PRESET_GREYSCALE = 5,
    CORE_THEME_PRESET_COUNT
} CoreThemePresetId;

typedef enum CoreThemeColorToken {
    CORE_THEME_COLOR_SURFACE_0 = 0,
    CORE_THEME_COLOR_SURFACE_1 = 1,
    CORE_THEME_COLOR_SURFACE_2 = 2,
    CORE_THEME_COLOR_TEXT_PRIMARY = 3,
    CORE_THEME_COLOR_TEXT_MUTED = 4,
    CORE_THEME_COLOR_ACCENT_PRIMARY = 5,
    CORE_THEME_COLOR_STATUS_OK = 6,
    CORE_THEME_COLOR_STATUS_WARN = 7,
    CORE_THEME_COLOR_STATUS_ERROR = 8,
    CORE_THEME_COLOR_COUNT
} CoreThemeColorToken;

typedef struct CoreThemeColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} CoreThemeColor;

typedef struct CoreThemeScale {
    float spacing_xs;
    float spacing_sm;
    float spacing_md;
    float spacing_lg;
    float radius_sm;
    float radius_md;
    float text_small;
    float text_normal;
    float text_large;
} CoreThemeScale;

typedef struct CoreThemePreset {
    CoreThemePresetId id;
    const char *name;
    CoreThemeColor colors[CORE_THEME_COLOR_COUNT];
    CoreThemeScale scale;
} CoreThemePreset;

const char *core_theme_preset_name(CoreThemePresetId id);
CoreResult core_theme_preset_id_from_name(const char *name, CoreThemePresetId *out_id);
CoreResult core_theme_get_preset(CoreThemePresetId id, CoreThemePreset *out_preset);
CoreResult core_theme_get_preset_by_name(const char *name, CoreThemePreset *out_preset);
CoreResult core_theme_get_color(const CoreThemePreset *preset,
                                CoreThemeColorToken token,
                                CoreThemeColor *out_color);

CoreResult core_theme_select_preset(CoreThemePresetId id);
CoreResult core_theme_select_preset_by_name(const char *name);
CoreThemePresetId core_theme_selected_preset_id(void);
CoreResult core_theme_selected_preset(CoreThemePreset *out_preset);
CoreResult core_theme_apply_env_override(const char *env_var_name,
                                         CoreThemePresetId *out_selected_id);

#ifdef __cplusplus
}
#endif

#endif
