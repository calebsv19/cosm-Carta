#include "ui/shared_theme_font_adapter.h"

#include <stdio.h>
#include <stdlib.h>

static int fail(const char *msg) {
    fprintf(stderr, "shared_theme_font_adapter_test: %s\n", msg);
    return 1;
}

int main(void) {
    MapForgeThemePalette palette = {0};
    char path[256] = {0};
    int point_size = 0;
    size_t i = 0;
    const char *theme_presets[] = {
        "dark_default",
        "ide_gray",
        "daw_default",
        "light_default"
    };

    unsetenv("MAPFORGE_USE_SHARED_THEME_FONT");
    unsetenv("MAPFORGE_USE_SHARED_THEME");
    unsetenv("MAPFORGE_USE_SHARED_FONT");
    unsetenv("MAPFORGE_THEME_PRESET");
    unsetenv("MAPFORGE_FONT_PRESET");

    if (mapforge_shared_theme_resolve_palette(&palette)) {
        return fail("theme should be disabled by default");
    }
    if (mapforge_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should be disabled by default");
    }

    setenv("MAPFORGE_USE_SHARED_THEME_FONT", "1", 1);
    setenv("MAPFORGE_THEME_PRESET", "ide_gray", 1);
    if (!mapforge_shared_theme_resolve_palette(&palette)) {
        return fail("theme should resolve when shared toggle is enabled");
    }

    setenv("MAPFORGE_USE_SHARED_FONT", "0", 1);
    if (mapforge_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should be disabled via per-domain toggle");
    }

    setenv("MAPFORGE_USE_SHARED_FONT", "1", 1);
    setenv("MAPFORGE_FONT_PRESET", "daw_default", 1);
    if (!mapforge_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should resolve when enabled");
    }
    if (path[0] == '\0' || point_size <= 0) {
        return fail("font resolution returned invalid path or point size");
    }

    for (i = 0; i < sizeof(theme_presets) / sizeof(theme_presets[0]); ++i) {
        setenv("MAPFORGE_THEME_PRESET", theme_presets[i], 1);
        if (!mapforge_shared_theme_resolve_palette(&palette)) {
            return fail("theme preset matrix should resolve");
        }
    }

    puts("shared_theme_font_adapter_test: success");
    return 0;
}
