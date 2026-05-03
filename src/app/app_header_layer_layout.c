#include "app/app_ui_internal.h"

static float app_header_layer_layout_clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

float app_header_layer_strip_start_x(float strip_x, float strip_w, float content_w, float scroll_px) {
    if (strip_w <= 0.0f) {
        return strip_x;
    }
    if (content_w <= 0.0f) {
        return strip_x + strip_w;
    }

    float max_scroll = content_w - strip_w;
    if (max_scroll < 0.0f) {
        max_scroll = 0.0f;
    }
    float scroll = app_header_layer_layout_clampf(scroll_px, 0.0f, max_scroll);
    return strip_x + strip_w - content_w + scroll;
}
