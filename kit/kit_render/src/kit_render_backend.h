#ifndef KIT_RENDER_BACKEND_H
#define KIT_RENDER_BACKEND_H

#include "kit_render.h"

typedef struct KitRenderBackendOps {
    CoreResult (*init)(KitRenderContext *ctx);
    void (*shutdown)(KitRenderContext *ctx);
    CoreResult (*attach_external)(KitRenderContext *ctx,
                                  void *backend_handle,
                                  int take_ownership,
                                  KitRenderExternalReleaseFn release_fn,
                                  void *release_user);
    CoreResult (*begin_frame)(KitRenderContext *ctx, KitRenderFrame *frame);
    CoreResult (*submit)(KitRenderContext *ctx, KitRenderFrame *frame);
    CoreResult (*end_frame)(KitRenderContext *ctx, KitRenderFrame *frame);
    CoreResult (*wait_idle)(KitRenderContext *ctx);
    CoreResult (*measure_text)(const KitRenderContext *ctx,
                               CoreFontRoleId font_role,
                               CoreFontTextSizeTier text_tier,
                               const char *text,
                               KitRenderTextMetrics *out_metrics);
} KitRenderBackendOps;

const KitRenderBackendOps *kit_render_backend_null_ops(void);
const KitRenderBackendOps *kit_render_backend_vk_ops(void);

#endif
