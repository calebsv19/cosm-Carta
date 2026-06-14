#include "hud_renderer.h"
#include "hud_snapshot.h"
#include "../core/session.h"

#include <stdio.h>
#include <string.h>

#define HUD_DEFAULT_PADDING 6
#define HUD_DEFAULT_SPACING 4
#define HUD_DEFAULT_BG_ALPHA 40
#define HUD_TEXT_COLOR (TimerHUDColor){255, 255, 255, 255}
#define HUD_GRAPH_COLOR (TimerHUDColor){64, 225, 255, 255}
#define HUD_GRAPH_BG_COLOR (TimerHUDColor){18, 18, 18, 120}
#define HUD_SCALE_TEXT_COLOR (TimerHUDColor){180, 180, 180, 255}
#define HUD_BAR_COLOR (TimerHUDColor){88, 180, 255, 190}
#define HUD_ROW_ACCENT_COLOR (TimerHUDColor){64, 225, 255, 150}
#define HUD_HEADER_ACCENT_COLOR (TimerHUDColor){120, 120, 120, 120}
#define HUD_GRAPH_ROW_PADDING 3
#define HUD_GRAPH_TEXT_GAP 3
#define HUD_HEADER_LABEL_GAP 14
#define HUD_COMPARE_BAR_HEIGHT 5
#define HUD_CONTROL_W 14
#define HUD_CONTROL_GAP 2
#define HUD_MIN_CONTENT_W 420

static int get_padding(const TimerHUDSession* session) {
    if (session && session->backend && session->backend->hud_padding > 0) return session->backend->hud_padding;
    return HUD_DEFAULT_PADDING;
}

static int get_spacing(const TimerHUDSession* session) {
    if (session && session->backend && session->backend->hud_spacing > 0) return session->backend->hud_spacing;
    return HUD_DEFAULT_SPACING;
}

static int get_bg_alpha(const TimerHUDSession* session) {
    if (session && session->backend && session->backend->hud_bg_alpha > 0) return session->backend->hud_bg_alpha;
    return HUD_DEFAULT_BG_ALPHA;
}

static int backend_get_line_height(const TimerHUDSession* session) {
    if (!session || !session->backend) return 0;
    if (session->backend->get_line_height) {
        return session->backend->get_line_height();
    }
    if (session->backend->measure_text) {
        int w = 0;
        int h = 0;
        if (session->backend->measure_text("Ag", &w, &h)) {
            return h;
        }
    }
    return 0;
}

static int mode_has_graph(TimerHUDVisualMode mode) {
    return mode == TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH ||
           mode == TIMER_HUD_VISUAL_MODE_HYBRID ||
           mode == TIMER_HUD_VISUAL_MODE_SPIKES;
}

static int mode_has_compare_bar(TimerHUDVisualMode mode) {
    return mode == TIMER_HUD_VISUAL_MODE_COMPARE;
}

static int mode_uses_stable_body(TimerHUDVisualMode mode) {
    return mode_has_graph(mode) ||
           mode == TIMER_HUD_VISUAL_MODE_STATS ||
           mode_has_compare_bar(mode);
}

static int anchor_is_right_aligned(TimerHUDAnchor anchor) {
    return anchor == TIMER_HUD_ANCHOR_TOP_RIGHT || anchor == TIMER_HUD_ANCHOR_BOTTOM_RIGHT;
}

static int anchor_is_bottom_aligned(TimerHUDAnchor anchor) {
    return anchor == TIMER_HUD_ANCHOR_BOTTOM_LEFT || anchor == TIMER_HUD_ANCHOR_BOTTOM_RIGHT;
}

static void draw_graph(const TimerHUDSession* session,
                       const TimerHUDGraphSnapshot* graph,
                       int x,
                       int y,
                       int w,
                       int h) {
    if (!session || !graph || w <= 1 || h <= 1) {
        return;
    }

    session->backend->draw_rect(x, y, w, h, HUD_GRAPH_BG_COLOR);

    if (graph->sample_count == 0) {
        return;
    }

    int prev_x = 0;
    int prev_y = 0;
    int has_prev = 0;

    for (size_t i = 0; i < graph->sample_count; ++i) {
        double normalized = graph->samples[i] / graph->scale_max_ms;
        if (normalized < 0.0) normalized = 0.0;
        if (normalized > 1.0) normalized = 1.0;

        int px = x;
        if (graph->sample_count > 1) {
            px = x + (int)((double)(w - 1) * (double)i / (double)(graph->sample_count - 1));
        }

        int py = y + h - 1 - (int)((double)(h - 1) * normalized);

        if (session->backend->draw_line && has_prev) {
            session->backend->draw_line(prev_x, prev_y, px, py, HUD_GRAPH_COLOR);
        } else if (!session->backend->draw_line) {
            int bar_h = (y + h) - py;
            if (bar_h < 1) bar_h = 1;
            session->backend->draw_rect(px, py, 1, bar_h, HUD_GRAPH_COLOR);
        }

        prev_x = px;
        prev_y = py;
        has_prev = 1;
    }

}

static void clear_control_rects(TimerHUDSession* session) {
    if (!session) {
        return;
    }
    session->control_mode_prev_w = 0;
    session->control_mode_prev_h = 0;
    session->control_mode_next_w = 0;
    session->control_mode_next_h = 0;
    session->control_mode_history_w = 0;
    session->control_mode_history_h = 0;
}

static void draw_control_button(const TimerHUDSession* session,
                                const char* label,
                                int x,
                                int y,
                                int w,
                                int h) {
    TimerHUDColor border = {120, 120, 120, 180};
    TimerHUDColor fill = {30, 30, 30, 160};
    if (!session || !session->backend || !label) {
        return;
    }
    session->backend->draw_rect(x, y, w, h, fill);
    if (session->backend->draw_line) {
        session->backend->draw_line(x, y, x + w - 1, y, border);
        session->backend->draw_line(x, y + h - 1, x + w - 1, y + h - 1, border);
        session->backend->draw_line(x, y, x, y + h - 1, border);
        session->backend->draw_line(x + w - 1, y, x + w - 1, y + h - 1, border);
    }
    session->backend->draw_text(label,
                                x + w / 2,
                                y + h / 2,
                                TIMER_HUD_ALIGN_CENTER | TIMER_HUD_ALIGN_MIDDLE,
                                HUD_SCALE_TEXT_COLOR);
}

void hud_set_backend(TimerHUDSession* session, const TimerHUDBackend* backend) {
    if (!session) {
        return;
    }
    session->backend = backend;
}

void hud_init(TimerHUDSession* session) {
    if (!session) {
        return;
    }
    memset(session->display_max_ms, 0, sizeof(session->display_max_ms));
    if (session->backend && session->backend->init) {
        session->backend->init();
    }
}

void hud_shutdown(TimerHUDSession* session) {
    if (session && session->backend && session->backend->shutdown) {
        session->backend->shutdown();
    }
}

void hud_render(TimerHUDSession* session) {
    TimerHUDRenderSnapshot snapshot;

    if (!session || !session->backend || !session->backend->draw_text || !session->backend->draw_rect) {
        return;
    }
    if (!hud_snapshot_build(session, &snapshot)) {
        return;
    }
    clear_control_rects(session);
    if (!snapshot.hud_enabled || snapshot.row_count <= 0) {
        return;
    }

    int stable_body = mode_uses_stable_body(snapshot.visual_mode);
    int padding = get_padding(session);
    int graph_padding = stable_body ? HUD_GRAPH_ROW_PADDING : padding;
    int spacing = get_spacing(session);

    int screenW = 0;
    int screenH = 0;
    if (!session->backend->get_screen_size || !session->backend->get_screen_size(&screenW, &screenH)) {
        screenW = 800;
        screenH = 600;
    }

    int fontHeight = backend_get_line_height(session);
    if (fontHeight <= 0) fontHeight = 14;

    int row_h[MAX_TIMERS];
    int contentWidth = snapshot.graph_width;
    int bodyHeight = snapshot.graph_height;
    int maxWidth = 0;
    int totalHeight = 0;
    int headerHeight = fontHeight + padding * 2;

    if (contentWidth < HUD_MIN_CONTENT_W) {
        contentWidth = HUD_MIN_CONTENT_W;
    }
    if (bodyHeight < HUD_COMPARE_BAR_HEIGHT) {
        bodyHeight = HUD_COMPARE_BAR_HEIGHT;
    }
    maxWidth = contentWidth + graph_padding * 2;

    if (snapshot.mode_label[0] != '\0') {
        totalHeight += headerHeight + spacing;
    }

    for (int i = 0; i < snapshot.row_count; ++i) {
        int h = fontHeight + graph_padding * 2;
        if (stable_body) {
            h += HUD_GRAPH_TEXT_GAP + bodyHeight;
        }
        row_h[i] = h;

        totalHeight += h + spacing;
    }
    totalHeight -= spacing;

    int offsetX = snapshot.offset_x;
    int offsetY = snapshot.offset_y;

    int baseX = 0;
    int baseY = 0;
    int rightAlign = anchor_is_right_aligned(snapshot.anchor);

    if (snapshot.anchor == TIMER_HUD_ANCHOR_TOP_LEFT) {
        baseX = offsetX;
        baseY = offsetY;
    } else if (snapshot.anchor == TIMER_HUD_ANCHOR_TOP_RIGHT) {
        baseX = screenW - offsetX - maxWidth;
        baseY = offsetY;
    } else if (snapshot.anchor == TIMER_HUD_ANCHOR_BOTTOM_LEFT) {
        baseX = offsetX;
        baseY = screenH - offsetY - totalHeight;
    } else if (snapshot.anchor == TIMER_HUD_ANCHOR_BOTTOM_RIGHT) {
        baseX = screenW - offsetX - maxWidth;
        baseY = screenH - offsetY - totalHeight;
    } else {
        baseX = offsetX;
        baseY = offsetY;
    }

    if (anchor_is_bottom_aligned(snapshot.anchor)) {
        baseY = screenH - offsetY - totalHeight;
    }

    int y = baseY;
    TimerHUDColor bg = {0, 0, 0, (unsigned char)get_bg_alpha(session)};

    if (snapshot.mode_label[0] != '\0') {
        int control_h = fontHeight;
        int control_y = y + padding;
        int control_total_w = HUD_CONTROL_W * 3 + HUD_CONTROL_GAP * 2;
        int control_x = rightAlign ? (baseX + padding) : (baseX + maxWidth - padding - control_total_w);

        session->backend->draw_rect(baseX, y, maxWidth, headerHeight, bg);
        session->backend->draw_text(snapshot.mode_label,
                                    rightAlign ? (baseX + maxWidth - padding) : (baseX + padding),
                                    y + padding,
                                    TIMER_HUD_ALIGN_TOP | (rightAlign ? TIMER_HUD_ALIGN_RIGHT : TIMER_HUD_ALIGN_LEFT),
                                    HUD_SCALE_TEXT_COLOR);
        session->control_mode_prev_x = control_x;
        session->control_mode_prev_y = control_y;
        session->control_mode_prev_w = HUD_CONTROL_W;
        session->control_mode_prev_h = control_h;
        session->control_mode_history_x = control_x + HUD_CONTROL_W + HUD_CONTROL_GAP;
        session->control_mode_history_y = control_y;
        session->control_mode_history_w = HUD_CONTROL_W;
        session->control_mode_history_h = control_h;
        session->control_mode_next_x = control_x + (HUD_CONTROL_W + HUD_CONTROL_GAP) * 2;
        session->control_mode_next_y = control_y;
        session->control_mode_next_w = HUD_CONTROL_W;
        session->control_mode_next_h = control_h;

        draw_control_button(session, "<", session->control_mode_prev_x, control_y, HUD_CONTROL_W, control_h);
        draw_control_button(session, "H", session->control_mode_history_x, control_y, HUD_CONTROL_W, control_h);
        draw_control_button(session, ">", session->control_mode_next_x, control_y, HUD_CONTROL_W, control_h);
        if (session->backend->draw_line) {
            session->backend->draw_line(baseX,
                                        y + headerHeight - 1,
                                        baseX + maxWidth - 1,
                                        y + headerHeight - 1,
                                        HUD_HEADER_ACCENT_COLOR);
        }
        y += headerHeight + spacing;
    }

    for (int i = 0; i < snapshot.row_count; ++i) {
        int card_x = baseX;
        int card_w = maxWidth;
        int card_h = row_h[i];

        session->backend->draw_rect(card_x, y, card_w, card_h, bg);
        session->backend->draw_rect(card_x, y, 2, card_h, HUD_ROW_ACCENT_COLOR);

        int text_x = rightAlign ? (card_x + card_w - graph_padding) : (card_x + graph_padding);
        int text_align = TIMER_HUD_ALIGN_TOP | (rightAlign ? TIMER_HUD_ALIGN_RIGHT : TIMER_HUD_ALIGN_LEFT);
        session->backend->draw_text(snapshot.rows[i].text, text_x, y + graph_padding, text_align, HUD_TEXT_COLOR);

        if (snapshot.rows[i].has_graph) {
            int graph_w = contentWidth;
            int graph_x = rightAlign ? (card_x + card_w - graph_padding - graph_w) : (card_x + graph_padding);
            int graph_y = y + graph_padding + fontHeight + HUD_GRAPH_TEXT_GAP;
            int scale_x = rightAlign ? (card_x + graph_padding) : (card_x + card_w - graph_padding);
            int scale_align = TIMER_HUD_ALIGN_TOP | (rightAlign ? TIMER_HUD_ALIGN_LEFT : TIMER_HUD_ALIGN_RIGHT);

            if (snapshot.rows[i].graph.scale_label[0] != '\0') {
                session->backend->draw_text(snapshot.rows[i].graph.scale_label,
                                            scale_x,
                                            y + graph_padding,
                                            scale_align,
                                            HUD_SCALE_TEXT_COLOR);
            }

            draw_graph(session, &snapshot.rows[i].graph, graph_x, graph_y, graph_w, bodyHeight);
        } else if (snapshot.rows[i].has_compare_bar) {
            int bar_w = card_w - graph_padding * 2;
            int bar_x = card_x + graph_padding;
            int bar_y = y + graph_padding + fontHeight + HUD_GRAPH_TEXT_GAP + (bodyHeight - HUD_COMPARE_BAR_HEIGHT) / 2;
            double normalized = snapshot.rows[i].analysis.avg_percent_of_total / 100.0;
            int fill_w = 0;
            if (normalized < 0.0) normalized = 0.0;
            if (normalized > 1.0) normalized = 1.0;
            fill_w = (int)((double)bar_w * normalized);
            if (fill_w < 1 && normalized > 0.0) {
                fill_w = 1;
            }
            session->backend->draw_rect(bar_x, bar_y, bar_w, HUD_COMPARE_BAR_HEIGHT, HUD_GRAPH_BG_COLOR);
            if (fill_w > 0) {
                session->backend->draw_rect(bar_x, bar_y, fill_w, HUD_COMPARE_BAR_HEIGHT, HUD_BAR_COLOR);
            }
        } else if (stable_body) {
            int body_x = card_x + graph_padding;
            int body_y = y + graph_padding + fontHeight + HUD_GRAPH_TEXT_GAP + bodyHeight - 1;
            int body_w = card_w - graph_padding * 2;
            if (session->backend->draw_line) {
                session->backend->draw_line(body_x,
                                            body_y,
                                            body_x + body_w - 1,
                                            body_y,
                                            HUD_HEADER_ACCENT_COLOR);
            }
        }

        y += card_h + spacing;
    }
}
