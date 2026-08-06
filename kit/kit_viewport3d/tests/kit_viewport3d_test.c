#include "kit_viewport3d.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect(const char* name, int condition) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", name);
    failures += 1;
}

static size_t at(int x, int y, int width) {
    return (size_t)y * (size_t)width + (size_t)x;
}

static void test_f32_and_f64_match(void) {
    enum { W = 5, H = 5 };
    uint8_t rgba32[W * H * 4] = {0};
    uint8_t rgba64[W * H * 4] = {0};
    float depth32[W * H];
    double depth64[W * H];
    int32_t owner[W * H];
    size_t count32 = 0u;
    size_t count64 = 0u;
    for (int i = 0; i < W * H; ++i) {
        depth32[i] = INFINITY;
        depth64[i] = INFINITY;
        owner[i] = -1;
    }
    for (int y = 1; y <= 3; ++y) {
        for (int x = 1; x <= 3; ++x) {
            const size_t p = at(x, y, W);
            rgba32[p * 4u + 3u] = 255u;
            rgba64[p * 4u + 3u] = 255u;
            depth32[p] = 2.0f;
            depth64[p] = 2.0;
            owner[p] = x < 3 ? 0 : 1;
        }
    }
    {
        KitViewport3dOutlineParams params = {
            rgba32, depth32, owner, W, H, KIT_VIEWPORT3D_DEPTH_F32,
            0.18, -1, -1, false, NULL};
        expect("f32_apply", kit_viewport3d_apply_outline(&params, &count32));
    }
    {
        KitViewport3dOutlineParams params = {
            rgba64, depth64, owner, W, H, KIT_VIEWPORT3D_DEPTH_F64,
            0.18, -1, -1, false, NULL};
        expect("f64_apply", kit_viewport3d_apply_outline(&params, &count64));
    }
    expect("depth_formats_count_match", count32 == count64 && count32 > 0u);
    expect("depth_formats_pixels_match", memcmp(rgba32, rgba64, sizeof(rgba32)) == 0);
}

static void test_priority_and_invalid_nonmutation(void) {
    uint8_t rgba[4] = {1u, 2u, 3u, 4u};
    const uint8_t before[4] = {1u, 2u, 3u, 4u};
    size_t count = 99u;
    const KitViewport3dColor selected = kit_viewport3d_outline_color(NULL, 2, 2, 2);
    KitViewport3dOutlineParams invalid = {0};
    invalid.rgba = rgba;
    expect("selected_precedes_hover",
           selected.r == 255u && selected.g == 168u && selected.b == 76u);
    expect("invalid_rejected", !kit_viewport3d_apply_outline(&invalid, &count));
    expect("invalid_count_cleared", count == 0u);
    expect("invalid_rgba_nonmutation", memcmp(rgba, before, sizeof(rgba)) == 0);

    {
        KitViewport3dOutlinePalette invalid_palette = {0};
        const KitViewport3dColor fallback =
            kit_viewport3d_outline_color(&invalid_palette, 0, -1, -1);
        expect("invalid_palette_uses_default",
               fallback.r == 76u && fallback.g == 176u && fallback.b == 232u &&
                   fallback.a == 250u);
    }
}

int main(void) {
    test_f32_and_f64_match();
    test_priority_and_invalid_nonmutation();
    if (failures != 0) return 1;
    puts("kit_viewport3d tests passed");
    return 0;
}
