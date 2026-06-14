#ifndef KIT_UI_SDL_H
#define KIT_UI_SDL_H

#include <SDL2/SDL.h>

#include "kit_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*KitUiSdlMeasureTextFn)(void *user,
                                     const char *text,
                                     int scale,
                                     int *out_width,
                                     int *out_height);
typedef int (*KitUiSdlLineHeightFn)(void *user, int scale);
typedef void (*KitUiSdlDrawTextClippedFn)(void *user,
                                          SDL_Renderer *renderer,
                                          const SDL_Rect *clip_rect,
                                          int x,
                                          int y,
                                          const char *text,
                                          int scale,
                                          KitRenderColor color);

typedef struct KitUiSdlTextApi {
    void *user;
    int scale;
    int clip_padding_x;
    KitUiSdlMeasureTextFn measure_text;
    KitUiSdlLineHeightFn line_height;
    KitUiSdlDrawTextClippedFn draw_text_clipped;
} KitUiSdlTextApi;

SDL_Rect kit_ui_sdl_rect_from_render(KitRenderRect rect);
void kit_ui_sdl_fill_rounded_rect(SDL_Renderer *renderer,
                                  const SDL_Rect *rect,
                                  int radius,
                                  KitRenderColor color);
void kit_ui_sdl_draw_button(SDL_Renderer *renderer,
                            const SDL_Rect *rect,
                            const char *label,
                            const KitUiButtonState *state,
                            const KitUiHudStyle *style,
                            const KitUiSdlTextApi *text_api);
void kit_ui_sdl_draw_readout(SDL_Renderer *renderer,
                             const SDL_Rect *rect,
                             const char *text,
                             const KitUiHudStyle *style,
                             const KitUiSdlTextApi *text_api);

#ifdef __cplusplus
}
#endif

#endif
