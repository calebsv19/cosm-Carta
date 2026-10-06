#include "vk_renderer_draw_internal.h"
#include <math.h>
// Records two triangles after validating corners, texture handles and normalized UVs.
VkResult vk_renderer_draw_texture_corners(VkRenderer *r, const VkRendererTexture *t, const SDL_FPoint p[4],
                                          const float lo[2], const float hi[2]) {
    if (!r || !t || !p || !lo || !hi || !t->width || !t->height || !t->descriptor_set || !t->image.view ||
        !t->sampler)
        return VK_ERROR_INITIALIZATION_FAILED;
    for (int j = 0; j < 4; j++)
        if (!isfinite(p[j].x) || !isfinite(p[j].y))
            return VK_ERROR_INITIALIZATION_FAILED;
    for (int j = 0; j < 2; j++)
        if (!isfinite(lo[j]) || !isfinite(hi[j]) || lo[j] < 0 || hi[j] > 1 || lo[j] > hi[j])
            return VK_ERROR_INITIALIZATION_FAILED;
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    float v[6][8];
    for (int j = 0; j < 6; j++) {
        int k = indices[j];
        v[j][0] = p[k].x;
        v[j][1] = p[k].y;
        v[j][2] = k == 1 || k == 2 ? hi[0] : lo[0];
        v[j][3] = k >= 2 ? hi[1] : lo[1];
        for (int c = 4; c < 8; c++)
            v[j][c] = 1;
    }
    return vk_renderer_emit_textured_vertices(r, t, (const float (*)[8])v, 6);
}
