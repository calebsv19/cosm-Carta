#ifndef KIT_PANE_H
#define KIT_PANE_H

#include <stdint.h>

#include "core_pane.h"
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

typedef struct KitPaneSplitterInteraction {
    float handle_thickness;
    int hover_active;
    int drag_active;
    CorePaneSplitterHit hover_hit;
    CorePaneSplitterHit drag_hit;
    float drag_last_x;
    float drag_last_y;
} KitPaneSplitterInteraction;

void kit_pane_style_default(KitPaneStyle *out_style);

void kit_pane_splitter_interaction_init(KitPaneSplitterInteraction *out_interaction,
                                        float handle_thickness);

CoreResult kit_pane_splitter_interaction_set_hover(KitPaneSplitterInteraction *interaction,
                                                   const CorePaneNode *nodes,
                                                   uint32_t node_count,
                                                   uint32_t root_index,
                                                   CorePaneRect bounds,
                                                   float point_x,
                                                   float point_y);

CoreResult kit_pane_splitter_interaction_set_hover_from_hits(KitPaneSplitterInteraction *interaction,
                                                             const CorePaneSplitterHit *hits,
                                                             uint32_t hit_count,
                                                             float point_x,
                                                             float point_y);

CoreResult kit_pane_splitter_interaction_begin_drag(KitPaneSplitterInteraction *interaction,
                                                    const CorePaneNode *nodes,
                                                    uint32_t node_count,
                                                    uint32_t root_index,
                                                    CorePaneRect bounds,
                                                    float point_x,
                                                    float point_y);

CoreResult kit_pane_splitter_interaction_begin_drag_from_hits(KitPaneSplitterInteraction *interaction,
                                                              const CorePaneSplitterHit *hits,
                                                              uint32_t hit_count,
                                                              float point_x,
                                                              float point_y);

CoreResult kit_pane_splitter_interaction_update_drag(KitPaneSplitterInteraction *interaction,
                                                     CorePaneNode *nodes,
                                                     uint32_t node_count,
                                                     float point_x,
                                                     float point_y,
                                                     int *out_changed);

void kit_pane_splitter_interaction_end_drag(KitPaneSplitterInteraction *interaction);

int kit_pane_splitter_interaction_current(const KitPaneSplitterInteraction *interaction,
                                          CorePaneRect *out_bounds,
                                          int *out_hovered,
                                          int *out_active);

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
