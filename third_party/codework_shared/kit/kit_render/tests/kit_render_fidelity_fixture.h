#ifndef KIT_RENDER_FIDELITY_FIXTURE_H
#define KIT_RENDER_FIDELITY_FIXTURE_H

#include "kit_render.h"
#include "vk_renderer.h"

typedef struct FidelityRect {
    KitRenderRect bounds;
    float radius;
    KitRenderColor color;
    KitRenderTransform transform;
} FidelityRect;

typedef struct FidelityTexture {
    KitRenderTransform transform;
    KitRenderVec2 uv_min, uv_max;
    KitRenderColor tint;
    int clipped;
} FidelityTexture;

static const KitRenderColor fidelity_background = {20, 25, 35, 255};
static const FidelityRect fidelity_rects[] = {
    {{0, 0, 20, 12}, 0, {230, 80, 60, 255}, {8.25f, 8.5f, 1.5f, 2}},
    {{0, 0, 30, 22}, 7, {90, 190, 100, 255}, {60, 10, 2, 1.25f}},
    {{0, 0, 30, 22}, 7, {190, 110, 220, 255}, {180, 10, -1.5f, 1.25f}},
    {{0, 0, 30, 22}, 7, {255, 0, 0, 255}, {220, 10, 0, 2}},
    {{230, 8, 8, 8}, 0, {255, 200, 30, 255}, {0, 0, 1, 1}},
    {{300, 218, 8, 8}, 0, {255, 200, 30, 255}, {0, 0, 1, 1}},
};
static const KitRenderVec2 fidelity_polyline[] = {{0, 0}, {22, 0}, {22, 22}};
static const FidelityTexture fidelity_textures[] = {
    {{8.5f, 115.25f, 1.5f, 1}, {.25f, 0}, {.75f, 1}, {255, 255, 255, 255}, 0},
    {{115, 115, -1, 1}, {1, 1}, {0, 0}, {180, 210, 120, 255}, 0},
    {{135, 115, 1.5f, 1.25f}, {.125f, .25f}, {.875f, .75f}, {128, 200, 64, 128}, 0},
    {{205, 115, 1, 1}, {0, 0}, {1, 1}, {255, 255, 255, 255}, 1},
    {{265, 115, 1, 1}, {-.5f, -.25f}, {1.5f, 1.25f}, {255, 255, 255, 255}, 0},
};
static const KitRenderRect fidelity_clip = {215, 121, 15, 22};
static const KitRenderTransform fidelity_text_transforms[] = {
    {0, 0, 1, 1}, {62, 0, 1, 1}, {140, -220, 2, 2}, {250, 440, -2, -1},
};

static inline KitRenderColor fidelity_texel(unsigned x, unsigned y) {
    return (KitRenderColor){(uint8_t)(40 + x * 24), (uint8_t)(40 + y * 22),
                             (uint8_t)(30 + (x + y) * 10), (uint8_t)((x + y) % 2 ? 128 : 255)};
}

int kit_render_fidelity_verify_capture(const char *path, VkExtent2D extent, unsigned scale,
                                      VkFormat format, KitRenderTextMetrics text_metrics);

#endif
