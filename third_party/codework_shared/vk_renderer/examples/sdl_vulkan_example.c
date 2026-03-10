#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <stdbool.h>
#include <stdio.h>

#include "vk_renderer.h"
#include "vk_renderer_sdl.h"

static void handle_frame(VkRenderer* renderer, SDL_Window* window) {
    int win_w = 0;
    int win_h = 0;
    int drawable_w = 0;
    int drawable_h = 0;

    SDL_GetWindowSize(window, &win_w, &win_h);
    SDL_Vulkan_GetDrawableSize(window, &drawable_w, &drawable_h);
    vk_renderer_set_logical_size(renderer, (float)win_w, (float)win_h);

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFramebuffer fb = VK_NULL_HANDLE;
    VkExtent2D extent = {0, 0};

    VkResult frame = vk_renderer_begin_frame(renderer, &cmd, &fb, &extent);
    if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
        vk_renderer_recreate_swapchain(renderer, window);
        return;
    }
    if (frame != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_begin_frame failed: %d\n", frame);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    SDL_Rect background = {0, 0, win_w, win_h};
    SDL_RenderFillRect(renderer, &background);

    SDL_SetRenderDrawColor(renderer, 200, 120, 40, 255);
    SDL_Rect rect = {40, 40, 240, 140};
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderDrawLine(renderer, 40, 40, 280, 180);

    VkResult end = vk_renderer_end_frame(renderer, cmd);
    if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
        vk_renderer_recreate_swapchain(renderer, window);
    } else if (end != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_end_frame failed: %d\n", end);
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "SDL Vulkan Example",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        960,
        540,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    VkRenderer renderer;
    VkRendererConfig cfg;
    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = VK_FALSE;

    VkResult init = vk_renderer_init(&renderer, window, &cfg);
    if (init != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed: %d\n", init);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
        handle_frame(&renderer, window);
    }

    vk_renderer_shutdown(&renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
