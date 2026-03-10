#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "kit_render.h"
#include "vk_renderer.h"

static SDL_Surface *build_checker_surface(void) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_RGBA32);
    uint32_t *pixels;
    int x;
    int y;

    if (!surface) {
        return 0;
    }

    pixels = (uint32_t *)surface->pixels;
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            const int tile = ((x / 8) + (y / 8)) & 1;
            const uint8_t r = tile ? 235 : 70;
            const uint8_t g = tile ? 120 : 170;
            const uint8_t b = tile ? 40 : 210;
            const uint8_t a = 255;
            pixels[(y * 64) + x] = ((uint32_t)a << 24) |
                                   ((uint32_t)b << 16) |
                                   ((uint32_t)g << 8) |
                                   (uint32_t)r;
        }
    }

    return surface;
}

static int record_validation_scene(KitRenderContext *ctx,
                                   uint32_t width,
                                   uint32_t height,
                                   uint64_t texture_id) {
    KitRenderCommand commands[32];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    KitRenderVec2 polyline[4];
    CoreResult result;

    command_buffer.commands = commands;
    command_buffer.capacity = 32;
    command_buffer.count = 0;

    result = kit_render_begin_frame(ctx, width, height, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_begin_frame failed: %d (%s)\n", (int)result.code, result.message);
        return 1;
    }

    result = kit_render_push_clear(&frame, (KitRenderColor){18, 22, 30, 255});
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_set_clip(&frame, (KitRenderRect){30.0f, 30.0f, 720.0f, 460.0f});
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_rect(
        &frame,
        &(KitRenderRectCommand){
            {40.0f, 40.0f, 240.0f, 140.0f},
            0.0f,
            {45, 110, 205, 255},
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_rect(
        &frame,
        &(KitRenderRectCommand){
            {110.0f, 90.0f, 220.0f, 120.0f},
            16.0f,
            {220, 90, 50, 255},
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_line(
        &frame,
        &(KitRenderLineCommand){
            {50.0f, 260.0f},
            {360.0f, 420.0f},
            2.0f,
            {250, 250, 250, 255},
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    polyline[0] = (KitRenderVec2){420.0f, 120.0f};
    polyline[1] = (KitRenderVec2){510.0f, 70.0f};
    polyline[2] = (KitRenderVec2){620.0f, 155.0f};
    polyline[3] = (KitRenderVec2){720.0f, 90.0f};
    result = kit_render_push_polyline(
        &frame,
        &(KitRenderPolylineCommand){
            polyline,
            4,
            2.0f,
            {90, 235, 170, 255},
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_textured_quad(
        &frame,
        &(KitRenderTexturedQuadCommand){
            {470.0f, 220.0f, 180.0f, 180.0f},
            texture_id,
            {0.0f, 0.0f},
            {1.0f, 1.0f},
            {255, 255, 255, 255},
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_text(
        &frame,
        &(KitRenderTextCommand){
            {470.0f, 430.0f},
            "KIT RENDER",
            CORE_FONT_ROLE_UI_BOLD,
            CORE_FONT_TEXT_SIZE_TITLE,
            CORE_THEME_COLOR_TEXT_PRIMARY,
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_clear_clip(&frame);
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_rect(
        &frame,
        &(KitRenderRectCommand){
            {760.0f, 30.0f, 180.0f, 500.0f},
            0.0f,
            {52, 58, 70, 255},
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_text(
        &frame,
        &(KitRenderTextCommand){
            {790.0f, 90.0f},
            "PANEL",
            CORE_FONT_ROLE_UI_MEDIUM,
            CORE_FONT_TEXT_SIZE_PARAGRAPH,
            CORE_THEME_COLOR_TEXT_PRIMARY,
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_render_end_frame(ctx, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_end_frame failed: %d (%s)\n", (int)result.code, result.message);
        return 1;
    }

    return 0;
}

int main(void) {
    SDL_Window *window = 0;
    SDL_Surface *surface = 0;
    bool running = true;
    SDL_Event event;
    VkRenderer renderer;
    VkRendererConfig config;
    VkRendererTexture texture;
    KitRenderContext kit_ctx;
    CoreResult result;
    int exit_code = 1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("kit_render Vulkan Validation",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              960,
                              540,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    vk_renderer_config_set_defaults(&config);
    config.enable_validation = VK_FALSE;

    {
        VkResult init_result = vk_renderer_init(&renderer, window, &config);
        if (init_result != VK_SUCCESS) {
            fprintf(stderr, "vk_renderer_init failed: %d\n", (int)init_result);
            fprintf(stderr, "If this persists, compare against shared/vk_renderer/examples/sdl_vulkan_example.c on the same machine.\n");
            goto cleanup;
        }
    }

    surface = build_checker_surface();
    if (!surface) {
        fprintf(stderr, "Failed to create validation texture surface\n");
        goto cleanup_renderer;
    }
    if (vk_renderer_upload_sdl_surface(&renderer, surface, &texture) != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_upload_sdl_surface failed\n");
        goto cleanup_renderer;
    }

    result = kit_render_context_init(&kit_ctx,
                                     KIT_RENDER_BACKEND_VULKAN,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_context_init failed: %d\n", (int)result.code);
        goto cleanup_texture;
    }

    result = kit_render_attach_external_backend(&kit_ctx, &renderer);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_attach_external_backend failed: %d\n", (int)result.code);
        goto cleanup_kit;
    }

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        if (record_validation_scene(&kit_ctx, 960u, 540u, (uint64_t)(uintptr_t)&texture) != 0) {
            goto cleanup_kit;
        }
    }

    exit_code = 0;

cleanup_kit:
    kit_render_context_shutdown(&kit_ctx);
cleanup_texture:
    vk_renderer_queue_texture_destroy(&renderer, &texture);
cleanup_renderer:
    vk_renderer_shutdown(&renderer);
cleanup:
    if (surface) {
        SDL_FreeSurface(surface);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    return exit_code;
}
