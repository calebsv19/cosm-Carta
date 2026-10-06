#include "vk_renderer_draw_internal.h"

#include <math.h>
#include <string.h>

enum { MAX_ARC_SEGMENTS = 32, MAX_POINTS = 4 * (MAX_ARC_SEGMENTS + 1) };

static void vertex(float out[6], float x, float y, const float color[4], float coverage) {
    out[0] = x;
    out[1] = y;
    memcpy(out + 2, color, 4 * sizeof(float));
    out[5] *= coverage;
}

void vk_renderer_fill_rounded_rect(VkRenderer *renderer,
                                    const SDL_FRect *rect,
                                    float corner_radius) {
    float vertices[MAX_POINTS * 9][6];
    float inner[MAX_POINTS][2];
    float outer[MAX_POINTS][2];
    uint32_t count = 0;
    uint32_t emitted = 0;
    if (!renderer || !rect || !isfinite(rect->x) || !isfinite(rect->y) ||
        !isfinite(rect->w) || !isfinite(rect->h) || !isfinite(corner_radius) ||
        rect->w <= 0.0f || rect->h <= 0.0f) return;

    float radius = fminf(fmaxf(corner_radius, 0.0f), fminf(rect->w, rect->h) * 0.5f);
    const float *color = renderer->draw_state.current_color;
    float cx = rect->x + rect->w * 0.5f;
    float cy = rect->y + rect->h * 0.5f;
    if (radius == 0.0f) {
        const float points[4][2] = {
            {rect->x, rect->y}, {rect->x + rect->w, rect->y},
            {rect->x + rect->w, rect->y + rect->h}, {rect->x, rect->y + rect->h}
        };
        const unsigned indices[6] = {0, 1, 2, 0, 2, 3};
        for (uint32_t i = 0; i < 6; ++i)
            vertex(vertices[emitted++], points[indices[i]][0], points[indices[i]][1], color, 1.0f);
        vk_renderer_emit_solid_vertices(renderer, (const float (*)[6])vertices, emitted);
        return;
    }

    float logical_w = renderer->draw_state.logical_size[0];
    float logical_h = renderer->draw_state.logical_size[1];
    float sx = logical_w > 0.0f ? renderer->context.swapchain.extent.width / logical_w : 1.0f;
    float sy = logical_h > 0.0f ? renderer->context.swapchain.extent.height / logical_h : 1.0f;
    if (renderer->draw_state.transform_enabled) {
        sx *= fabsf(renderer->draw_state.transform[2]);
        sy *= fabsf(renderer->draw_state.transform[3]);
    }
    if (sx <= 0.0f || sy <= 0.0f) return;
    /* A one-drawable-pixel coverage fringe, independent of logical UI scale. */
    float fx = fminf(0.5f / sx, radius);
    float fy = fminf(0.5f / sy, radius);
    float requested_segments = ceilf(sqrtf(radius * fmaxf(sx, sy)) * 2.0f);
    int segments = requested_segments >= MAX_ARC_SEGMENTS ? MAX_ARC_SEGMENTS : (int)requested_segments;
    if (segments < 4) segments = 4;
    if (segments > MAX_ARC_SEGMENTS) segments = MAX_ARC_SEGMENTS;
    const float centers[4][2] = {
        {rect->x + radius, rect->y + radius},
        {rect->x + rect->w - radius, rect->y + radius},
        {rect->x + rect->w - radius, rect->y + rect->h - radius},
        {rect->x + radius, rect->y + rect->h - radius}
    };
    const float pi = 3.14159265358979323846f;
    for (int corner = 0; corner < 4; ++corner) {
        for (int step = 0; step <= segments; ++step) {
            float angle = pi + corner * pi * 0.5f + step * pi * 0.5f / segments;
            float dx = cosf(angle);
            float dy = sinf(angle);
            inner[count][0] = centers[corner][0] + dx * (radius - fx);
            inner[count][1] = centers[corner][1] + dy * (radius - fy);
            outer[count][0] = centers[corner][0] + dx * (radius + fx);
            outer[count][1] = centers[corner][1] + dy * (radius + fy);
            ++count;
        }
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t next = (i + 1) % count;
        vertex(vertices[emitted++], cx, cy, color, 1.0f);
        vertex(vertices[emitted++], inner[i][0], inner[i][1], color, 1.0f);
        vertex(vertices[emitted++], inner[next][0], inner[next][1], color, 1.0f);
        vertex(vertices[emitted++], inner[i][0], inner[i][1], color, 1.0f);
        vertex(vertices[emitted++], outer[i][0], outer[i][1], color, 0.0f);
        vertex(vertices[emitted++], outer[next][0], outer[next][1], color, 0.0f);
        vertex(vertices[emitted++], inner[i][0], inner[i][1], color, 1.0f);
        vertex(vertices[emitted++], outer[next][0], outer[next][1], color, 0.0f);
        vertex(vertices[emitted++], inner[next][0], inner[next][1], color, 1.0f);
    }
    vk_renderer_emit_solid_vertices(renderer, (const float (*)[6])vertices, emitted);
}
