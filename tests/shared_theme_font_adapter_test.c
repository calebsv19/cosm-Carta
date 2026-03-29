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
    int base_point_size = 0;
    int zoomed_point_size = 0;
    int reset_point_size = 0;
    size_t i = 0;
    const char *theme_presets[] = {
        "studio_blue",
        "harbor_blue",
        "midnight_contrast",
        "soft_light",
        "standard_grey",
        "greyscale"
    };

    unsetenv("MAPFORGE_USE_SHARED_THEME_FONT");
    unsetenv("MAPFORGE_USE_SHARED_THEME");
    unsetenv("MAPFORGE_USE_SHARED_FONT");
    unsetenv("MAPFORGE_THEME_PRESET");
    unsetenv("MAPFORGE_FONT_PRESET");

    if (!mapforge_shared_theme_resolve_palette(&palette)) {
        return fail("theme should be enabled by default");
    }
    if (!mapforge_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should be enabled by default");
    }
    base_point_size = point_size;

    setenv("MAPFORGE_USE_SHARED_THEME_FONT", "1", 1);
    setenv("MAPFORGE_THEME_PRESET", "standard_grey", 1);
    if (!mapforge_shared_theme_resolve_palette(&palette)) {
        return fail("theme should resolve when shared toggle is enabled");
    }

    setenv("MAPFORGE_USE_SHARED_FONT", "0", 1);
    if (mapforge_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should be disabled via per-domain toggle");
    }

    setenv("MAPFORGE_USE_SHARED_FONT", "1", 1);
    setenv("MAPFORGE_FONT_PRESET", "studio_blue", 1);
    if (!mapforge_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should resolve when enabled");
    }
    if (path[0] == '\0' || point_size <= 0) {
        return fail("font resolution returned invalid path or point size");
    }
    if (!mapforge_shared_font_set_zoom_step(2)) {
        return fail("zoom step should change from default");
    }
    if (mapforge_shared_font_zoom_step() != 2) {
        return fail("zoom step should report current value");
    }
    if (!mapforge_shared_font_resolve_ui_regular(path, sizeof(path), &zoomed_point_size)) {
        return fail("font should resolve after zoom step update");
    }
    if (zoomed_point_size <= base_point_size) {
        return fail("positive zoom step should increase resolved point size");
    }
    if (!mapforge_shared_font_reset_zoom_step()) {
        return fail("zoom step reset should report a change");
    }
    if (mapforge_shared_font_zoom_step() != 0) {
        return fail("zoom step should reset to zero");
    }
    if (!mapforge_shared_font_resolve_ui_regular(path, sizeof(path), &reset_point_size)) {
        return fail("font should resolve after zoom reset");
    }
    if (reset_point_size != base_point_size) {
        return fail("zoom reset should restore base point size");
    }
    (void)mapforge_shared_font_set_zoom_step(99);
    if (mapforge_shared_font_zoom_step() != 5) {
        return fail("zoom step should clamp to max");
    }
    (void)mapforge_shared_font_set_zoom_step(-99);
    if (mapforge_shared_font_zoom_step() != -4) {
        return fail("zoom step should clamp to min");
    }
    (void)mapforge_shared_font_reset_zoom_step();

    for (i = 0; i < sizeof(theme_presets) / sizeof(theme_presets[0]); ++i) {
        setenv("MAPFORGE_THEME_PRESET", theme_presets[i], 1);
        if (!mapforge_shared_theme_resolve_palette(&palette)) {
            return fail("theme preset matrix should resolve");
        }
    }

    puts("shared_theme_font_adapter_test: success");
    return 0;
}
