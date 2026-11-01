#ifndef MAPFORGE_TEXT_DRAW_FALLBACK_H
#define MAPFORGE_TEXT_DRAW_FALLBACK_H
#include "ui/text_draw.h"
/* Return zero for an ordinary font run, positive for native success, negative for refusal. */
int map_forge_fallback_measure(Renderer *renderer,TTF_Font *font,const char *text,int *w,int *h);
int map_forge_fallback_draw(Renderer *renderer,TTF_Font *font,const char *text,SDL_Color ink,SDL_Rect *dst);
/* Retire only matching optional native run textures before ownership ends. */
void map_forge_fallback_reset(Renderer *renderer,TTF_Font *font);
#endif
