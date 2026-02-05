#ifndef MAPFORGE_RENDER_RENDERER_H
#define MAPFORGE_RENDER_RENDERER_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

// Owns render backend state and target sizing.
typedef struct Renderer {
    SDL_Renderer *sdl;
    int width;
    int height;
} Renderer;

// Initializes the renderer backend for the given window.
bool renderer_init(Renderer *renderer, SDL_Window *window, int width, int height);

// Shuts down the renderer backend.
void renderer_shutdown(Renderer *renderer);

// Begins a new frame.
void renderer_begin_frame(Renderer *renderer);

// Clears the frame buffer to a solid color.
void renderer_clear(Renderer *renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Ends the frame and presents it.
void renderer_end_frame(Renderer *renderer);

#endif
