#include "kit_render_fidelity_fixture.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "render fidelity failed at line %d: %s\n", __LINE__, #condition); return 1; \
} } while (0)

static int draw_fixture(VkRenderer *renderer, KitRenderContext *context, VkRendererTexture *texture,
                        unsigned scale, KitRenderTextMetrics *metrics) {
    KitRenderCommand commands[40];
    KitRenderCommandBuffer buffer = {commands, 40, 0};
    KitRenderFrame frame;
    VkExtent2D extent = renderer->context.swapchain.extent;
    CHECK(kit_render_begin_frame(context, extent.width / scale, extent.height / scale,
                                  &buffer, &frame).code == CORE_OK);
    CHECK(vk_renderer_set_draw_transform(renderer, NAN, 0, 1, 1) != VK_SUCCESS);
    VkRendererTexture invalid_texture = {0};
    const SDL_FRect bounds = {0, 0, 8, 8};
    const float uv_min[2] = {0, 0}, uv_max[2] = {1, 1}, white[4] = {1, 1, 1, 1};
    CHECK(vk_renderer_draw_textured_quad(renderer, &invalid_texture, &bounds,
                                          uv_min, uv_max, white) != VK_SUCCESS);
    CHECK(renderer->draw_state.draw_call_count == 0);
    CHECK(kit_render_push_clear(&frame, fidelity_background).code == CORE_OK);
    for (size_t i = 0; i + 1 < sizeof(fidelity_rects) / sizeof(*fidelity_rects); ++i) {
        const FidelityRect *rect = fidelity_rects + i;
        KitRenderRectCommand command = {rect->bounds, rect->radius, rect->color, rect->transform};
        CHECK(kit_render_push_rect(&frame, &command).code == CORE_OK);
    }
    KitRenderLineCommand line = {{0, 0}, {30, 0}, 4, {230, 80, 60, 255}, {8, 65, 2, 1.5f}};
    CHECK(kit_render_push_line(&frame, &line).code == CORE_OK);
    line = (KitRenderLineCommand){{0, 0}, {0, 30}, 4, {100, 140, 230, 255}, {110, 55, -1.5f, 1.5f}};
    CHECK(kit_render_push_line(&frame, &line).code == CORE_OK);
    KitRenderPolylineCommand polyline = {fidelity_polyline, 3, 3, {190, 110, 220, 255},
                                         {150, 55, -1.5f, 1.5f}};
    CHECK(kit_render_push_polyline(&frame, &polyline).code == CORE_OK);
    for (size_t i = 0; i < sizeof(fidelity_textures) / sizeof(*fidelity_textures); ++i) {
        const FidelityTexture *test = fidelity_textures + i;
        if (test->clipped) CHECK(kit_render_push_set_clip(&frame, fidelity_clip).code == CORE_OK);
        KitRenderTexturedQuadCommand quad = {{0, 0, 32, 32}, (uint64_t)(uintptr_t)texture,
            test->uv_min, test->uv_max, test->tint, test->transform};
        CHECK(kit_render_push_textured_quad(&frame, &quad).code == CORE_OK);
        if (test->clipped) CHECK(kit_render_push_clear_clip(&frame).code == CORE_OK);
    }
    /* A zero-area fractional clip must stay empty after integer scissor conversion. */
    CHECK(kit_render_push_set_clip(&frame, (KitRenderRect){310.5f, 115.5f, 0, 0}).code == CORE_OK);
    KitRenderRectCommand hidden = {{300, 110, 20, 20}, 0, {255, 0, 0, 255}, {0, 0, 1, 1}};
    CHECK(kit_render_push_rect(&frame, &hidden).code == CORE_OK);
    CHECK(kit_render_push_clear_clip(&frame).code == CORE_OK);
    CHECK(kit_render_measure_text(context, CORE_FONT_ROLE_UI_REGULAR, CORE_FONT_TEXT_SIZE_CAPTION,
                                   "F", metrics).code == CORE_OK);
    for (size_t i = 0; i < sizeof(fidelity_text_transforms) / sizeof(*fidelity_text_transforms); ++i) {
        KitRenderTextCommand text = {{8, 220}, "F", CORE_FONT_ROLE_UI_REGULAR,
            CORE_FONT_TEXT_SIZE_CAPTION, CORE_THEME_COLOR_TEXT_PRIMARY, fidelity_text_transforms[i]};
        CHECK(kit_render_push_text(&frame, &text).code == CORE_OK);
    }
    const FidelityRect *last = fidelity_rects + sizeof(fidelity_rects) / sizeof(*fidelity_rects) - 1;
    KitRenderRectCommand marker = {last->bounds, last->radius, last->color, last->transform};
    CHECK(kit_render_push_rect(&frame, &marker).code == CORE_OK);
    /* Detect mutation of caller-owned command storage before any native draw. */
    commands[1].data.rect.transform.tx = NAN;
    CHECK(kit_render_end_frame(context, &frame).code == CORE_ERR_INVALID_ARG);
    CHECK(context->frame_open && renderer->draw_state.draw_call_count == 0);
    commands[1].data.rect.transform.tx = fidelity_rects[0].transform.tx;
    CHECK(kit_render_end_frame(context, &frame).code == CORE_OK);
    CHECK(renderer->draw_state.transform_enabled == SDL_FALSE);
    return 0;
}

int main(int argc, char **argv) {
    int failed = 1;
    const char *output = argc > 1 ? argv[1] : "build/vk/fidelity";
    CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_Window *window = SDL_CreateWindow("shared render command fidelity", 0, 0, 640, 480,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (!window) { SDL_Quit(); return 1; }
    VkRenderer renderer;
    VkRendererConfig config;
    vk_renderer_config_set_defaults(&config);
    config.enable_validation = VK_TRUE;
    if (vk_renderer_init(&renderer, window, &config) != VK_SUCCESS) goto window_cleanup;
    VkRendererTexture texture = {0};
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 8, 8, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) goto renderer_cleanup;
    for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 8; ++x) {
        KitRenderColor color = fidelity_texel(x, y);
        ((uint32_t *)((unsigned char *)surface->pixels + y * surface->pitch))[x] =
            SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a);
    }
    VkResult uploaded = vk_renderer_upload_sdl_surface_with_filter(&renderer, surface, &texture, VK_FILTER_NEAREST);
    SDL_FreeSurface(surface);
    if (uploaded != VK_SUCCESS) goto renderer_cleanup;
    KitRenderContext context;
    if (kit_render_context_init(&context, KIT_RENDER_BACKEND_VULKAN,
        CORE_THEME_PRESET_DAW_DEFAULT, CORE_FONT_PRESET_DAW_DEFAULT).code != CORE_OK) goto texture_cleanup;
    if (kit_render_attach_external_backend(&context, &renderer).code != CORE_OK) goto context_cleanup;
    /* Force replacement while earlier commands still refer to the old buffer. */
    vk_renderer_memory_destroy_buffer(&renderer.context, &renderer.frames[0].vertex_buffer);
    if (vk_renderer_memory_create_buffer(&renderer.context, 256,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &renderer.frames[0].vertex_buffer) != VK_SUCCESS) goto context_cleanup;
    for (unsigned scale = 1; scale <= 2; ++scale) {
        if (scale == 2) {
            SDL_SetWindowSize(window, 800, 600);
            SDL_PumpEvents();
            if (vk_renderer_recover_surface(&renderer, window, VK_ERROR_OUT_OF_DATE_KHR) != VK_SUCCESS)
                goto context_cleanup;
        }
        VkExtent2D extent = renderer.context.swapchain.extent;
        if (extent.width / scale < 340 || extent.height / scale < 270 ||
            extent.width % scale || extent.height % scale) goto context_cleanup;
        char path[1024];
        if (snprintf(path, sizeof(path), "%s/fidelity-%ux.ppm", output, scale) >= (int)sizeof(path))
            goto context_cleanup;
        KitRenderTextMetrics metrics;
        if (vk_renderer_request_capture(&renderer, path) != VK_SUCCESS ||
            draw_fixture(&renderer, &context, &texture, scale, &metrics) ||
            kit_render_fidelity_verify_capture(path, extent, scale,
                renderer.context.swapchain.image_format, metrics)) goto context_cleanup;
        if (scale == 1) {
            if (!renderer.frames[0].retired_vertex_buffer_count) goto context_cleanup;
            for (uint32_t reuse = 0; reuse < renderer.frame_count; ++reuse)
                if (draw_fixture(&renderer, &context, &texture, scale, &metrics)) goto context_cleanup;
            if (renderer.frames[0].retired_vertex_buffer_count) goto context_cleanup;
            puts("render fidelity image proof: in-flight buffer growth and frame-fence reuse passed");
        }
    }
    const VkRuntimeCapabilityReport *report = vk_runtime_get_capability_report(&renderer.context.device->runtime);
    if (!report || !report->validation_enabled || report->validation_warning_count ||
        report->validation_error_count) goto context_cleanup;
    puts("render fidelity image proof: validation warnings=0 errors=0; transforms/UV/tint/clip/text/resize passed");
    failed = 0;
context_cleanup:
    kit_render_context_shutdown(&context);
texture_cleanup:
    vk_renderer_texture_destroy(&renderer, &texture);
renderer_cleanup:
    vk_renderer_shutdown(&renderer);
window_cleanup:
    SDL_DestroyWindow(window);
    SDL_Quit();
    return failed;
}
