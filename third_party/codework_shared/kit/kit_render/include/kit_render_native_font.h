#ifndef KIT_RENDER_NATIVE_FONT_H
#define KIT_RENDER_NATIVE_FONT_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
/* Optional platform adapter. No font files are copied or installed. macOS uses
 * CoreText fallback runs; unsupported platforms return 0/NULL for host fallback.
 * Measurement and rasterization use the same shaped line and logical point size.
 * Returned surfaces are RGBA straight alpha, owned by the caller. */
int kit_render_native_font_needs_fallback(TTF_Font* font, const char* utf8);
int kit_render_native_font_measure(const char* font_path, float points, const char* utf8, int* width, int* height);
SDL_Surface* kit_render_native_font_rasterize(const char* font_path, float points, const char* utf8, SDL_Color color, float scale);
#endif
