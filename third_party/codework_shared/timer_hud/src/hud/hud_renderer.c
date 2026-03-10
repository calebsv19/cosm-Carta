#include "hud_renderer.h"
#include "../core/timer_manager.h"
#include "timer_hud/settings_loader.h"

#include <stdio.h>
#include <string.h>

#define HUD_DEFAULT_PADDING 6
#define HUD_DEFAULT_SPACING 4
#define HUD_DEFAULT_BG_ALPHA 40
#define HUD_TEXT_COLOR (TimerHUDColor){255, 255, 255, 255}
#define HUD_GRAPH_COLOR (TimerHUDColor){64, 225, 255, 255}
#define HUD_GRAPH_BG_COLOR (TimerHUDColor){18, 18, 18, 120}
#define HUD_SCALE_TEXT_COLOR (TimerHUDColor){180, 180, 180, 255}
#define HUD_GRAPH_ROW_PADDING 3
#define HUD_GRAPH_TEXT_GAP 2
#define MAX_HUD_LINES MAX_TIMERS

static const TimerHUDBackend* g_backend = NULL;
static double g_display_max_ms[MAX_HUD_LINES];

static int get_padding(void) {
    if (g_backend && g_backend->hud_padding > 0) return g_backend->hud_padding;
    return HUD_DEFAULT_PADDING;
}

static int get_spacing(void) {
    if (g_backend && g_backend->hud_spacing > 0) return g_backend->hud_spacing;
    return HUD_DEFAULT_SPACING;
}

static int get_bg_alpha(void) {
    if (g_backend && g_backend->hud_bg_alpha > 0) return g_backend->hud_bg_alpha;
    return HUD_DEFAULT_BG_ALPHA;
}

static int backend_get_line_height(void) {
    if (!g_backend) return 0;
    if (g_backend->get_line_height) {
        return g_backend->get_line_height();
    }
    if (g_backend->measure_text) {
        int w = 0;
        int h = 0;
        if (g_backend->measure_text("Ag", &w, &h)) {
            return h;
        }
    }
    return 0;
}

static int backend_measure_text(const char* text, int* out_w, int* out_h) {
    if (g_backend && g_backend->measure_text) {
        return g_backend->measure_text(text, out_w, out_h);
    }
    if (out_w) *out_w = (int)strlen(text) * 8;
    if (out_h) *out_h = 14;
    return 0;
}

static int is_mode_history_graph(void) {
    return strcmp(ts_settings.hud_visual_mode, "history_graph") == 0;
}

static int is_mode_hybrid(void) {
    return strcmp(ts_settings.hud_visual_mode, "hybrid") == 0;
}

static int mode_has_graph(void) {
    return is_mode_history_graph() || is_mode_hybrid();
}

static void build_timer_text(const Timer* t, char* out, size_t out_cap) {
    if (!t || !out || out_cap == 0) return;

    if (!ts_settings.hud_compact_text) {
        snprintf(out,
                 out_cap,
                 "%s: %.2f ms (min %.2f / max %.2f / sigma %.2f)",
                 t->name,
                 t->avg,
                 t->min,
                 t->max,
                 t->stddev);
        return;
    }

    int used = snprintf(out, out_cap, "%s %.2fms", t->name, t->avg);
    if (used < 0 || (size_t)used >= out_cap) {
        out[out_cap - 1] = '\0';
        return;
    }

    if (ts_settings.hud_show_avg) {
        used += snprintf(out + used, out_cap - (size_t)used, " avg %.2f", t->avg);
    }
    if (ts_settings.hud_show_minmax) {
        used += snprintf(out + used, out_cap - (size_t)used, " min %.2f max %.2f", t->min, t->max);
    }
    if (ts_settings.hud_show_stddev) {
        (void)snprintf(out + used, out_cap - (size_t)used, " sd %.2f", t->stddev);
    }
}

static double compute_graph_scale_max(size_t row_index, const double* samples, size_t sample_count) {
    double observed_max = 0.0;
    for (size_t i = 0; i < sample_count; ++i) {
        if (samples[i] > observed_max) observed_max = samples[i];
    }
    if (observed_max < 0.1) observed_max = 0.1;

    if (strcmp(ts_settings.hud_scale_mode, "fixed") == 0) {
        double fixed_max = (double)ts_settings.hud_scale_fixed_max_ms;
        return fixed_max >= 0.1 ? fixed_max : 0.1;
    }

    double target_max = observed_max * 1.10;
    if (g_display_max_ms[row_index] <= 0.0 || g_display_max_ms[row_index] < target_max) {
        g_display_max_ms[row_index] = target_max;
    } else {
        double decay = (double)ts_settings.hud_scale_decay;
        g_display_max_ms[row_index] = g_display_max_ms[row_index] * decay + target_max * (1.0 - decay);
    }

    if (g_display_max_ms[row_index] < 0.1) {
        g_display_max_ms[row_index] = 0.1;
    }
    return g_display_max_ms[row_index];
}

static void draw_graph(size_t row_index, const Timer* timer, int x, int y, int w, int h) {
    if (!timer || w <= 1 || h <= 1) return;

    g_backend->draw_rect(x, y, w, h, HUD_GRAPH_BG_COLOR);

    double samples[TIMER_HISTORY_SIZE];
    size_t requested = (size_t)ts_settings.hud_graph_samples;
    size_t sample_count = timer_copy_history(timer, samples, requested);
    if (sample_count == 0) {
        return;
    }

    double scale_max = compute_graph_scale_max(row_index, samples, sample_count);

    int prev_x = 0;
    int prev_y = 0;
    int has_prev = 0;

    for (size_t i = 0; i < sample_count; ++i) {
        double normalized = samples[i] / scale_max;
        if (normalized < 0.0) normalized = 0.0;
        if (normalized > 1.0) normalized = 1.0;

        int px = x;
        if (sample_count > 1) {
            px = x + (int)((double)(w - 1) * (double)i / (double)(sample_count - 1));
        }

        int py = y + h - 1 - (int)((double)(h - 1) * normalized);

        if (g_backend->draw_line && has_prev) {
            g_backend->draw_line(prev_x, prev_y, px, py, HUD_GRAPH_COLOR);
        } else if (!g_backend->draw_line) {
            int bar_h = (y + h) - py;
            if (bar_h < 1) bar_h = 1;
            g_backend->draw_rect(px, py, 1, bar_h, HUD_GRAPH_COLOR);
        }

        prev_x = px;
        prev_y = py;
        has_prev = 1;
    }

    char scale_line[64];
    snprintf(scale_line, sizeof(scale_line), "%.2fms max", scale_max);
    int scale_x = x + w - 2;
    int scale_y = y + 1;
    g_backend->draw_text(scale_line,
                         scale_x,
                         scale_y,
                         TIMER_HUD_ALIGN_TOP | TIMER_HUD_ALIGN_RIGHT,
                         HUD_SCALE_TEXT_COLOR);
}

void hud_set_backend(const TimerHUDBackend* backend) {
    g_backend = backend;
}

void hud_init(void) {
    memset(g_display_max_ms, 0, sizeof(g_display_max_ms));
    if (g_backend && g_backend->init) {
        g_backend->init();
    }
}

void hud_shutdown(void) {
    if (g_backend && g_backend->shutdown) {
        g_backend->shutdown();
    }
}

void ts_render(void) {
    if (!g_backend || !g_backend->draw_text || !g_backend->draw_rect) {
        return;
    }
    if (!ts_settings.hud_enabled) {
        return;
    }

    TimerManager* tm = &g_timer_manager;
    int timer_count = tm->count;
    if (timer_count > MAX_HUD_LINES) timer_count = MAX_HUD_LINES;
    if (timer_count <= 0) return;

    int graph_enabled = mode_has_graph();
    int padding = get_padding();
    int graph_padding = graph_enabled ? HUD_GRAPH_ROW_PADDING : padding;
    int spacing = get_spacing();

    int screenW = 0;
    int screenH = 0;
    if (!g_backend->get_screen_size || !g_backend->get_screen_size(&screenW, &screenH)) {
        screenW = 800;
        screenH = 600;
    }

    int fontHeight = backend_get_line_height();
    if (fontHeight <= 0) fontHeight = 14;

    int row_h[MAX_HUD_LINES];
    char row_text[MAX_HUD_LINES][256];

    int maxWidth = 0;
    int totalHeight = 0;

    for (int i = 0; i < timer_count; ++i) {
        Timer* t = &tm->timers[i];
        build_timer_text(t, row_text[i], sizeof(row_text[i]));

        int textW = 0;
        int textH = 0;
        backend_measure_text(row_text[i], &textW, &textH);
        int content_w = textW;
        if (graph_enabled && ts_settings.hud_graph_width > content_w) {
            content_w = ts_settings.hud_graph_width;
        }

        int h = fontHeight + graph_padding * 2;
        if (graph_enabled) {
            h += HUD_GRAPH_TEXT_GAP + ts_settings.hud_graph_height;
        }
        row_h[i] = h;

        int row_w = content_w + graph_padding * 2;
        if (row_w > maxWidth) maxWidth = row_w;

        totalHeight += h + spacing;
    }
    totalHeight -= spacing;

    const char* pos = ts_settings.hud_position;
    int offsetX = ts_settings.hud_offset_x;
    int offsetY = ts_settings.hud_offset_y;

    int baseX = 0;
    int baseY = 0;
    int rightAlign = 0;

    if (strcmp(pos, "top-left") == 0) {
        baseX = offsetX;
        baseY = offsetY;
    } else if (strcmp(pos, "top-right") == 0) {
        baseX = screenW - offsetX - maxWidth;
        baseY = offsetY;
        rightAlign = 1;
    } else if (strcmp(pos, "bottom-left") == 0) {
        baseX = offsetX;
        baseY = screenH - offsetY - totalHeight;
    } else if (strcmp(pos, "bottom-right") == 0) {
        baseX = screenW - offsetX - maxWidth;
        baseY = screenH - offsetY - totalHeight;
        rightAlign = 1;
    } else {
        baseX = offsetX;
        baseY = offsetY;
    }

    int y = baseY;
    TimerHUDColor bg = {0, 0, 0, (unsigned char)get_bg_alpha()};

    for (int i = 0; i < timer_count; ++i) {
        int card_x = baseX;
        int card_w = maxWidth;
        int card_h = row_h[i];

        g_backend->draw_rect(card_x, y, card_w, card_h, bg);

        int text_x = rightAlign ? (card_x + card_w - graph_padding) : (card_x + graph_padding);
        int text_align = TIMER_HUD_ALIGN_TOP | (rightAlign ? TIMER_HUD_ALIGN_RIGHT : TIMER_HUD_ALIGN_LEFT);
        g_backend->draw_text(row_text[i], text_x, y + graph_padding, text_align, HUD_TEXT_COLOR);

        if (graph_enabled) {
            int graph_w = ts_settings.hud_graph_width;
            int graph_x = rightAlign ? (card_x + card_w - graph_padding - graph_w) : (card_x + graph_padding);
            int graph_y = y + graph_padding + fontHeight + HUD_GRAPH_TEXT_GAP;

            draw_graph((size_t)i, &tm->timers[i], graph_x, graph_y, graph_w, ts_settings.hud_graph_height);
        }

        y += card_h + spacing;
    }
}
