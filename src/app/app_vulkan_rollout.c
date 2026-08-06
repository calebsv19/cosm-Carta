#include "app/app_vulkan_rollout.h"

#include "render/renderer.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(MAPFORGE_HAVE_VK) && defined(MAPFORGE_HAVE_VK_RUNTIME)
#include "vk_renderer.h"
#include "vk_runtime.h"

static const char *map_forge_rollout_capture_path(const char *name,
                                                  const char *fallback) {
    const char *value = getenv(name);
    return value && value[0] ? value : fallback;
}

static int map_forge_rollout_verify_runtime(const VkRenderer *vk,
                                            const char *stage) {
    const VkRendererDevice *device;
    const VkRuntimeCapabilityReport *report;
    const char *runtime_version;

    if (!vk || !vk->context.device) {
        fprintf(stderr, "map_forge Vulkan rollout: runtime device unavailable\n");
        return 0;
    }
    device = vk->context.device;
    if (device->instance != device->runtime.instance ||
        device->device != device->runtime.device ||
        device->graphics_queue != device->runtime.graphics_queue ||
        device->present_queue != device->runtime.present_queue) {
        fprintf(stderr, "map_forge Vulkan rollout: compatibility mirrors diverged\n");
        return 0;
    }

    runtime_version = vk_runtime_version_string();
    report = vk_runtime_get_capability_report(&device->runtime);
    if (!runtime_version || !runtime_version[0] || !report ||
        report->status != VK_RUNTIME_STATUS_OK ||
        report->selected_device_index >= report->device_count ||
        !report->validation_requested || !report->validation_enabled ||
        report->validation_load_failed ||
        report->validation_warning_count != 0u ||
        report->validation_error_count != 0u) {
        fprintf(stderr, "map_forge Vulkan rollout: runtime/validation report incomplete\n");
        return 0;
    }

    printf("MAPFORGE_VULKAN_ROLLOUT schema=1 stage=%s runtime=%s device=%s validation_requested=%d validation_enabled=%d validation_warnings=%u validation_errors=%u\n",
           stage,
           runtime_version,
           report->devices[report->selected_device_index].device_name,
           report->validation_requested ? 1 : 0,
           report->validation_enabled ? 1 : 0,
           (unsigned int)report->validation_warning_count,
           (unsigned int)report->validation_error_count);
    return 1;
}

static int map_forge_rollout_draw_frame(Renderer *renderer,
                                        const char *capture_path) {
    SDL_FRect panel = {48.0f, 48.0f, 320.0f, 180.0f};
    VkRenderer *vk = (VkRenderer *)renderer->vk;

    if (vk_renderer_request_capture(vk, capture_path) != VK_SUCCESS) {
        fprintf(stderr, "map_forge Vulkan rollout: capture request failed\n");
        return 0;
    }
    renderer_begin_frame(renderer);
    if (renderer->vk_cmd == 0) {
        fprintf(stderr, "map_forge Vulkan rollout: begin frame failed\n");
        return 0;
    }
    renderer_clear(renderer, 18u, 24u, 34u, 255u);
    renderer_set_draw_color(renderer, 47u, 132u, 196u, 255u);
    renderer_fill_rect(renderer, &panel);
    renderer_set_draw_color(renderer, 241u, 196u, 15u, 255u);
    renderer_draw_line(renderer, 48.0f, 260.0f, 520.0f, 96.0f);
    renderer_end_frame(renderer);
    return renderer->vk_last_begin_result == VK_SUCCESS;
}
#endif

int map_forge_vulkan_rollout_self_test(void) {
#if !defined(MAPFORGE_HAVE_VK) || !defined(MAPFORGE_HAVE_VK_RUNTIME)
    fprintf(stderr, "map_forge Vulkan rollout: vk_runtime-backed renderer unavailable\n");
    return 1;
#else
    const char *initial_capture = map_forge_rollout_capture_path(
        "MAPFORGE_VULKAN_ROLLOUT_INITIAL_CAPTURE",
        "build/vulkan-rollout/initial.bmp");
    const char *resized_capture = map_forge_rollout_capture_path(
        "MAPFORGE_VULKAN_ROLLOUT_RESIZED_CAPTURE",
        "build/vulkan-rollout/resized.bmp");
    SDL_Window *window = NULL;
    Renderer renderer = {0};
    VkRenderer *vk;
    VkExtent2D initial_extent;
    VkExtent2D resized_extent;
    int exit_code = 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "map_forge Vulkan rollout: SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("Carta Vulkan Rollout Proof",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              1280,
                              720,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "map_forge Vulkan rollout: window creation failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    renderer_set_backend(&renderer, RENDERER_BACKEND_VULKAN);
    if (!renderer_init(&renderer, window, 1280, 720) ||
        renderer_get_backend(&renderer) != RENDERER_BACKEND_VULKAN ||
        !renderer.vulkan_available || !renderer.vk) {
        fprintf(stderr, "map_forge Vulkan rollout: Vulkan backend fell back or failed\n");
        goto cleanup;
    }
    vk = (VkRenderer *)renderer.vk;
    if (!map_forge_rollout_verify_runtime(vk, "startup")) {
        goto cleanup;
    }
    initial_extent = vk->context.swapchain.extent;
    if (!map_forge_rollout_draw_frame(&renderer, initial_capture)) {
        goto cleanup;
    }

    SDL_SetWindowSize(window, 1440, 800);
    SDL_Delay(100);
    SDL_PumpEvents();
    if (!renderer_resize(&renderer, 1440, 800)) {
        fprintf(stderr, "map_forge Vulkan rollout: resize recovery failed\n");
        goto cleanup;
    }
    resized_extent = vk->context.swapchain.extent;
    if (resized_extent.width == 0u || resized_extent.height == 0u ||
        (resized_extent.width == initial_extent.width &&
         resized_extent.height == initial_extent.height)) {
        fprintf(stderr, "map_forge Vulkan rollout: drawable extent did not change\n");
        goto cleanup;
    }
    if (!map_forge_rollout_draw_frame(&renderer, resized_capture) ||
        !map_forge_rollout_verify_runtime(vk, "shutdown")) {
        goto cleanup;
    }

    printf("MAPFORGE_VULKAN_ROLLOUT_RESIZE schema=1 initial=%ux%u resized=%ux%u recreates=%u initial_capture=%s resized_capture=%s\n",
           initial_extent.width,
           initial_extent.height,
           resized_extent.width,
           resized_extent.height,
           renderer.vk_swapchain_recreates,
           initial_capture,
           resized_capture);
    exit_code = 0;

cleanup:
    renderer_shutdown(&renderer);
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    return exit_code;
#endif
}
