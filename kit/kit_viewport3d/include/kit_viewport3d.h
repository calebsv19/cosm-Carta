#ifndef KIT_VIEWPORT3D_H
#define KIT_VIEWPORT3D_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KIT_VIEWPORT3D_OBJECT_ACCENT_CAP 6u

typedef struct KitViewport3dColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} KitViewport3dColor;

typedef struct KitViewport3dOutlinePalette {
    KitViewport3dColor object_accents[KIT_VIEWPORT3D_OBJECT_ACCENT_CAP];
    size_t object_accent_count;
    KitViewport3dColor selected;
    KitViewport3dColor hover;
} KitViewport3dOutlinePalette;

typedef enum KitViewport3dDepthFormat {
    KIT_VIEWPORT3D_DEPTH_F32 = 0,
    KIT_VIEWPORT3D_DEPTH_F64 = 1
} KitViewport3dDepthFormat;

typedef struct KitViewport3dOutlineParams {
    uint8_t* rgba;
    const void* depth;
    const int32_t* owner;
    int width;
    int height;
    KitViewport3dDepthFormat depth_format;
    double relative_depth_threshold;
    int32_t selected_owner;
    int32_t hover_owner;
    bool outline_only;
    const KitViewport3dOutlinePalette* palette;
} KitViewport3dOutlineParams;

KitViewport3dOutlinePalette kit_viewport3d_outline_palette_default(void);

KitViewport3dColor kit_viewport3d_outline_color(
    const KitViewport3dOutlinePalette* palette,
    int32_t owner,
    int32_t selected_owner,
    int32_t hover_owner);

bool kit_viewport3d_apply_outline(const KitViewport3dOutlineParams* params,
                                  size_t* out_outline_pixels);

#endif
