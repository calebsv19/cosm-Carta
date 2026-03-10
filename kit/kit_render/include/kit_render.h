#ifndef KIT_RENDER_H
#define KIT_RENDER_H

#include <stddef.h>
#include <stdint.h>

#include "core_base.h"
#include "core_font.h"
#include "core_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*KitRenderExternalReleaseFn)(void *backend_handle, void *user);

typedef enum KitRenderBackendKind {
    KIT_RENDER_BACKEND_NULL = 0,
    KIT_RENDER_BACKEND_VULKAN = 1
} KitRenderBackendKind;

typedef enum KitRenderCommandKind {
    KIT_RENDER_CMD_CLEAR = 0,
    KIT_RENDER_CMD_SET_CLIP = 1,
    KIT_RENDER_CMD_CLEAR_CLIP = 2,
    KIT_RENDER_CMD_RECT = 3,
    KIT_RENDER_CMD_LINE = 4,
    KIT_RENDER_CMD_POLYLINE = 5,
    KIT_RENDER_CMD_TEXTURED_QUAD = 6,
    KIT_RENDER_CMD_TEXT = 7
} KitRenderCommandKind;

typedef struct KitRenderVec2 {
    float x;
    float y;
} KitRenderVec2;

typedef struct KitRenderRect {
    float x;
    float y;
    float width;
    float height;
} KitRenderRect;

typedef struct KitRenderColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} KitRenderColor;

typedef struct KitRenderTransform {
    float tx;
    float ty;
    float sx;
    float sy;
} KitRenderTransform;

typedef struct KitRenderClearCommand {
    KitRenderColor color;
} KitRenderClearCommand;

typedef struct KitRenderClipCommand {
    KitRenderRect rect;
} KitRenderClipCommand;

typedef struct KitRenderRectCommand {
    KitRenderRect rect;
    float corner_radius;
    KitRenderColor color;
    KitRenderTransform transform;
} KitRenderRectCommand;

typedef struct KitRenderLineCommand {
    KitRenderVec2 p0;
    KitRenderVec2 p1;
    float thickness;
    KitRenderColor color;
    KitRenderTransform transform;
} KitRenderLineCommand;

typedef struct KitRenderPolylineCommand {
    const KitRenderVec2 *points;
    uint32_t point_count;
    float thickness;
    KitRenderColor color;
    KitRenderTransform transform;
} KitRenderPolylineCommand;

typedef struct KitRenderTexturedQuadCommand {
    KitRenderRect rect;
    uint64_t texture_id;
    KitRenderVec2 uv_min;
    KitRenderVec2 uv_max;
    KitRenderColor tint;
    KitRenderTransform transform;
} KitRenderTexturedQuadCommand;

typedef struct KitRenderTextCommand {
    KitRenderVec2 origin;
    const char *text;
    CoreFontRoleId font_role;
    CoreFontTextSizeTier text_tier;
    CoreThemeColorToken color_token;
    KitRenderTransform transform;
} KitRenderTextCommand;

typedef struct KitRenderTextMetrics {
    float width_px;
    float height_px;
} KitRenderTextMetrics;

typedef struct KitRenderCommand {
    KitRenderCommandKind kind;
    union {
        KitRenderClearCommand clear;
        KitRenderClipCommand clip;
        KitRenderRectCommand rect;
        KitRenderLineCommand line;
        KitRenderPolylineCommand polyline;
        KitRenderTexturedQuadCommand textured_quad;
        KitRenderTextCommand text;
    } data;
} KitRenderCommand;

typedef struct KitRenderCommandBuffer {
    KitRenderCommand *commands;
    size_t capacity;
    size_t count;
} KitRenderCommandBuffer;

typedef struct KitRenderFrame {
    uint32_t width_px;
    uint32_t height_px;
    KitRenderCommandBuffer *command_buffer;
} KitRenderFrame;

typedef struct KitRenderContext {
    KitRenderBackendKind backend;
    CoreThemePreset theme;
    CoreFontPreset font;
    void *backend_state;
    const void *backend_ops;
    int frame_open;
} KitRenderContext;

KitRenderTransform kit_render_identity_transform(void);

CoreResult kit_render_context_init(KitRenderContext *ctx,
                                   KitRenderBackendKind backend,
                                   CoreThemePresetId theme_id,
                                   CoreFontPresetId font_id);
CoreResult kit_render_set_theme_preset(KitRenderContext *ctx, CoreThemePresetId theme_id);
CoreResult kit_render_set_font_preset(KitRenderContext *ctx, CoreFontPresetId font_id);
CoreResult kit_render_attach_external_backend(KitRenderContext *ctx, void *backend_handle);
CoreResult kit_render_adopt_external_backend(KitRenderContext *ctx,
                                             void *backend_handle,
                                             KitRenderExternalReleaseFn release_fn,
                                             void *release_user);
void kit_render_context_shutdown(KitRenderContext *ctx);

CoreResult kit_render_begin_frame(KitRenderContext *ctx,
                                  uint32_t width_px,
                                  uint32_t height_px,
                                  KitRenderCommandBuffer *command_buffer,
                                  KitRenderFrame *out_frame);

CoreResult kit_render_end_frame(KitRenderContext *ctx, KitRenderFrame *frame);

CoreResult kit_render_push_clear(KitRenderFrame *frame, KitRenderColor color);
CoreResult kit_render_push_set_clip(KitRenderFrame *frame, KitRenderRect rect);
CoreResult kit_render_push_clear_clip(KitRenderFrame *frame);
CoreResult kit_render_push_rect(KitRenderFrame *frame, const KitRenderRectCommand *cmd);
CoreResult kit_render_push_line(KitRenderFrame *frame, const KitRenderLineCommand *cmd);
CoreResult kit_render_push_polyline(KitRenderFrame *frame, const KitRenderPolylineCommand *cmd);
CoreResult kit_render_push_textured_quad(KitRenderFrame *frame,
                                         const KitRenderTexturedQuadCommand *cmd);
CoreResult kit_render_push_text(KitRenderFrame *frame, const KitRenderTextCommand *cmd);
CoreResult kit_render_measure_text(const KitRenderContext *ctx,
                                   CoreFontRoleId font_role,
                                   CoreFontTextSizeTier text_tier,
                                   const char *text,
                                   KitRenderTextMetrics *out_metrics);

CoreResult kit_render_resolve_theme_color(const KitRenderContext *ctx,
                                          CoreThemeColorToken token,
                                          KitRenderColor *out_color);

#ifdef __cplusplus
}
#endif

#endif
