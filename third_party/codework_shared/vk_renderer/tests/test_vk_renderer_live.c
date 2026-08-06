#include "vk_renderer.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* message, VkResult result) {
    fprintf(stderr, "vk_renderer live proof failed: %s (%d)\n", message,
            (int)result);
    return 1;
}

static int render_capture(VkRenderer* renderer,
                          const char* path,
                          int width,
                          int height) {
    SDL_FPoint line_points[2] = {
        {(float)width * 0.15f, (float)height * 0.80f},
        {(float)width * 0.85f, (float)height * 0.80f},
    };
    VkRendererLineMesh line_mesh;
    VkResult result = vk_renderer_create_line_mesh(
        renderer, line_points, 2u, 1.0f, 1.0f, 1.0f, 1.0f, &line_mesh);
    if (result != VK_SUCCESS) return fail("tinted line mesh", result);

    result = vk_renderer_request_capture(renderer, path);
    if (result != VK_SUCCESS) {
        vk_renderer_destroy_line_mesh(renderer, &line_mesh);
        return fail("capture request", result);
    }

    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    result = vk_renderer_begin_frame(renderer, &command, &framebuffer, NULL);
    if (result != VK_SUCCESS) {
        vk_renderer_destroy_line_mesh(renderer, &line_mesh);
        return fail("begin frame", result);
    }

    vk_renderer_set_logical_size(renderer, (float)width, (float)height);
    vk_renderer_set_draw_color(renderer, 0.10f, 0.20f, 0.35f, 1.0f);
    SDL_Rect background = {0, 0, width, height};
    vk_renderer_fill_rect(renderer, &background);
    vk_renderer_set_draw_color(renderer, 0.90f, 0.25f, 0.10f, 1.0f);
    SDL_Rect marker = {width / 4, height / 4, width / 2, height / 2};
    vk_renderer_fill_rect(renderer, &marker);
    vk_renderer_draw_line_mesh_affine_tinted(
        renderer, &line_mesh,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.20f, 0.95f, 0.35f, 1.0f);

    result = vk_renderer_end_frame(renderer, command);
    if (renderer->context.device &&
        renderer->context.device->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(renderer->context.device->device);
    }
    vk_renderer_destroy_line_mesh(renderer, &line_mesh);
    if (result != VK_SUCCESS) return fail("end frame", result);
    return 0;
}

static int verify_ppm(const char* path, uint32_t width, uint32_t height) {
    FILE* file = fopen(path, "rb");
    if (!file) return 1;
    char magic[3] = {0};
    unsigned parsed_width = 0;
    unsigned parsed_height = 0;
    unsigned max_value = 0;
    int matched = fscanf(file, "%2s %u %u %u", magic, &parsed_width,
                         &parsed_height, &max_value);
    int separator = fgetc(file);
    if (matched != 4 || strcmp(magic, "P6") != 0 || max_value != 255u ||
        parsed_width != width || parsed_height != height || separator == EOF) {
        fclose(file);
        return 1;
    }
    long payload_offset = ftell(file);
    unsigned char first_pixel[3] = {0};
    if (payload_offset < 0 || fread(first_pixel, 1, sizeof(first_pixel), file) !=
                                  sizeof(first_pixel)) {
        fclose(file);
        return 1;
    }
    int found_distinct_pixel = 0;
    unsigned char pixel[3] = {0};
    while (fread(pixel, 1, sizeof(pixel), file) == sizeof(pixel)) {
        if (memcmp(pixel, first_pixel, sizeof(pixel)) != 0) {
            found_distinct_pixel = 1;
            break;
        }
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 1;
    }
    long size = ftell(file);
    fclose(file);
    long expected_size = payload_offset + (long)((size_t)width * height * 3u);
    return found_distinct_pixel && size == expected_size ? 0 : 1;
}

int main(void) {
    const char* first_path = "build/vk_renderer_capture_initial.ppm";
    const char* resized_path = "build/vk_renderer_capture_resized.ppm";
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "vk_renderer automated proof", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, 320, 240,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE |
            SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    VkRenderer renderer;
    VkRendererConfig config;
    vk_renderer_config_set_defaults(&config);
    config.enable_validation = VK_TRUE;
    VkResult result = vk_renderer_init(&renderer, window, &config);
    if (result != VK_SUCCESS) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return fail("initialize", result);
    }

    VkRendererDevice* device = renderer.context.device;
    if (!device || device->instance != device->runtime.instance ||
        device->device != device->runtime.device ||
        device->graphics_queue != device->runtime.graphics_queue ||
        device->present_queue != device->runtime.present_queue) {
        vk_renderer_shutdown(&renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return fail("runtime compatibility mirrors", VK_ERROR_UNKNOWN);
    }

    VkExtent2D first_extent = renderer.context.swapchain.extent;
    if (render_capture(&renderer, first_path, 320, 240) != 0 ||
        verify_ppm(first_path, first_extent.width, first_extent.height) != 0) {
        vk_renderer_shutdown(&renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return fail("initial readback", VK_ERROR_UNKNOWN);
    }

    SDL_SetWindowSize(window, 480, 300);
    SDL_PumpEvents();
    result = vk_renderer_recover_surface(
        &renderer, window, VK_ERROR_OUT_OF_DATE_KHR);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown(&renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return fail("out-of-date recovery", result);
    }
    VkExtent2D resized_extent = renderer.context.swapchain.extent;
    if (resized_extent.width == 0u || resized_extent.height == 0u ||
        (resized_extent.width == first_extent.width &&
         resized_extent.height == first_extent.height) ||
        render_capture(&renderer, resized_path, 480, 300) != 0 ||
        verify_ppm(resized_path, resized_extent.width, resized_extent.height) != 0) {
        vk_renderer_shutdown(&renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return fail("resized readback", VK_ERROR_UNKNOWN);
    }

    const VkRuntimeCapabilityReport* report =
        vk_runtime_get_capability_report(&device->runtime);
    if (!report || !report->validation_enabled ||
        report->validation_load_failed ||
        report->validation_error_count != 0u) {
        vk_renderer_shutdown(&renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return fail("validation errors", VK_ERROR_VALIDATION_FAILED_EXT);
    }

    vk_renderer_shutdown(&renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("vk_renderer runtime/readback/resize/capture proof: ok\n");
    return 0;
}
