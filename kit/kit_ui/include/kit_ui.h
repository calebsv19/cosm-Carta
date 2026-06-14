#ifndef KIT_UI_H
#define KIT_UI_H

#include "kit_render.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KIT_UI_CLIP_STACK_MAX 16
#define KIT_UI_HUD_BUTTON_ROW_MAX 16

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

typedef enum KitUiButtonVariant {
    KIT_UI_BUTTON_VARIANT_DEFAULT = 0,
    KIT_UI_BUTTON_VARIANT_PRIMARY = 1,
    KIT_UI_BUTTON_VARIANT_POSITIVE = 2
} KitUiButtonVariant;

typedef enum KitUiButtonAppearancePreset {
    KIT_UI_BUTTON_APPEARANCE_DEFAULT = 0,
    KIT_UI_BUTTON_APPEARANCE_ROUNDED = 1,
    KIT_UI_BUTTON_APPEARANCE_COMPACT_ROUNDED = 2,
    KIT_UI_BUTTON_APPEARANCE_PILL = 3
} KitUiButtonAppearancePreset;

typedef struct KitUiButtonState {
    int hovered;
    int selected;
    int pressed;
    int disabled;
    int focused;
} KitUiButtonState;

typedef struct KitUiButtonSpec {
    const char *label;
    KitUiButtonVariant variant;
    KitUiButtonState state;
} KitUiButtonSpec;

typedef struct KitUiButtonStyle {
    CoreThemeColor fill;
    CoreThemeColor outline;
    CoreThemeColor text;
} KitUiButtonStyle;

typedef struct KitUiButtonLayout {
    float text_offset_x;
    float text_offset_y;
} KitUiButtonLayout;

typedef struct KitUiButtonAppearance {
    float corner_radius;
    float border_thickness;
    float padding_x;
    float padding_y;
} KitUiButtonAppearance;

typedef struct KitUiHudStyle {
    KitRenderColor panel_fill;
    KitRenderColor button_fill;
    KitRenderColor button_active_fill;
    KitRenderColor button_disabled_fill;
    KitRenderColor readout_fill;
    KitRenderColor text;
    KitRenderColor text_disabled;
    float panel_corner_radius;
    float button_corner_radius;
} KitUiHudStyle;

typedef struct KitUiHudButtonRowConfig {
    float viewport_width;
    float viewport_height;
    float max_width;
    float pad;
    float gap;
    float button_height;
    float bottom_margin;
    float min_top_y;
    float readout_min_width;
    float readout_max_width;
    uint32_t button_count;
    float button_widths[KIT_UI_HUD_BUTTON_ROW_MAX];
} KitUiHudButtonRowConfig;

typedef struct KitUiHudButtonRowLayout {
    KitRenderRect panel_rect;
    KitRenderRect button_rects[KIT_UI_HUD_BUTTON_ROW_MAX];
    KitRenderRect readout_rect;
    uint32_t button_count;
    int has_readout;
} KitUiHudButtonRowLayout;

typedef struct KitUiButtonTheme {
    CoreThemeColor idle_fill;
    CoreThemeColor selected_fill;
    CoreThemeColor hover_fill;
    CoreThemeColor positive_fill;
    CoreThemeColor outline_idle;
    CoreThemeColor outline_highlight;
    CoreThemeColor text_primary;
    CoreThemeColor text_muted;
} KitUiButtonTheme;

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

void kit_ui_button_state_init(KitUiButtonState *state);
void kit_ui_button_spec_init(KitUiButtonSpec *spec, const char *label);
void kit_ui_button_layout_init(KitUiButtonLayout *layout,
                               float text_offset_x,
                               float text_offset_y);
void kit_ui_button_appearance_init(KitUiButtonAppearance *appearance);
int kit_ui_button_appearance_preset(KitUiButtonAppearancePreset preset,
                                    KitUiButtonAppearance *out_appearance);
void kit_ui_button_layout_from_appearance(const KitUiButtonAppearance *appearance,
                                          KitUiButtonLayout *out_layout);
KitRenderColor kit_ui_color_with_alpha(KitRenderColor color, uint8_t alpha);
float kit_ui_corner_radius_clamp(float radius, float width, float height);
float kit_ui_corner_radius_for_inset(float outer_radius, float inset);
KitRenderRect kit_ui_rect_inset(KitRenderRect rect, float inset);
void kit_ui_hud_style_scale(KitUiHudStyle *style, float scale);
void kit_ui_hud_style_dark_floating(KitUiHudStyle *out_style);
void kit_ui_hud_button_row_config_init(KitUiHudButtonRowConfig *config);
float kit_ui_hud_button_row_control_corner_radius(const KitUiHudStyle *style,
                                                  const KitUiHudButtonRowConfig *config);
int kit_ui_hud_button_row_layout(const KitUiHudButtonRowConfig *config,
                                 KitUiHudButtonRowLayout *out_layout);
void kit_ui_button_text_origin(const KitUiButtonLayout *layout,
                               float rect_x,
                               float rect_y,
                               float *out_x,
                               float *out_y);
void kit_ui_button_text_origin_i(const KitUiButtonLayout *layout,
                                 int rect_x,
                                 int rect_y,
                                 int *out_x,
                                 int *out_y);
int kit_ui_button_theme_from_preset(const CoreThemePreset *preset,
                                    KitUiButtonTheme *out_theme);
int kit_ui_button_style_resolve(const KitUiButtonTheme *theme,
                                const KitUiButtonSpec *spec,
                                KitUiButtonStyle *out_style);
int kit_ui_button_style_resolve_preset(const CoreThemePreset *preset,
                                       const KitUiButtonSpec *spec,
                                       KitUiButtonStyle *out_style);

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
CoreResult kit_ui_draw_button_spec(KitUiContext *ctx,
                                   KitRenderFrame *frame,
                                   KitRenderRect bounds,
                                   const KitUiButtonSpec *spec,
                                   const KitUiButtonLayout *layout);
CoreResult kit_ui_draw_button_spec_custom(KitUiContext *ctx,
                                          KitRenderFrame *frame,
                                          KitRenderRect bounds,
                                          const KitUiButtonSpec *spec,
                                          const KitUiButtonLayout *layout,
                                          CoreFontRoleId font_role,
                                          CoreFontTextSizeTier text_tier);
CoreResult kit_ui_draw_button_spec_appearance(KitUiContext *ctx,
                                              KitRenderFrame *frame,
                                              KitRenderRect bounds,
                                              const KitUiButtonSpec *spec,
                                              const KitUiButtonLayout *layout,
                                              const KitUiButtonAppearance *appearance);
CoreResult kit_ui_draw_button_spec_appearance_custom(KitUiContext *ctx,
                                                     KitRenderFrame *frame,
                                                     KitRenderRect bounds,
                                                     const KitUiButtonSpec *spec,
                                                     const KitUiButtonLayout *layout,
                                                     const KitUiButtonAppearance *appearance,
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
