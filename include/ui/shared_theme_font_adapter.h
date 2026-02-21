#ifndef MAPFORGE_UI_SHARED_THEME_FONT_ADAPTER_H
#define MAPFORGE_UI_SHARED_THEME_FONT_ADAPTER_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct MapForgeThemePalette {
    SDL_Color background_clear;
    SDL_Color header_fill;
    SDL_Color header_outline;
    SDL_Color button_fill;
    SDL_Color button_outline;
    SDL_Color button_active_primary;
    SDL_Color button_active_success;
    SDL_Color badge_fill;
    SDL_Color badge_outline;
    SDL_Color text_primary;
    SDL_Color text_muted;
    SDL_Color progress_fill;
    SDL_Color progress_bg;
    SDL_Color chip_idle_fill;
    SDL_Color chip_idle_outline;
    SDL_Color chip_loading_fill;
    SDL_Color chip_loading_outline;
    SDL_Color chip_ready_fill;
    SDL_Color chip_ready_outline;
} MapForgeThemePalette;

bool mapforge_shared_theme_resolve_palette(MapForgeThemePalette *out_palette);
bool mapforge_shared_font_resolve_ui_regular(char *out_path, size_t out_path_size, int *out_point_size);

#endif
