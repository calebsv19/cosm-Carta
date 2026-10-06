#include "kit_ui.h"
#include "vk_renderer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Shape {
    KitRenderRect rect;
    float radius;
    float border;
    int clipped;
    KitRenderRect clip;
} Shape;

static const Shape shapes[] = {
    {{8, 8, 40, 24}, 0, 0, 0, {0}},
    {{58, 8, 40, 24}, 8, 0, 0, {0}},
    {{108, 8, 56, 24}, 9999, 0, 0, {0}},
    {{8, 48, 40, 24}, 9999, 0, 0, {0}},
    {{58, 48, 40, 24}, 8, 0, 1, {72, 46, 26, 28}},
    {{108, 48, 56, 24}, 8, 0, 0, {0}},
    {{8, 88, 40, 24}, 8, 3, 0, {0}},
    {{58, 88, 40, 24}, 7, 1, 0, {0}}
};
static KitRenderRect caption_bounds;

static CoreResult push_rect(KitRenderFrame *frame, KitRenderRect rect,
                            float radius, KitRenderColor color) {
    KitRenderRectCommand command = {rect, radius, color, {0, 0, 1, 1}};
    return kit_render_push_rect(frame, &command);
}

static int draw_fixture(VkRenderer *renderer, KitRenderContext *context, KitUiContext *ui,
                        uint32_t width, uint32_t height) {
    KitRenderCommand storage[32];
    KitRenderCommandBuffer buffer = {storage, 32, 0};
    KitRenderFrame frame;
    const KitRenderColor colors[] = {
        {230, 80, 60, 255}, {90, 190, 100, 255}, {100, 140, 230, 255},
        {190, 110, 220, 255}, {80, 200, 210, 255}, {255, 0, 0, 128},
        {255, 200, 30, 255}
    };
    if (kit_render_begin_frame(context, width, height, &buffer, &frame).code != CORE_OK ||
        kit_render_push_clear(&frame, (KitRenderColor){20, 25, 35, 255}).code != CORE_OK)
        return 1;
    for (size_t i = 0; i < 7; ++i) {
        const Shape *shape = shapes + i;
        if (shape->clipped && kit_render_push_set_clip(&frame, shape->clip).code != CORE_OK)
            return 1;
        if (push_rect(&frame, shape->rect, shape->radius, colors[i]).code != CORE_OK)
            return 1;
        if (shape->border > 0) {
            KitRenderRect inset = {shape->rect.x + shape->border, shape->rect.y + shape->border,
                                   shape->rect.width - 2 * shape->border,
                                   shape->rect.height - 2 * shape->border};
            if (push_rect(&frame, inset, shape->radius - shape->border,
                          (KitRenderColor){240, 240, 240, 255}).code != CORE_OK) return 1;
        }
        if (shape->clipped && kit_render_push_clear_clip(&frame).code != CORE_OK) return 1;
    }
    KitUiButtonSpec spec;
    KitUiButtonAppearance appearance;
    KitUiButtonLayout layout;
    KitRenderTextMetrics metrics;
    kit_ui_button_spec_init(&spec, "OK");
    spec.state.selected = 1;
    if (kit_render_measure_text(context, CORE_FONT_ROLE_UI_REGULAR,
                                  CORE_FONT_TEXT_SIZE_CAPTION, spec.label, &metrics).code != CORE_OK)
        return 1;
    layout.text_offset_x = (shapes[7].rect.width - metrics.width_px) * 0.5f;
    layout.text_offset_y = shapes[7].rect.height * 0.5f;
    caption_bounds = (KitRenderRect){shapes[7].rect.x + layout.text_offset_x,
                                     shapes[7].rect.y + layout.text_offset_y - metrics.height_px * 0.5f,
                                     metrics.width_px, metrics.height_px};
    if (!kit_ui_button_appearance_preset(KIT_UI_BUTTON_APPEARANCE_COMPACT_ROUNDED, &appearance) ||
        kit_ui_draw_button_spec_appearance_custom(ui, &frame, shapes[7].rect, &spec,
                                                   &layout, &appearance, CORE_FONT_ROLE_UI_REGULAR,
                                                   CORE_FONT_TEXT_SIZE_CAPTION).code != CORE_OK)
        return 1;
    const VkRuntimeCapabilityReport *report = vk_runtime_get_capability_report(
        &renderer->context.device->runtime);
    if (report && report->validation_error_count) {
        fprintf(stderr, "rounded proof: validation rejected frame before submission\n");
        return 1;
    }
    return kit_render_end_frame(context, &frame).code != CORE_OK;
}

/* Independent signed-distance oracle: it never uses the renderer's mesh. */
static float distance_to_shape(KitRenderRect rect, float radius, float x, float y) {
    float r = fminf(fmaxf(radius, 0), fminf(rect.width, rect.height) * 0.5f);
    float qx = fabsf(x - rect.x - rect.width * 0.5f) - rect.width * 0.5f + r;
    float qy = fabsf(y - rect.y - rect.height * 0.5f) - rect.height * 0.5f + r;
    return hypotf(fmaxf(qx, 0), fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - r;
}

static int same_rgb(const unsigned char *a, const unsigned char *b) {
    for (int i = 0; i < 3; ++i) if (abs((int)a[i] - (int)b[i]) > 2) return 0;
    return 1;
}

static const unsigned char *pixel(const unsigned char *pixels, uint32_t width,
                                  int x, int y) {
    return pixels + ((size_t)y * width + (size_t)x) * 3;
}

static int verify_capture(const char *path, VkExtent2D extent, unsigned scale) {
    FILE *file = fopen(path, "rb");
    unsigned width = 0, height = 0, max_value = 0;
    char magic[3] = {0};
    if (!file) return 1;
    if (fscanf(file, "%2s %u %u %u", magic, &width, &height, &max_value) != 4 ||
        strcmp(magic, "P6") || max_value != 255 || width != extent.width || height != extent.height ||
        fgetc(file) == EOF) { fclose(file); return 1; }
    size_t bytes = (size_t)width * height * 3;
    unsigned char *pixels = malloc(bytes);
    if (!pixels) { fclose(file); return 1; }
    int failed = fread(pixels, 1, bytes, file) != bytes || fgetc(file) != EOF;
    fclose(file);
    if (failed) { free(pixels); return 1; }
    const unsigned char *background = pixel(pixels, width, 0, 0);
    size_t checked = 0, corners = 0;
    for (size_t i = 0; i < sizeof(shapes) / sizeof(shapes[0]) && !failed; ++i) {
        const Shape *shape = shapes + i;
        int center_x = (int)((shape->rect.x + (i == 7 ? 3.0f : shape->rect.width * 0.5f)) * scale);
        int center_y = (int)((shape->rect.y + shape->rect.height * 0.5f) * scale);
        const unsigned char *fill = pixel(pixels, width, center_x, center_y);
        const unsigned char *border = pixel(pixels, width, center_x,
                                            (int)((shape->rect.y + 1.5f) * scale));
        if (same_rgb(fill, background)) { failed = 1; break; }
        for (int py = (int)(shape->rect.y * scale) - 2;
             py < (int)((shape->rect.y + shape->rect.height) * scale) + 2 && !failed; ++py) {
            for (int px = (int)(shape->rect.x * scale) - 2;
                 px < (int)((shape->rect.x + shape->rect.width) * scale) + 2; ++px) {
                float x = (px + 0.5f) / scale;
                float y = (py + 0.5f) / scale;
                /* Font rasterization has its own tests; exclude only the caption area. */
                if (i == 7 && x >= caption_bounds.x - 1 &&
                    x < caption_bounds.x + caption_bounds.width + 1 &&
                    y >= caption_bounds.y - 1 && y < caption_bounds.y + caption_bounds.height + 1) continue;
                float distance = distance_to_shape(shape->rect, shape->radius, x, y) * scale;
                if (fabsf(distance) < 1.25f) continue; /* Coverage fringe and raster edges. */
                int inside = distance < 0;
                if (shape->clipped) {
                    float clip_distance = distance_to_shape(shape->clip, 0, x, y) * scale;
                    if (fabsf(clip_distance) < 1.25f) continue;
                    inside &= clip_distance < 0;
                }
                const unsigned char *expected = inside ? fill : background;
                if (inside && shape->border > 0) {
                    KitRenderRect inset = {shape->rect.x + shape->border, shape->rect.y + shape->border,
                                           shape->rect.width - 2 * shape->border,
                                           shape->rect.height - 2 * shape->border};
                    float inner = distance_to_shape(inset, shape->radius - shape->border, x, y) * scale;
                    if (fabsf(inner) < 1.25f) continue;
                    if (inner > 0) expected = border;
                }
                if (!same_rgb(pixel(pixels, width, px, py), expected)) {
                    fprintf(stderr, "rounded pixel mismatch: scale=%u shape=%zu pixel=%d,%d distance=%.3f\n",
                            scale, i, px, py, distance);
                    failed = 1;
                    break;
                }
                ++checked;
                if (!inside && x >= shape->rect.x && y >= shape->rect.y &&
                    x < shape->rect.x + shape->rect.width && y < shape->rect.y + shape->rect.height)
                    ++corners;
            }
        }
    }
    /* Transparent fill must differ from both the background and opaque red. */
    const unsigned char *alpha = pixel(pixels, width, 136 * scale, 60 * scale);
    if (same_rgb(alpha, background) || alpha[0] <= background[0] || alpha[0] >= 250) failed = 1;
    if (checked < 1000 || corners < 20) failed = 1;
    if (!failed) printf("rounded Vulkan image proof: scale=%u checked=%zu excluded_corners=%zu capture=%s\n",
                        scale, checked, corners, path);
    free(pixels);
    return failed;
}

int main(void) {
    int failed = 1;
    if (SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_Window *window = SDL_CreateWindow("shared rounded image proof", 0, 0, 400, 280,
                                           SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (!window) { SDL_Quit(); return 1; }
    VkRenderer renderer;
    VkRendererConfig config;
    vk_renderer_config_set_defaults(&config);
    config.enable_validation = VK_TRUE;
    if (vk_renderer_init(&renderer, window, &config) != VK_SUCCESS) goto window_cleanup;
    /* Force a real buffer replacement after the clear draw has been recorded. */
    vk_renderer_memory_destroy_buffer(&renderer.context, &renderer.frames[0].vertex_buffer);
    if (vk_renderer_memory_create_buffer(&renderer.context, 256,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &renderer.frames[0].vertex_buffer) != VK_SUCCESS) goto renderer_cleanup;
    KitRenderContext context;
    if (kit_render_context_init(&context, KIT_RENDER_BACKEND_VULKAN,
                                CORE_THEME_PRESET_DAW_DEFAULT, CORE_FONT_PRESET_DAW_DEFAULT).code != CORE_OK)
        goto renderer_cleanup;
    if (kit_render_attach_external_backend(&context, &renderer).code != CORE_OK) goto context_cleanup;
    KitUiContext ui;
    if (kit_ui_context_init(&ui, &context).code != CORE_OK) goto context_cleanup;
    for (unsigned scale = 1; scale <= 2; ++scale) {
        if (scale == 2) {
            SDL_SetWindowSize(window, 440, 300);
            SDL_PumpEvents();
            if (vk_renderer_recover_surface(&renderer, window, VK_ERROR_OUT_OF_DATE_KHR) != VK_SUCCESS)
                goto context_cleanup;
        }
        VkExtent2D extent = renderer.context.swapchain.extent;
        if (extent.width < 180 * scale || extent.height < 120 * scale ||
            extent.width % scale || extent.height % scale) goto context_cleanup;
        char path[128];
        snprintf(path, sizeof(path), "build/rounded-%ux.ppm", scale);
        if (vk_renderer_request_capture(&renderer, path) != VK_SUCCESS ||
            draw_fixture(&renderer, &context, &ui, extent.width / scale, extent.height / scale) ||
            verify_capture(path, extent, scale)) goto context_cleanup;
        if (scale == 1) {
            if (!renderer.frames[0].retired_vertex_buffer_count) goto context_cleanup;
            for (uint32_t reuse = 0; reuse < renderer.frame_count; ++reuse)
                if (draw_fixture(&renderer, &context, &ui, extent.width, extent.height)) goto context_cleanup;
            if (renderer.frames[0].retired_vertex_buffer_count) goto context_cleanup;
            puts("rounded Vulkan image proof: buffer growth retained recorded draws; frame-fence reuse retired storage");
        }
    }
    const VkRuntimeCapabilityReport *report = vk_runtime_get_capability_report(&renderer.context.device->runtime);
    if (!report || !report->validation_enabled || report->validation_error_count ||
        report->validation_warning_count) goto context_cleanup;
    puts("rounded Vulkan image proof: validation clean, clipping/border/alpha/square/pill/shared-button/resize passed");
    failed = 0;
context_cleanup:
    kit_render_context_shutdown(&context);
renderer_cleanup:
    vk_renderer_shutdown(&renderer);
window_cleanup:
    SDL_DestroyWindow(window);
    SDL_Quit();
    return failed;
}
