#include "kit_render.h"

#include <stdio.h>

static void test_release_callback(void *backend_handle, void *user) {
    int *release_count = (int *)user;

    if (backend_handle) {
        *release_count += 1;
    }
}

static int test_frame_recording(void) {
    KitRenderContext ctx;
    KitRenderCommand storage[12];
    KitRenderCommandBuffer buffer;
    KitRenderFrame frame;
    KitRenderRectCommand rect_cmd;
    KitRenderLineCommand line_cmd;
    KitRenderVec2 points[3];
    KitRenderPolylineCommand polyline_cmd;
    KitRenderTextCommand text_cmd;
    CoreResult result;

    buffer.commands = storage;
    buffer.capacity = 12;
    buffer.count = 99;

    result = kit_render_context_init(&ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_context_init failed: %d\n", (int)result.code);
        return 1;
    }

    result = kit_render_begin_frame(&ctx, 1280, 720, &buffer, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_begin_frame failed: %d\n", (int)result.code);
        return 1;
    }
    if (buffer.count != 0) {
        fprintf(stderr, "expected command buffer reset\n");
        return 1;
    }

    result = kit_render_push_clear(&frame, (KitRenderColor){10, 20, 30, 255});
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_set_clip(&frame, (KitRenderRect){5.0f, 5.0f, 250.0f, 180.0f});
    if (result.code != CORE_OK) return 1;

    rect_cmd.rect = (KitRenderRect){20.0f, 30.0f, 100.0f, 60.0f};
    rect_cmd.corner_radius = 8.0f;
    rect_cmd.color = (KitRenderColor){200, 100, 50, 255};
    rect_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_rect(&frame, &rect_cmd);
    if (result.code != CORE_OK) return 1;

    line_cmd.p0 = (KitRenderVec2){0.0f, 0.0f};
    line_cmd.p1 = (KitRenderVec2){100.0f, 100.0f};
    line_cmd.thickness = 2.0f;
    line_cmd.color = (KitRenderColor){255, 255, 255, 255};
    line_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_line(&frame, &line_cmd);
    if (result.code != CORE_OK) return 1;

    points[0] = (KitRenderVec2){0.0f, 5.0f};
    points[1] = (KitRenderVec2){10.0f, 15.0f};
    points[2] = (KitRenderVec2){30.0f, 10.0f};
    polyline_cmd.points = points;
    polyline_cmd.point_count = 3;
    polyline_cmd.thickness = 1.5f;
    polyline_cmd.color = (KitRenderColor){80, 90, 100, 255};
    polyline_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_polyline(&frame, &polyline_cmd);
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_textured_quad(
        &frame,
        &(KitRenderTexturedQuadCommand){
            (KitRenderRect){100.0f, 120.0f, 64.0f, 64.0f},
            7u,
            (KitRenderVec2){0.0f, 0.0f},
            (KitRenderVec2){1.0f, 1.0f},
            (KitRenderColor){255, 255, 255, 255},
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_clear_clip(&frame);
    if (result.code != CORE_OK) return 1;

    text_cmd.origin = (KitRenderVec2){40.0f, 50.0f};
    text_cmd.text = "kit-render";
    text_cmd.font_role = CORE_FONT_ROLE_UI_REGULAR;
    text_cmd.text_tier = CORE_FONT_TEXT_SIZE_BASIC;
    text_cmd.color_token = CORE_THEME_COLOR_TEXT_PRIMARY;
    text_cmd.transform = kit_render_identity_transform();
    result = kit_render_push_text(&frame, &text_cmd);
    if (result.code != CORE_OK) return 1;

    if (buffer.count != 8) {
        fprintf(stderr, "expected 8 commands, got %zu\n", buffer.count);
        return 1;
    }
    if (buffer.commands[1].kind != KIT_RENDER_CMD_SET_CLIP) return 1;
    if (buffer.commands[5].kind != KIT_RENDER_CMD_TEXTURED_QUAD) return 1;
    if (buffer.commands[7].kind != KIT_RENDER_CMD_TEXT) return 1;

    result = kit_render_end_frame(&ctx, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_end_frame failed: %d\n", (int)result.code);
        return 1;
    }

    return 0;
}

static int test_theme_color_resolution(void) {
    KitRenderContext ctx;
    KitRenderColor color;
    CoreResult result;

    result = kit_render_context_init(&ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_IDE_GRAY,
                                     CORE_FONT_PRESET_IDE);
    if (result.code != CORE_OK) return 1;

    result = kit_render_resolve_theme_color(&ctx, CORE_THEME_COLOR_TEXT_PRIMARY, &color);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_resolve_theme_color failed: %d\n", (int)result.code);
        return 1;
    }

    if (color.a == 0) {
        fprintf(stderr, "expected non-zero alpha\n");
        return 1;
    }
    return 0;
}

static int test_text_metrics(void) {
    KitRenderContext ctx;
    KitRenderTextMetrics metrics;
    CoreResult result;

    result = kit_render_context_init(&ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;

    result = kit_render_measure_text(&ctx,
                                     CORE_FONT_ROLE_UI_REGULAR,
                                     CORE_FONT_TEXT_SIZE_PARAGRAPH,
                                     "kit-render",
                                     &metrics);
    if (result.code != CORE_OK) return 1;
    if (metrics.width_px <= 0.0f || metrics.height_px <= 0.0f) {
        fprintf(stderr, "expected positive text metrics\n");
        return 1;
    }

    result = kit_render_measure_text(&ctx,
                                     CORE_FONT_ROLE_UI_REGULAR,
                                     CORE_FONT_TEXT_SIZE_PARAGRAPH,
                                     "",
                                     &metrics);
    if (result.code != CORE_OK) return 1;
    if (metrics.width_px != 0.0f || metrics.height_px <= 0.0f) {
        fprintf(stderr, "unexpected empty-text metrics\n");
        return 1;
    }

    result = kit_render_measure_text(&ctx,
                                     CORE_FONT_ROLE_UI_REGULAR,
                                     CORE_FONT_TEXT_SIZE_PARAGRAPH,
                                     "x",
                                     0);
    if (result.code != CORE_ERR_INVALID_ARG) {
        fprintf(stderr, "expected invalid-arg for null metrics out\n");
        return 1;
    }

    return 0;
}

static int test_runtime_preset_switching(void) {
    KitRenderContext ctx;
    KitRenderCommand storage[2];
    KitRenderCommandBuffer buffer;
    KitRenderFrame frame;
    CoreResult result;

    result = kit_render_context_init(&ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) {
        fprintf(stderr, "runtime preset test: init failed: %d\n", (int)result.code);
        return 1;
    }
    if (ctx.theme.id != CORE_THEME_PRESET_DAW_DEFAULT) {
        fprintf(stderr, "runtime preset test: wrong initial theme id\n");
        return 1;
    }
    if (ctx.font.id != CORE_FONT_PRESET_DAW_DEFAULT) {
        fprintf(stderr, "runtime preset test: wrong initial font id\n");
        return 1;
    }

    result = kit_render_set_theme_preset(&ctx, CORE_THEME_PRESET_GREYSCALE);
    if (result.code != CORE_OK) {
        fprintf(stderr,
                "runtime preset test: theme switch failed: %d (frame_open=%d, target=%d)\n",
                (int)result.code,
                ctx.frame_open,
                (int)CORE_THEME_PRESET_GREYSCALE);
        return 1;
    }
    if (ctx.theme.id != CORE_THEME_PRESET_GREYSCALE) {
        fprintf(stderr, "runtime preset test: theme id did not update\n");
        return 1;
    }

    result = kit_render_set_font_preset(&ctx, CORE_FONT_PRESET_IDE);
    if (result.code != CORE_OK) {
        fprintf(stderr, "runtime preset test: font switch failed: %d\n", (int)result.code);
        return 1;
    }
    if (ctx.font.id != CORE_FONT_PRESET_IDE) {
        fprintf(stderr, "runtime preset test: font id did not update\n");
        return 1;
    }

    buffer.commands = storage;
    buffer.capacity = 2;
    buffer.count = 0;
    result = kit_render_begin_frame(&ctx, 64, 64, &buffer, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "runtime preset test: begin_frame failed: %d\n", (int)result.code);
        return 1;
    }

    result = kit_render_set_theme_preset(&ctx, CORE_THEME_PRESET_DAW_DEFAULT);
    if (result.code != CORE_ERR_INVALID_ARG) {
        fprintf(stderr, "runtime preset test: expected theme switch rejection during frame\n");
        return 1;
    }
    result = kit_render_set_font_preset(&ctx, CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_ERR_INVALID_ARG) {
        fprintf(stderr, "runtime preset test: expected font switch rejection during frame\n");
        return 1;
    }

    result = kit_render_end_frame(&ctx, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "runtime preset test: end_frame failed: %d\n", (int)result.code);
        return 1;
    }
    return 0;
}

static int test_invalid_commands(void) {
    KitRenderContext ctx;
    KitRenderCommand storage[2];
    KitRenderCommandBuffer buffer;
    KitRenderFrame frame;
    CoreResult result;

    buffer.commands = storage;
    buffer.capacity = 2;
    buffer.count = 0;

    result = kit_render_context_init(&ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;
    result = kit_render_begin_frame(&ctx, 32, 32, &buffer, &frame);
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_set_clip(&frame, (KitRenderRect){0.0f, 0.0f, -1.0f, 10.0f});
    if (result.code != CORE_ERR_INVALID_ARG) return 1;

    result = kit_render_push_textured_quad(
        &frame,
        &(KitRenderTexturedQuadCommand){
            (KitRenderRect){0.0f, 0.0f, 8.0f, 8.0f},
            1u,
            (KitRenderVec2){0.0f, 0.0f},
            (KitRenderVec2){1.0f, 1.0f},
            (KitRenderColor){255, 255, 255, 255},
            {0.0f, 0.0f, 1.0f, 1.0f}
        });
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_clear_clip(&frame);
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_clear(&frame, (KitRenderColor){0, 0, 0, 255});
    if (result.code != CORE_ERR_INVALID_ARG) return 1;

    result = kit_render_end_frame(&ctx, &frame);
    if (result.code != CORE_OK) return 1;
    return 0;
}

static int test_external_backend_attachment(void) {
    KitRenderContext null_ctx;
    KitRenderContext vk_ctx;
    int fake_backend = 42;
    int owned_backend = 84;
    int release_count = 0;
    CoreResult result;

    result = kit_render_context_init(&null_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;
    result = kit_render_attach_external_backend(&null_ctx, &fake_backend);
    if (result.code != CORE_ERR_INVALID_ARG) return 1;

    result = kit_render_context_init(&vk_ctx,
                                     KIT_RENDER_BACKEND_VULKAN,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;
    result = kit_render_attach_external_backend(&vk_ctx, &fake_backend);
    if (result.code != CORE_OK) return 1;
    kit_render_context_shutdown(&vk_ctx);

    result = kit_render_context_init(&vk_ctx,
                                     KIT_RENDER_BACKEND_VULKAN,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) return 1;
    result = kit_render_adopt_external_backend(&vk_ctx,
                                               &owned_backend,
                                               test_release_callback,
                                               &release_count);
    if (result.code != CORE_OK) return 1;
    if (release_count != 0) return 1;
    kit_render_context_shutdown(&vk_ctx);
    if (release_count != 1) return 1;

    return 0;
}

int main(void) {
    if (test_frame_recording() != 0) return 1;
    if (test_theme_color_resolution() != 0) return 1;
    if (test_text_metrics() != 0) return 1;
    if (test_runtime_preset_switching() != 0) return 1;
    if (test_invalid_commands() != 0) return 1;
    if (test_external_backend_attachment() != 0) return 1;

    puts("kit_render tests passed");
    return 0;
}
