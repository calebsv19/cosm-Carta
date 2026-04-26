#ifndef MAPFORGE_UI_TEXT_DRAW_H
#define MAPFORGE_UI_TEXT_DRAW_H

#include "render/renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void map_forge_text_register_font_source(TTF_Font *font,
                                         const char *path,
                                         int logical_point_size,
                                         int loaded_point_size,
                                         int kerning_enabled);

void map_forge_text_unregister_font_source(TTF_Font *font);
void map_forge_text_reset_renderer(Renderer *renderer);

int map_forge_text_measure_utf8(Renderer *renderer,
                                TTF_Font *font,
                                const char *text,
                                int *out_w,
                                int *out_h);

int map_forge_text_draw_utf8(Renderer *renderer,
                             TTF_Font *font,
                             const char *text,
                             SDL_Color color,
                             SDL_Rect *io_dst);

int map_forge_text_draw_utf8_clipped(Renderer *renderer,
                                     TTF_Font *font,
                                     const char *text,
                                     SDL_Color color,
                                     SDL_Rect *io_dst,
                                     int max_width);

#endif
