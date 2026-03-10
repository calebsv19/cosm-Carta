#ifndef KIT_PANE_H
#define KIT_PANE_H

#include <stdint.h>

#include "kit_render.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum KitPaneVisualState {
    KIT_PANE_STATE_NORMAL = 0,
    KIT_PANE_STATE_HOVERED = 1,
    KIT_PANE_STATE_ACTIVE = 2,
    KIT_PANE_STATE_FOCUSED = 3,
    KIT_PANE_STATE_DISABLED = 4
} KitPaneVisualState;

typedef struct KitPaneStyle {
    float border_thickness;
    float header_height;
    float corner_radius;
    float title_padding;
    float splitter_thickness;
} KitPaneStyle;

typedef struct KitPaneChrome {
    uint32_t pane_id;
    const char *title;
    KitRenderRect bounds;
    KitPaneVisualState state;
    int show_header;
    int show_id;
    int authoring_selected;
} KitPaneChrome;

void kit_pane_style_default(KitPaneStyle *out_style);

CoreResult kit_pane_draw_chrome(KitRenderContext *render_ctx,
                                KitRenderFrame *frame,
                                const KitPaneStyle *style,
                                const KitPaneChrome *chrome);

CoreResult kit_pane_draw_splitter(KitRenderContext *render_ctx,
                                  KitRenderFrame *frame,
                                  KitRenderRect bounds,
                                  int hovered,
                                  int active);

#ifdef __cplusplus
}
#endif

#endif
