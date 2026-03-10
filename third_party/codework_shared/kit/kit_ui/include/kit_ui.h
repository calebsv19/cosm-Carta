#ifndef KIT_UI_H
#define KIT_UI_H

#include "kit_render.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KIT_UI_CLIP_STACK_MAX 16

typedef enum KitUiAxis {
    KIT_UI_AXIS_VERTICAL = 0,
    KIT_UI_AXIS_HORIZONTAL = 1
} KitUiAxis;

typedef enum KitUiWidgetState {
    KIT_UI_STATE_NORMAL = 0,
    KIT_UI_STATE_HOVERED = 1,
    KIT_UI_STATE_ACTIVE = 2,
    KIT_UI_STATE_DISABLED = 3
} KitUiWidgetState;

typedef struct KitUiStyle {
    float padding;
    float gap;
    float control_height;
    float checkbox_size;
    float slider_track_height;
    float slider_knob_width;
} KitUiStyle;

typedef struct KitUiContext {
    KitRenderContext *render_ctx;
    KitUiStyle style;
    KitRenderRect clip_stack[KIT_UI_CLIP_STACK_MAX];
    int clip_depth;
} KitUiContext;

typedef struct KitUiStackLayout {
    KitUiAxis axis;
    KitRenderRect bounds;
    float cursor;
    float gap;
} KitUiStackLayout;

typedef struct KitUiInputState {
    float mouse_x;
    float mouse_y;
    int mouse_down;
    int mouse_pressed;
    int mouse_released;
} KitUiInputState;

typedef struct KitUiButtonResult {
    KitUiWidgetState state;
    int clicked;
} KitUiButtonResult;

typedef struct KitUiCheckboxResult {
    KitUiWidgetState state;
    int toggled;
    int checked;
} KitUiCheckboxResult;

typedef struct KitUiSliderResult {
    KitUiWidgetState state;
    int changed;
    float value_01;
} KitUiSliderResult;

typedef struct KitUiScrollResult {
    int changed;
    float offset_y;
} KitUiScrollResult;

typedef struct KitUiSegmentedResult {
    int changed;
    int selected_index;
    int clicked_index;
} KitUiSegmentedResult;

typedef struct KitUiTextFitResult {
    CoreFontTextSizeTier text_tier;
    float width_px;
    float height_px;
    int truncated;
} KitUiTextFitResult;

void kit_ui_style_default(KitUiStyle *out_style);
CoreResult kit_ui_style_apply_theme_scale(KitUiContext *ctx);
CoreResult kit_ui_context_init(KitUiContext *ctx, KitRenderContext *render_ctx);

CoreResult kit_ui_stack_begin(KitUiStackLayout *layout,
                              KitUiAxis axis,
                              KitRenderRect bounds,
                              float gap);
CoreResult kit_ui_stack_next(KitUiStackLayout *layout,
                             float span,
                             float cross_span,
                             KitRenderRect *out_rect);

void kit_ui_clip_stack_reset(KitUiContext *ctx);
CoreResult kit_ui_clip_push(KitUiContext *ctx,
                            KitRenderFrame *frame,
                            KitRenderRect clip_rect);
CoreResult kit_ui_clip_pop(KitUiContext *ctx, KitRenderFrame *frame);

int kit_ui_point_in_rect(KitRenderRect bounds, float x, float y);
KitUiWidgetState kit_ui_state_for_input(KitRenderRect bounds,
                                        const KitUiInputState *input,
                                        int enabled);
KitUiButtonResult kit_ui_eval_button(KitRenderRect bounds,
                                     const KitUiInputState *input,
                                     int enabled);
KitUiCheckboxResult kit_ui_eval_checkbox(KitRenderRect bounds,
                                         const KitUiInputState *input,
                                         int enabled,
                                         int checked);
KitUiSliderResult kit_ui_eval_slider(KitRenderRect bounds,
                                     const KitUiInputState *input,
                                     int enabled,
                                     float current_value_01);
KitUiScrollResult kit_ui_eval_scroll(KitRenderRect viewport_bounds,
                                     float current_offset_y,
                                     float content_height,
                                     float wheel_delta_y);
float kit_ui_scroll_content_height_top_anchor(int row_count,
                                              float row_height,
                                              float viewport_height);
KitUiSegmentedResult kit_ui_eval_segmented(KitRenderRect bounds,
                                           const KitUiInputState *input,
                                           int enabled,
                                           int segment_count,
                                           int selected_index);

CoreResult kit_ui_draw_label(KitUiContext *ctx,
                             KitRenderFrame *frame,
                             KitRenderRect bounds,
                             const char *text,
                             CoreThemeColorToken color_token);
CoreResult kit_ui_draw_label_custom(KitUiContext *ctx,
                                    KitRenderFrame *frame,
                                    KitRenderRect bounds,
                                    const char *text,
                                    CoreThemeColorToken color_token,
                                    CoreFontRoleId font_role,
                                    CoreFontTextSizeTier text_tier);

CoreResult kit_ui_draw_button(KitUiContext *ctx,
                              KitRenderFrame *frame,
                              KitRenderRect bounds,
                              const char *label,
                              KitUiWidgetState state);
CoreResult kit_ui_draw_button_custom(KitUiContext *ctx,
                                     KitRenderFrame *frame,
                                     KitRenderRect bounds,
                                     const char *label,
                                     KitUiWidgetState state,
                                     CoreFontRoleId font_role,
                                     CoreFontTextSizeTier text_tier);

CoreResult kit_ui_draw_checkbox(KitUiContext *ctx,
                                KitRenderFrame *frame,
                                KitRenderRect bounds,
                                const char *label,
                                int checked,
                                KitUiWidgetState state);

CoreResult kit_ui_draw_slider(KitUiContext *ctx,
                              KitRenderFrame *frame,
                              KitRenderRect bounds,
                              float value_01,
                              KitUiWidgetState state);
CoreResult kit_ui_draw_scrollbar(KitUiContext *ctx,
                                 KitRenderFrame *frame,
                                 KitRenderRect viewport_bounds,
                                 float offset_y,
                                 float content_height);
CoreResult kit_ui_draw_segmented(KitUiContext *ctx,
                                 KitRenderFrame *frame,
                                 KitRenderRect bounds,
                                 const char *const *labels,
                                 int segment_count,
                                 int selected_index,
                                 int hovered_index,
                                 int enabled);

CoreResult kit_ui_measure_text(const KitUiContext *ctx,
                               CoreFontRoleId font_role,
                               CoreFontTextSizeTier text_tier,
                               const char *text,
                               KitRenderTextMetrics *out_metrics);
CoreResult kit_ui_fit_text_to_rect(const KitUiContext *ctx,
                                   const char *source_text,
                                   CoreFontRoleId font_role,
                                   const CoreFontTextSizeTier *tier_candidates,
                                   uint32_t tier_candidate_count,
                                   float max_width_px,
                                   float max_height_px,
                                   char *out_text,
                                   size_t out_text_cap,
                                   KitUiTextFitResult *out_fit);

#ifdef __cplusplus
}
#endif

#endif
