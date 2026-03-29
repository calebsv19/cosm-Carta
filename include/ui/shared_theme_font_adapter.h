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
    SDL_Color overlay_fill;
    SDL_Color overlay_outline;
    SDL_Color overlay_accent;
    SDL_Color route_panel_fill;
    SDL_Color route_panel_outline;
    SDL_Color route_progress_fill;
    SDL_Color playback_marker_fill;
} MapForgeThemePalette;

bool mapforge_shared_theme_resolve_palette(MapForgeThemePalette *out_palette);
bool mapforge_shared_font_resolve_ui_regular(char *out_path, size_t out_path_size, int *out_point_size);
bool mapforge_shared_theme_cycle_next(void);
bool mapforge_shared_theme_cycle_prev(void);
bool mapforge_shared_theme_set_preset(const char *preset_name);
bool mapforge_shared_theme_current_preset(char *out_name, size_t out_name_size);
bool mapforge_shared_theme_load_persisted(void);
bool mapforge_shared_theme_save_persisted(void);
int mapforge_shared_font_zoom_step(void);
bool mapforge_shared_font_set_zoom_step(int step);
bool mapforge_shared_font_step_by(int delta);
bool mapforge_shared_font_reset_zoom_step(void);

#endif
