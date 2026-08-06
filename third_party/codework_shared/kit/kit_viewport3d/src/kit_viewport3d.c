#include "kit_viewport3d.h"

#include <math.h>
#include <stdlib.h>

static bool kit_viewport3d_depth_at(const KitViewport3dOutlineParams* params,
                                    size_t pixel,
                                    double* out_depth) {
    if (!params || !params->depth || !out_depth) return false;
    if (params->depth_format == KIT_VIEWPORT3D_DEPTH_F32) {
        *out_depth = (double)((const float*)params->depth)[pixel];
    } else if (params->depth_format == KIT_VIEWPORT3D_DEPTH_F64) {
        *out_depth = ((const double*)params->depth)[pixel];
    } else {
        return false;
    }
    return true;
}

static int32_t kit_viewport3d_owner_at(const KitViewport3dOutlineParams* params,
                                       size_t pixel) {
    return params->owner ? params->owner[pixel] : 0;
}

KitViewport3dOutlinePalette kit_viewport3d_outline_palette_default(void) {
    KitViewport3dOutlinePalette palette = {
        .object_accents = {
            {76u, 176u, 232u, 250u},
            {91u, 197u, 181u, 250u},
            {132u, 162u, 238u, 250u},
            {174u, 143u, 220u, 250u},
            {99u, 188u, 221u, 250u},
            {115u, 205u, 157u, 250u}
        },
        .object_accent_count = KIT_VIEWPORT3D_OBJECT_ACCENT_CAP,
        .selected = {255u, 168u, 76u, 255u},
        .hover = {104u, 232u, 255u, 255u}
    };
    return palette;
}

KitViewport3dColor kit_viewport3d_outline_color(
    const KitViewport3dOutlinePalette* palette,
    int32_t owner,
    int32_t selected_owner,
    int32_t hover_owner) {
    const KitViewport3dOutlinePalette fallback = kit_viewport3d_outline_palette_default();
    const bool palette_valid = palette && palette->object_accent_count > 0u &&
                               palette->object_accent_count <=
                                   KIT_VIEWPORT3D_OBJECT_ACCENT_CAP;
    const KitViewport3dOutlinePalette* resolved = palette_valid ? palette : &fallback;
    const size_t count = resolved->object_accent_count;
    const uint32_t index = owner >= 0 ? (uint32_t)owner : 0u;
    if (owner >= 0 && owner == selected_owner) return resolved->selected;
    if (owner >= 0 && owner == hover_owner) return resolved->hover;
    return resolved->object_accents[index % count];
}

bool kit_viewport3d_apply_outline(const KitViewport3dOutlineParams* params,
                                  size_t* out_outline_pixels) {
    uint8_t* boundary = NULL;
    size_t count = 0u;
    static const int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    if (out_outline_pixels) *out_outline_pixels = 0u;
    if (!params || !params->rgba || !params->depth || params->width <= 2 ||
        params->height <= 2 || !isfinite(params->relative_depth_threshold) ||
        params->relative_depth_threshold < 0.0 ||
        (params->depth_format != KIT_VIEWPORT3D_DEPTH_F32 &&
         params->depth_format != KIT_VIEWPORT3D_DEPTH_F64)) {
        return false;
    }
    boundary = (uint8_t*)calloc((size_t)params->width * (size_t)params->height,
                                sizeof(*boundary));
    if (!boundary) return false;

    for (int y = 1; y < params->height - 1; ++y) {
        for (int x = 1; x < params->width - 1; ++x) {
            const size_t pixel = (size_t)y * (size_t)params->width + (size_t)x;
            const int32_t owner = kit_viewport3d_owner_at(params, pixel);
            double pixel_depth = 0.0;
            if (params->rgba[pixel * 4u + 3u] == 0u || owner < 0 ||
                !kit_viewport3d_depth_at(params, pixel, &pixel_depth) ||
                !isfinite(pixel_depth)) {
                continue;
            }
            for (size_t n = 0u; n < 4u; ++n) {
                const size_t neighbor =
                    (size_t)(y + offsets[n][1]) * (size_t)params->width +
                    (size_t)(x + offsets[n][0]);
                const int32_t neighbor_owner = kit_viewport3d_owner_at(params, neighbor);
                const bool empty = params->rgba[neighbor * 4u + 3u] == 0u ||
                                   neighbor_owner < 0;
                const bool object_edge = !empty && neighbor_owner != owner;
                double neighbor_depth = 0.0;
                const bool depth_edge = !empty &&
                    kit_viewport3d_depth_at(params, neighbor, &neighbor_depth) &&
                    isfinite(neighbor_depth) &&
                    fabs(pixel_depth - neighbor_depth) >
                        params->relative_depth_threshold * (1.0 + fabs(pixel_depth));
                if (empty || object_edge || depth_edge) {
                    boundary[pixel] = 1u;
                    break;
                }
            }
        }
    }

    for (size_t pixel = 0u;
         pixel < (size_t)params->width * (size_t)params->height;
         ++pixel) {
        if (boundary[pixel]) {
            const KitViewport3dColor color = kit_viewport3d_outline_color(
                params->palette,
                kit_viewport3d_owner_at(params, pixel),
                params->selected_owner,
                params->hover_owner);
            params->rgba[pixel * 4u + 0u] = color.r;
            params->rgba[pixel * 4u + 1u] = color.g;
            params->rgba[pixel * 4u + 2u] = color.b;
            params->rgba[pixel * 4u + 3u] = color.a;
            count += 1u;
        } else if (params->outline_only) {
            params->rgba[pixel * 4u + 0u] = 0u;
            params->rgba[pixel * 4u + 1u] = 0u;
            params->rgba[pixel * 4u + 2u] = 0u;
            params->rgba[pixel * 4u + 3u] = 0u;
        }
    }
    free(boundary);
    if (out_outline_pixels) *out_outline_pixels = count;
    return true;
}
