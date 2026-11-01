#ifndef MAPFORGE_TEXT_DRAW_SDL_H
#define MAPFORGE_TEXT_DRAW_SDL_H
#include "ui/text_draw.h"
/* Retain native SDL font-source metadata alongside shared registration. */
void map_forge_sdl_font_register(TTF_Font *font, const char *path, int points, int kerning);
/* Release optional density rasters when base ownership ends. */
void map_forge_sdl_font_unregister(TTF_Font *font);
/* Measure and paint the same density-specific native SDL raster. */
int map_forge_sdl_text_measure(Renderer *renderer, TTF_Font *font, const char *text, int *w, int *h);
int map_forge_sdl_text_draw(Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color, SDL_Rect *dst, int limit);
/* Describe a live registered source for the optional native fallback owner. */
int map_forge_native_font_source(TTF_Font *font,const char **path,int *points);

#endif
