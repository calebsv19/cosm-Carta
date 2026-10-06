#include "vk_renderer.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static float captured[6][8];
static int calls;
// Captures exactly the production emitter's vertex contract without a Vulkan device.
VkResult vk_renderer_emit_textured_vertices(VkRenderer *r, const VkRendererTexture *t, const float (*v)[8],
                                            uint32_t n) {
    (void)r;
    (void)t;
    assert(n == 6);
    memcpy(captured, v, sizeof(captured));
    calls++;
    return VK_SUCCESS;
}
// Verifies rotated corner order, crop UVs, tint and rejection before emission.
int main(void) {
    VkRenderer r = {0};
    VkRendererTexture t = {0};
    t.width = t.height = 258;
    t.descriptor_set = (VkDescriptorSet)1;
    t.image.view = (VkImageView)1;
    t.sampler = (VkSampler)1;
    SDL_FPoint p[4] = {{10, 20}, {70, 80}, {30, 120}, {-30, 60}};
    float lo[2] = {.1, .2}, hi[2] = {.8, .9};
    assert(vk_renderer_draw_texture_corners(&r, &t, p, lo, hi) == VK_SUCCESS);
    assert(calls == 1);
    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) {
        int k = order[i];
        assert(captured[i][0] == p[k].x && captured[i][1] == p[k].y);
        assert(captured[i][2] == (k == 1 || k == 2 ? hi[0] : lo[0]));
        assert(captured[i][3] == (k >= 2 ? hi[1] : lo[1]));
        for (int j = 4; j < 8; j++)
            assert(captured[i][j] == 1);
    }
    p[2].x = NAN;
    assert(vk_renderer_draw_texture_corners(&r, &t, p, lo, hi) != VK_SUCCESS);
    p[2].x = 30;
    hi[1] = 2;
    assert(vk_renderer_draw_texture_corners(&r, &t, p, lo, hi) != VK_SUCCESS);
    hi[1] = .9;
    t.sampler = 0;
    assert(vk_renderer_draw_texture_corners(&r, &t, p, lo, hi) != VK_SUCCESS);
    assert(vk_renderer_draw_texture_corners(&r, &t, NULL, lo, hi) != VK_SUCCESS);
    assert(calls == 1);
    puts("texture corner contract passed");
}
