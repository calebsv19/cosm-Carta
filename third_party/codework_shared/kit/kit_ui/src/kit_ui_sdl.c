#include "kit_ui_sdl.h"

#include <math.h>

static int kit_ui_sdl_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int kit_ui_sdl_round_float(float value) {
    if (value >= 0.0f) {
        return (int)(value + 0.5f);
    }
    return (int)(value - 0.5f);
}

SDL_Rect kit_ui_sdl_rect_from_render(KitRenderRect rect) {
    return (SDL_Rect){
        kit_ui_sdl_round_float(rect.x),
        kit_ui_sdl_round_float(rect.y),
        kit_ui_sdl_round_float(rect.width),
        kit_ui_sdl_round_float(rect.height)
    };
}

void kit_ui_sdl_fill_rounded_rect(SDL_Renderer *renderer,
                                  const SDL_Rect *rect,
                                  int radius,
                                  KitRenderColor color) {
    int y = 0;
    int max_radius = 0;
    if (renderer == 0 || rect == 0 || rect->w <= 0 || rect->h <= 0) {
        return;
    }
    max_radius = (rect->w < rect->h ? rect->w : rect->h) / 2;
    radius = kit_ui_sdl_clamp_int(radius, 0, max_radius);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (radius <= 0) {
        SDL_RenderFillRect(renderer, rect);
        return;
    }
    for (y = 0; y < rect->h; ++y) {
        int left_inset = 0;
        int right_inset = 0;
        if (y < radius) {
            int dy = radius - y;
            left_inset = radius - (int)sqrt((double)(radius * radius - dy * dy));
        } else if (y >= rect->h - radius) {
            int dy = y - (rect->h - radius - 1);
            left_inset = radius - (int)sqrt((double)(radius * radius - dy * dy));
        }
        right_inset = left_inset;
        SDL_RenderDrawLine(renderer,
                           rect->x + left_inset,
                           rect->y + y,
                           rect->x + rect->w - right_inset - 1,
                           rect->y + y);
    }
}

static void kit_ui_sdl_draw_text_centered(SDL_Renderer *renderer,
                                          const SDL_Rect *rect,
                                          const char *text,
                                          KitRenderColor color,
                                          const KitUiSdlTextApi *text_api) {
    int text_w = 0;
    int text_h = 0;
    int line_h = 0;
    int x = 0;
    int y = 0;
    int pad_x = 0;
    int scale = 1;
    SDL_Rect clip;

    if (renderer == 0 || rect == 0 || text == 0 || text_api == 0 ||
        text_api->measure_text == 0 ||
        text_api->line_height == 0 ||
        text_api->draw_text_clipped == 0) {
        return;
    }

    scale = text_api->scale > 0 ? text_api->scale : 1;
    pad_x = text_api->clip_padding_x >= 0 ? text_api->clip_padding_x : 4;
    (void)text_api->measure_text(text_api->user, text, scale, &text_w, &text_h);
    line_h = text_api->line_height(text_api->user, scale);
    clip = (SDL_Rect){rect->x + pad_x, rect->y, rect->w - (pad_x * 2), rect->h};
    x = rect->x + (rect->w - text_w) / 2;
    y = rect->y + (rect->h - line_h) / 2;
    if (x < clip.x) {
        x = clip.x;
    }
    text_api->draw_text_clipped(text_api->user, renderer, &clip, x, y, text, scale, color);
}

void kit_ui_sdl_draw_button(SDL_Renderer *renderer,
                            const SDL_Rect *rect,
                            const char *label,
                            const KitUiButtonState *state,
                            const KitUiHudStyle *style,
                            const KitUiSdlTextApi *text_api) {
    KitUiHudStyle default_style;
    KitRenderColor fill;
    KitRenderColor text;
    int selected = 0;
    int disabled = 0;

    if (renderer == 0 || rect == 0 || label == 0) {
        return;
    }
    if (style == 0) {
        kit_ui_hud_style_dark_floating(&default_style);
        style = &default_style;
    }
    selected = state != 0 && (state->selected || state->pressed);
    disabled = state != 0 && state->disabled;
    fill = selected ? style->button_active_fill : style->button_fill;
    text = disabled ? style->text_disabled : style->text;
    if (disabled) {
        fill = style->button_disabled_fill;
    }
    kit_ui_sdl_fill_rounded_rect(renderer,
                                 rect,
                                 kit_ui_sdl_round_float(style->button_corner_radius),
                                 fill);
    kit_ui_sdl_draw_text_centered(renderer, rect, label, text, text_api);
}

void kit_ui_sdl_draw_readout(SDL_Renderer *renderer,
                             const SDL_Rect *rect,
                             const char *text,
                             const KitUiHudStyle *style,
                             const KitUiSdlTextApi *text_api) {
    KitUiHudStyle default_style;
    int scale = 1;
    int line_h = 0;
    int pad_x = 8;
    SDL_Rect clip;

    if (renderer == 0 || rect == 0 || text == 0 || rect->w <= 0 || rect->h <= 0) {
        return;
    }
    if (style == 0) {
        kit_ui_hud_style_dark_floating(&default_style);
        style = &default_style;
    }
    kit_ui_sdl_fill_rounded_rect(renderer,
                                 rect,
                                 kit_ui_sdl_round_float(style->button_corner_radius),
                                 style->readout_fill);
    if (text_api == 0 || text_api->line_height == 0 || text_api->draw_text_clipped == 0) {
        return;
    }
    scale = text_api->scale > 0 ? text_api->scale : 1;
    pad_x = text_api->clip_padding_x >= 0 ? text_api->clip_padding_x : 8;
    line_h = text_api->line_height(text_api->user, scale);
    clip = (SDL_Rect){rect->x + pad_x, rect->y, rect->w - (pad_x * 2), rect->h};
    text_api->draw_text_clipped(text_api->user,
                                renderer,
                                &clip,
                                clip.x,
                                rect->y + (rect->h - line_h) / 2,
                                text,
                                scale,
                                style->text);
}

void kit_ui_sdl_scrollbar_layout(const SDL_Rect *viewport,
                                 int content_height,
                                 int offset_y,
                                 KitUiSdlScrollbarLayout *out_layout) {
    int max_offset = 0;
    int thumb_h = 0;
    int travel = 0;
    if (!out_layout) return;
    *out_layout = (KitUiSdlScrollbarLayout){{0}, {0}, 0};
    if (!viewport || viewport->w <= 0 || viewport->h <= 0 || content_height <= viewport->h) return;
    out_layout->scrollable = 1;
    out_layout->track = (SDL_Rect){viewport->x + viewport->w - 8, viewport->y, 6, viewport->h};
    thumb_h = (viewport->h * viewport->h) / content_height;
    if (thumb_h < viewport->h / 10) thumb_h = viewport->h / 10;
    if (thumb_h < 1) thumb_h = 1;
    if (thumb_h > viewport->h) thumb_h = viewport->h;
    max_offset = content_height - viewport->h;
    offset_y = kit_ui_sdl_clamp_int(offset_y, 0, max_offset);
    travel = out_layout->track.h - thumb_h;
    out_layout->thumb = (SDL_Rect){out_layout->track.x,
                                   out_layout->track.y + (travel > 0 ? (offset_y * travel) / max_offset : 0),
                                   out_layout->track.w,
                                   thumb_h};
}

void kit_ui_sdl_draw_scrollbar(SDL_Renderer *renderer,
                               const KitUiSdlScrollbarLayout *layout,
                               KitRenderColor track_color,
                               KitRenderColor thumb_color) {
    if (!renderer || !layout || !layout->scrollable) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, track_color.r, track_color.g, track_color.b, track_color.a);
    SDL_RenderFillRect(renderer, &layout->track);
    SDL_SetRenderDrawColor(renderer, thumb_color.r, thumb_color.g, thumb_color.b, thumb_color.a);
    SDL_RenderFillRect(renderer, &layout->thumb);
}
