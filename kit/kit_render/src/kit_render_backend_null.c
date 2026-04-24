#include "kit_render_backend.h"

#include <string.h>

static int null_backend_estimate_char_width(CoreFontTextSizeTier text_tier) {
    switch (text_tier) {
        case CORE_FONT_TEXT_SIZE_HEADER:
            return 13;
        case CORE_FONT_TEXT_SIZE_TITLE:
            return 11;
        case CORE_FONT_TEXT_SIZE_PARAGRAPH:
            return 9;
        case CORE_FONT_TEXT_SIZE_CAPTION:
            return 8;
        case CORE_FONT_TEXT_SIZE_BASIC:
        default:
            return 9;
    }
}

static int null_backend_estimate_line_height(CoreFontTextSizeTier text_tier) {
    switch (text_tier) {
        case CORE_FONT_TEXT_SIZE_HEADER:
            return 28;
        case CORE_FONT_TEXT_SIZE_TITLE:
            return 22;
        case CORE_FONT_TEXT_SIZE_PARAGRAPH:
            return 18;
        case CORE_FONT_TEXT_SIZE_CAPTION:
            return 16;
        case CORE_FONT_TEXT_SIZE_BASIC:
        default:
            return 18;
    }
}

static CoreResult null_backend_invalid(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static CoreResult null_backend_init(KitRenderContext *ctx) {
    if (!ctx) {
        return null_backend_invalid("null context");
    }
    ctx->backend_state = 0;
    return core_result_ok();
}

static void null_backend_shutdown(KitRenderContext *ctx) {
    (void)ctx;
}

static CoreResult null_backend_attach_external(KitRenderContext *ctx,
                                               void *backend_handle,
                                               int take_ownership,
                                               KitRenderExternalReleaseFn release_fn,
                                               void *release_user) {
    (void)ctx;
    (void)backend_handle;
    (void)take_ownership;
    (void)release_fn;
    (void)release_user;
    return null_backend_invalid("null backend does not accept external backend handles");
}

static CoreResult null_backend_begin_frame(KitRenderContext *ctx, KitRenderFrame *frame) {
    (void)ctx;
    if (!frame || !frame->command_buffer) {
        return null_backend_invalid("invalid frame");
    }
    return core_result_ok();
}

static CoreResult null_backend_submit(KitRenderContext *ctx, KitRenderFrame *frame) {
    (void)ctx;
    if (!frame || !frame->command_buffer) {
        return null_backend_invalid("invalid frame");
    }
    return core_result_ok();
}

static CoreResult null_backend_end_frame(KitRenderContext *ctx, KitRenderFrame *frame) {
    (void)ctx;
    if (!frame || !frame->command_buffer) {
        return null_backend_invalid("invalid frame");
    }
    return core_result_ok();
}

static CoreResult null_backend_wait_idle(KitRenderContext *ctx) {
    (void)ctx;
    return core_result_ok();
}

static CoreResult null_backend_measure_text(const KitRenderContext *ctx,
                                            CoreFontRoleId font_role,
                                            CoreFontTextSizeTier text_tier,
                                            const char *text,
                                            KitRenderTextMetrics *out_metrics) {
    size_t len;
    int char_width;
    int line_height;

    (void)ctx;
    (void)font_role;

    if (!text || !out_metrics) {
        return null_backend_invalid("invalid text metrics request");
    }

    len = strlen(text);
    char_width = null_backend_estimate_char_width(text_tier);
    line_height = null_backend_estimate_line_height(text_tier);
    if (char_width < 1) {
        char_width = 8;
    }
    if (line_height < 1) {
        line_height = 16;
    }

    {
        float pct = (float)kit_render_text_zoom_percent(ctx) / 100.0f;
        out_metrics->width_px = (float)((int)len * char_width) * pct;
        out_metrics->height_px = (float)line_height * pct;
    }
    return core_result_ok();
}

const KitRenderBackendOps *kit_render_backend_null_ops(void) {
    static const KitRenderBackendOps ops = {
        null_backend_init,
        null_backend_shutdown,
        null_backend_attach_external,
        null_backend_begin_frame,
        null_backend_submit,
        null_backend_end_frame,
        null_backend_wait_idle,
        null_backend_measure_text
    };
    return &ops;
}
