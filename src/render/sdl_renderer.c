#include "render/renderer.h"

bool renderer_init(Renderer *renderer, SDL_Window *window, int width, int height) {
    if (!renderer || !window) {
        return false;
    }

    renderer->sdl = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer->sdl) {
        return false;
    }

    renderer->width = width;
    renderer->height = height;
    return true;
}

void renderer_shutdown(Renderer *renderer) {
    if (!renderer) {
        return;
    }

    if (renderer->sdl) {
        SDL_DestroyRenderer(renderer->sdl);
        renderer->sdl = NULL;
    }
}

void renderer_begin_frame(Renderer *renderer) {
    (void)renderer;
}

void renderer_clear(Renderer *renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!renderer || !renderer->sdl) {
        return;
    }

    SDL_SetRenderDrawColor(renderer->sdl, r, g, b, a);
    SDL_RenderClear(renderer->sdl);
}

void renderer_end_frame(Renderer *renderer) {
    if (!renderer || !renderer->sdl) {
        return;
    }

    SDL_RenderPresent(renderer->sdl);
}
