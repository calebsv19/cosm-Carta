#ifndef MAPFORGE_RENDER_RENDERER_H
#define MAPFORGE_RENDER_RENDERER_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

// Identifies the active rendering backend.
typedef enum RendererBackend {
    RENDERER_BACKEND_SDL = 0,
    RENDERER_BACKEND_VULKAN = 1
} RendererBackend;

// Owns render backend state and target sizing.
typedef struct Renderer {
    RendererBackend backend;
    SDL_Renderer *sdl;
    void *vk;
    uintptr_t vk_cmd;
    SDL_Window *window;
    bool vulkan_available;
    uint32_t vk_geom_budget;
    uint32_t vk_geom_used;
    uint32_t vk_lines_drawn;
    uint32_t vk_line_budget;
    uint32_t vk_line_budget_skips;
    uint32_t vk_rects_drawn;
    uint32_t vk_rects_filled;
    uint32_t vk_geom_calls;
    uint32_t vk_geom_budget_skips;
    uint32_t vk_swapchain_recreates;
    uint64_t vk_begin_failures_total;
    uint32_t vk_begin_fail_streak;
    int vk_last_begin_result;
    int width;
    int height;
} Renderer;

// Sets preferred backend before renderer_init; defaults to SDL.
void renderer_set_backend(Renderer *renderer, RendererBackend backend);

// Returns selected backend for the renderer.
RendererBackend renderer_get_backend(const Renderer *renderer);

// Returns stable backend label for logs/UI.
const char *renderer_backend_name(RendererBackend backend);

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

// Sets the current draw color for subsequent primitive draw calls.
void renderer_set_draw_color(Renderer *renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Draws one line segment in screen space.
void renderer_draw_line(Renderer *renderer, float x0, float y0, float x1, float y1);

// Draws a connected line strip in screen space.
void renderer_draw_lines(Renderer *renderer, const SDL_FPoint *points, int count);

// Draws an unfilled rectangle in screen space.
void renderer_draw_rect(Renderer *renderer, const SDL_FRect *rect);

// Draws a filled rectangle in screen space.
void renderer_fill_rect(Renderer *renderer, const SDL_FRect *rect);

// Draws indexed/non-indexed triangle geometry.
void renderer_draw_geometry(Renderer *renderer,
                            const SDL_Vertex *vertices,
                            int num_vertices,
                            const int *indices,
                            int num_indices);

#endif
