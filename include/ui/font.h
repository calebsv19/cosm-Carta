#ifndef MAPFORGE_UI_FONT_H
#define MAPFORGE_UI_FONT_H

#include <SDL2/SDL.h>
#include <stdbool.h>

// Configure the active font and base point size (used as scale 1.0).
bool ui_font_set(const char *path, int base_point_size);
// Releases cached font resources.
void ui_font_shutdown(void);
// Clears cached text textures for the active renderer.
void ui_font_invalidate_cache(SDL_Renderer *renderer);

// Rendering helpers (scale is a float multiplier of the base size).
void ui_draw_text(SDL_Renderer *renderer, int x, int y, const char *text, SDL_Color color, float scale);
void ui_draw_text_clipped(SDL_Renderer *renderer,
                          int x,
                          int y,
                          const char *text,
                          SDL_Color color,
                          float scale,
                          int max_width);
int ui_measure_text_width(const char *text, float scale);
int ui_font_line_height(float scale);

#endif
