#include "kit_render.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "render command contract failed at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

int main(void) {
    KitRenderContext context;
    KitRenderCommand storage[16];
    KitRenderCommandBuffer buffer = {storage, 16, 0};
    KitRenderFrame frame;
    KitRenderVec2 points[2] = {{0, 0}, {12, 8}};
    const KitRenderTransform transform = {40.25f, 10.5f, -2, 1.5f};
    KitRenderRectCommand rect = {{0, 0, 12, 8}, 3, {80, 140, 230, 255}, transform};
    KitRenderLineCommand line = {{0, 0}, {12, 8}, 2, {80, 140, 230, 255}, transform};
    KitRenderPolylineCommand polyline = {points, 2, 2, {80, 140, 230, 255}, transform};
    KitRenderTexturedQuadCommand quad = {{0, 0, 12, 8}, 1, {1, 1}, {0, 0},
                                         {120, 220, 60, 128}, transform};
    KitRenderTextCommand text = {{0, 10}, "F", CORE_FONT_ROLE_UI_REGULAR,
                                  CORE_FONT_TEXT_SIZE_CAPTION, CORE_THEME_COLOR_TEXT_PRIMARY, transform};
    CHECK(kit_render_context_init(&context, KIT_RENDER_BACKEND_NULL,
             CORE_THEME_PRESET_DAW_DEFAULT, CORE_FONT_PRESET_DAW_DEFAULT).code == CORE_OK);
    CHECK(kit_render_begin_frame(&context, 160, 120, &buffer, &frame).code == CORE_OK);
    CHECK(kit_render_push_rect(&frame, &rect).code == CORE_OK);
    CHECK(kit_render_push_line(&frame, &line).code == CORE_OK);
    CHECK(kit_render_push_polyline(&frame, &polyline).code == CORE_OK);
    CHECK(kit_render_push_textured_quad(&frame, &quad).code == CORE_OK);
    CHECK(kit_render_push_text(&frame, &text).code == CORE_OK);
    CHECK(buffer.count == 5 && storage[3].data.textured_quad.uv_min.x == 1 &&
          storage[3].data.textured_quad.tint.a == 128 && storage[0].data.rect.transform.sx == -2);
    size_t before = buffer.count;
    rect.transform.tx = NAN;
    line.transform.ty = INFINITY;
    polyline.transform.sx = NAN;
    quad.transform.sy = -INFINITY;
    text.transform.tx = NAN;
    CHECK(kit_render_push_rect(&frame, &rect).code == CORE_ERR_INVALID_ARG);
    CHECK(kit_render_push_line(&frame, &line).code == CORE_ERR_INVALID_ARG);
    CHECK(kit_render_push_polyline(&frame, &polyline).code == CORE_ERR_INVALID_ARG);
    CHECK(kit_render_push_textured_quad(&frame, &quad).code == CORE_ERR_INVALID_ARG);
    CHECK(kit_render_push_text(&frame, &text).code == CORE_ERR_INVALID_ARG);
    CHECK(buffer.count == before);
    rect.transform = transform;
    rect.transform.sx = FLT_MAX;
    CHECK(kit_render_push_rect(&frame, &rect).code == CORE_ERR_INVALID_ARG);
    quad.transform = transform;
    quad.uv_max.y = NAN;
    CHECK(kit_render_push_textured_quad(&frame, &quad).code == CORE_ERR_INVALID_ARG);
    quad.uv_max.y = 1;
    quad.texture_id = 0;
    CHECK(kit_render_push_textured_quad(&frame, &quad).code == CORE_ERR_INVALID_ARG);
    CHECK(kit_render_push_set_clip(&frame, (KitRenderRect){0, 0, NAN, 2}).code == CORE_ERR_INVALID_ARG);
    CHECK(buffer.count == before);
    /* Revalidate borrowed data at submission, then recover the same open frame. */
    points[1].x = NAN;
    CHECK(kit_render_end_frame(&context, &frame).code == CORE_ERR_INVALID_ARG && context.frame_open);
    points[1].x = 12;
    storage[0].data.rect.transform.sy = INFINITY;
    CHECK(kit_render_end_frame(&context, &frame).code == CORE_ERR_INVALID_ARG && context.frame_open);
    storage[0].data.rect.transform.sy = transform.sy;
    CHECK(kit_render_end_frame(&context, &frame).code == CORE_OK && !context.frame_open);
    CHECK(kit_render_begin_frame(&context, 160, 120, &buffer, &frame).code == CORE_OK);
    rect.transform = (KitRenderTransform){1, 2, 0, -1};
    CHECK(kit_render_push_rect(&frame, &rect).code == CORE_OK);
    CHECK(kit_render_push_set_clip(&frame, (KitRenderRect){1.5f, 2.5f, 0, 0}).code == CORE_OK);
    CHECK(kit_render_end_frame(&context, &frame).code == CORE_OK);
    kit_render_context_shutdown(&context);
    puts("render command contract: signed/zero scale, UV/tint retention, invalid/overflow rejection and same-frame recovery passed");
    return 0;
}
