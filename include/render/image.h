#ifndef MAPFORGE_RENDER_IMAGE_H
#define MAPFORGE_RENDER_IMAGE_H
#include "render/renderer.h"
// Owns one backend texture whose owner must remain alive through disposal.
typedef struct RendererImage {
    SDL_Texture *sdl;
    void *gpu, *owner;
    uintptr_t device; size_t gpu_bytes;
} RendererImage;
// Uploads a bounded RGBA raster on the render owner thread.
bool renderer_image_upload(Renderer *renderer, RendererImage *image, const unsigned char *rgba, int width,
                           int height);
// Checks that a resident image belongs to the current live backend context.
bool renderer_image_ready(Renderer *renderer,RendererImage *image);
// Records a filtered quadrilateral using viewport-local TL/TR/BR/BL corners.
bool renderer_image_draw(Renderer *renderer, RendererImage *image, const SDL_FPoint corners[4]);
// Retires a live texture after device completion and forgets dead context handles.
void renderer_image_release(Renderer *renderer, RendererImage *image);
#endif
