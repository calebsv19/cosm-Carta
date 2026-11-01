#ifndef MAPFORGE_UI_FONT_BRIDGE_H
#define MAPFORGE_UI_FONT_BRIDGE_H

#include "render/renderer.h"

#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

typedef struct MapForgeResolvedFont {
    TTF_Font *font;
    const char *path;
    int logical_point_size;
    int kerning_enabled;
} MapForgeResolvedFont;

/* Start retaining the exact authoring entry bank. */
void map_forge_font_bridge_preview_begin(void);
/* Accept the new bank or restore entry handles without font I/O. */
void map_forge_font_bridge_preview_end(bool accept);
bool map_forge_font_bridge_set_active(const char *path, int base_point_size);
void map_forge_font_bridge_shutdown(Renderer *renderer);
void map_forge_font_bridge_reset_renderer(Renderer *renderer);
bool map_forge_font_bridge_acquire(float scale, MapForgeResolvedFont *out_font);

#endif
