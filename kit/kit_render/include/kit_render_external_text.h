#ifndef KIT_RENDER_EXTERNAL_TEXT_H
#define KIT_RENDER_EXTERNAL_TEXT_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#ifdef __cplusplus
extern "C" {
#endif

void kit_render_external_text_register_font_source(TTF_Font *font,
                                                   const char *path,
                                                   int logical_point_size,
                                                   int loaded_point_size,
                                                   int kerning_enabled);

void kit_render_external_text_unregister_font_source(TTF_Font *font);

void kit_render_external_text_reset_renderer(SDL_Renderer *renderer);

int kit_render_external_text_measure_utf8(SDL_Renderer *renderer,
                                          TTF_Font *font,
                                          const char *text,
                                          int *out_w,
                                          int *out_h);

int kit_render_external_text_draw_utf8(SDL_Renderer *renderer,
                                       TTF_Font *font,
                                       const char *text,
                                       SDL_Color color,
                                       SDL_Rect *io_dst);

int kit_render_external_text_draw_utf8_at(SDL_Renderer *renderer,
                                          TTF_Font *font,
                                          const char *text,
                                          int x,
                                          int y,
                                          SDL_Color color);

int kit_render_external_text_draw_utf8_wrapped(SDL_Renderer *renderer,
                                               TTF_Font *font,
                                               const char *text,
                                               int wrap_width,
                                               SDL_Color color,
                                               SDL_Rect *io_dst);

#ifdef __cplusplus
}
#endif

#endif
