/*
 * kit_render.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "kit_render.h"
#include "kit_render_backend.h"

#include <string.h>

static CoreResult kit_render_invalid(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static const KitRenderBackendOps *kit_render_select_backend_ops(KitRenderBackendKind backend) {
    switch (backend) {
        case KIT_RENDER_BACKEND_VULKAN:
            return kit_render_backend_vk_ops();
        case KIT_RENDER_BACKEND_NULL:
        default:
            return kit_render_backend_null_ops();
    }
}

static const KitRenderBackendOps *kit_render_ops(const KitRenderContext *ctx) {
    if (!ctx || !ctx->backend_ops) {
        return 0;
    }
    return (const KitRenderBackendOps *)ctx->backend_ops;
}

KitRenderTransform kit_render_identity_transform(void) {
    KitRenderTransform t;
    t.tx = 0.0f;
    t.ty = 0.0f;
    t.sx = 1.0f;
    t.sy = 1.0f;
    return t;
}

CoreResult kit_render_context_init(KitRenderContext *ctx,
                                   KitRenderBackendKind backend,
                                   CoreThemePresetId theme_id,
                                   CoreFontPresetId font_id) {
    CoreResult result;
    const KitRenderBackendOps *ops;

    if (!ctx) {
        return kit_render_invalid("null context");
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->backend = backend;
    ops = kit_render_select_backend_ops(backend);
    if (!ops) {
        return kit_render_invalid("unsupported backend");
    }
    ctx->backend_ops = ops;

    result = core_theme_get_preset(theme_id, &ctx->theme);
    if (result.code != CORE_OK) {
        return result;
    }

    result = core_font_get_preset(font_id, &ctx->font);
    if (result.code != CORE_OK) {
        return result;
    }

    ctx->frame_open = 0;
    return ops->init(ctx);
}

CoreResult kit_render_set_theme_preset(KitRenderContext *ctx, CoreThemePresetId theme_id) {
    if (!ctx) {
        return kit_render_invalid("null context");
    }
    if (ctx->frame_open) {
        return kit_render_invalid("cannot change theme during frame");
    }
    return core_theme_get_preset(theme_id, &ctx->theme);
}

CoreResult kit_render_set_font_preset(KitRenderContext *ctx, CoreFontPresetId font_id) {
    if (!ctx) {
        return kit_render_invalid("null context");
    }
    if (ctx->frame_open) {
        return kit_render_invalid("cannot change font during frame");
    }
    return core_font_get_preset(font_id, &ctx->font);
}

CoreResult kit_render_attach_external_backend(KitRenderContext *ctx, void *backend_handle) {
    return kit_render_adopt_external_backend(ctx, backend_handle, 0, 0);
}

CoreResult kit_render_adopt_external_backend(KitRenderContext *ctx,
                                             void *backend_handle,
                                             KitRenderExternalReleaseFn release_fn,
                                             void *release_user) {
    const KitRenderBackendOps *ops = kit_render_ops(ctx);

    if (!ctx || !backend_handle) {
        return kit_render_invalid("invalid argument");
    }
    if (!ops || !ops->attach_external) {
        return kit_render_invalid("backend does not support external attachment");
    }
    return ops->attach_external(ctx, backend_handle, release_fn ? 1 : 0, release_fn, release_user);
}

void kit_render_context_shutdown(KitRenderContext *ctx) {
    const KitRenderBackendOps *ops;

    if (!ctx) {
        return;
    }

    ops = kit_render_ops(ctx);
    if (ops && ops->wait_idle) {
        (void)ops->wait_idle(ctx);
    }
    if (ops && ops->shutdown) {
        ops->shutdown(ctx);
    }

    ctx->backend_state = 0;
    ctx->backend_ops = 0;
    ctx->frame_open = 0;
}

static CoreResult kit_render_append(KitRenderFrame *frame, const KitRenderCommand *cmd) {
    KitRenderCommandBuffer *buffer;

    if (!frame || !cmd || !frame->command_buffer) {
        return kit_render_invalid("null frame");
    }

    buffer = frame->command_buffer;
    if (!buffer->commands || buffer->capacity == 0) {
        return kit_render_invalid("invalid command buffer");
    }
    if (buffer->count >= buffer->capacity) {
        return kit_render_invalid("command buffer full");
    }

    buffer->commands[buffer->count++] = *cmd;
    return core_result_ok();
}

CoreResult kit_render_begin_frame(KitRenderContext *ctx,
                                  uint32_t width_px,
                                  uint32_t height_px,
                                  KitRenderCommandBuffer *command_buffer,
                                  KitRenderFrame *out_frame) {
    const KitRenderBackendOps *ops;
    CoreResult result;

    if (!ctx || !command_buffer || !out_frame) {
        return kit_render_invalid("invalid argument");
    }
    if (width_px == 0 || height_px == 0) {
        return kit_render_invalid("invalid frame size");
    }
    if (ctx->frame_open) {
        return kit_render_invalid("frame already open");
    }
    if (!command_buffer->commands || command_buffer->capacity == 0) {
        return kit_render_invalid("invalid command buffer");
    }

    command_buffer->count = 0;

    out_frame->width_px = width_px;
    out_frame->height_px = height_px;
    out_frame->command_buffer = command_buffer;

    ops = kit_render_ops(ctx);
    if (!ops) {
        return kit_render_invalid("backend not initialized");
    }
    result = ops->begin_frame(ctx, out_frame);
    if (result.code != CORE_OK) {
        return result;
    }

    ctx->frame_open = 1;
    return core_result_ok();
}

CoreResult kit_render_end_frame(KitRenderContext *ctx, KitRenderFrame *frame) {
    const KitRenderBackendOps *ops;
    CoreResult result;

    if (!ctx || !frame || !frame->command_buffer) {
        return kit_render_invalid("invalid argument");
    }
    if (!ctx->frame_open) {
        return kit_render_invalid("no frame open");
    }

    ops = kit_render_ops(ctx);
    if (!ops) {
        return kit_render_invalid("backend not initialized");
    }

    result = ops->submit(ctx, frame);
    if (result.code != CORE_OK) {
        return result;
    }

    result = ops->end_frame(ctx, frame);
    if (result.code != CORE_OK) {
        return result;
    }

    ctx->frame_open = 0;
    return core_result_ok();
}

CoreResult kit_render_push_clear(KitRenderFrame *frame, KitRenderColor color) {
    KitRenderCommand cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = KIT_RENDER_CMD_CLEAR;
    cmd.data.clear.color = color;
    return kit_render_append(frame, &cmd);
}

CoreResult kit_render_push_set_clip(KitRenderFrame *frame, KitRenderRect rect) {
    KitRenderCommand cmd;

    if (rect.width < 0.0f || rect.height < 0.0f) {
        return kit_render_invalid("invalid clip rect");
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = KIT_RENDER_CMD_SET_CLIP;
    cmd.data.clip.rect = rect;
    return kit_render_append(frame, &cmd);
}

CoreResult kit_render_push_clear_clip(KitRenderFrame *frame) {
    KitRenderCommand cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = KIT_RENDER_CMD_CLEAR_CLIP;
    return kit_render_append(frame, &cmd);
}

CoreResult kit_render_push_rect(KitRenderFrame *frame, const KitRenderRectCommand *rect_cmd) {
    KitRenderCommand cmd;

    if (!rect_cmd || rect_cmd->rect.width < 0.0f || rect_cmd->rect.height < 0.0f ||
        rect_cmd->corner_radius < 0.0f) {
        return kit_render_invalid("invalid rect command");
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = KIT_RENDER_CMD_RECT;
    cmd.data.rect = *rect_cmd;
    return kit_render_append(frame, &cmd);
}

CoreResult kit_render_push_line(KitRenderFrame *frame, const KitRenderLineCommand *line_cmd) {
    KitRenderCommand cmd;

    if (!line_cmd || line_cmd->thickness <= 0.0f) {
        return kit_render_invalid("invalid line command");
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = KIT_RENDER_CMD_LINE;
    cmd.data.line = *line_cmd;
    return kit_render_append(frame, &cmd);
}

CoreResult kit_render_push_polyline(KitRenderFrame *frame, const KitRenderPolylineCommand *line_cmd) {
    KitRenderCommand cmd;

    if (!line_cmd || !line_cmd->points || line_cmd->point_count < 2 || line_cmd->thickness <= 0.0f) {
        return kit_render_invalid("invalid polyline command");
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = KIT_RENDER_CMD_POLYLINE;
    cmd.data.polyline = *line_cmd;
    return kit_render_append(frame, &cmd);
}

CoreResult kit_render_push_textured_quad(KitRenderFrame *frame,
                                         const KitRenderTexturedQuadCommand *quad_cmd) {
    KitRenderCommand cmd;

    if (!quad_cmd || quad_cmd->rect.width < 0.0f || quad_cmd->rect.height < 0.0f) {
        return kit_render_invalid("invalid textured quad command");
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = KIT_RENDER_CMD_TEXTURED_QUAD;
    cmd.data.textured_quad = *quad_cmd;
    return kit_render_append(frame, &cmd);
}

CoreResult kit_render_push_text(KitRenderFrame *frame, const KitRenderTextCommand *text_cmd) {
    KitRenderCommand cmd;

    if (!text_cmd || !text_cmd->text || text_cmd->text[0] == '\0') {
        return kit_render_invalid("invalid text command");
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = KIT_RENDER_CMD_TEXT;
    cmd.data.text = *text_cmd;
    return kit_render_append(frame, &cmd);
}

CoreResult kit_render_measure_text(const KitRenderContext *ctx,
                                   CoreFontRoleId font_role,
                                   CoreFontTextSizeTier text_tier,
                                   const char *text,
                                   KitRenderTextMetrics *out_metrics) {
    const KitRenderBackendOps *ops;

    if (!ctx || !text || !out_metrics) {
        return kit_render_invalid("invalid argument");
    }

    ops = kit_render_ops(ctx);
    if (!ops || !ops->measure_text) {
        return kit_render_invalid("backend does not support text metrics");
    }

    return ops->measure_text(ctx, font_role, text_tier, text, out_metrics);
}

CoreResult kit_render_resolve_theme_color(const KitRenderContext *ctx,
                                          CoreThemeColorToken token,
                                          KitRenderColor *out_color) {
    CoreThemeColor color;
    CoreResult result;

    if (!ctx || !out_color) {
        return kit_render_invalid("invalid argument");
    }

    result = core_theme_get_color(&ctx->theme, token, &color);
    if (result.code != CORE_OK) {
        return result;
    }

    out_color->r = color.r;
    out_color->g = color.g;
    out_color->b = color.b;
    out_color->a = color.a;
    return core_result_ok();
}
