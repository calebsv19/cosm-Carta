#include "hud_snapshot.h"
#include "hud_format.h"

#include "../core/session.h"
#include <stdio.h>
#include <string.h>

static TimerHUDAnchor parse_anchor(const char* position) {
    if (!position || !position[0]) {
        return TIMER_HUD_ANCHOR_TOP_LEFT;
    }
    if (strcmp(position, "top-right") == 0) {
        return TIMER_HUD_ANCHOR_TOP_RIGHT;
    }
    if (strcmp(position, "bottom-left") == 0) {
        return TIMER_HUD_ANCHOR_BOTTOM_LEFT;
    }
    if (strcmp(position, "bottom-right") == 0) {
        return TIMER_HUD_ANCHOR_BOTTOM_RIGHT;
    }
    return TIMER_HUD_ANCHOR_TOP_LEFT;
}

static bool mode_has_graph(TimerHUDVisualMode mode) {
    return mode == TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH ||
           mode == TIMER_HUD_VISUAL_MODE_HYBRID ||
           mode == TIMER_HUD_VISUAL_MODE_SPIKES;
}

static bool mode_has_compare_bar(TimerHUDVisualMode mode) {
    return mode == TIMER_HUD_VISUAL_MODE_COMPARE;
}

static const char* mode_label(TimerHUDVisualMode mode) {
    switch (mode) {
        case TIMER_HUD_VISUAL_MODE_STATS:
            return "TimerHUD stats";
        case TIMER_HUD_VISUAL_MODE_SPIKES:
            return "TimerHUD spikes";
        case TIMER_HUD_VISUAL_MODE_COMPARE:
            return "TimerHUD compare";
        case TIMER_HUD_VISUAL_MODE_TEXT_COMPACT:
            return "TimerHUD compact";
        case TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH:
        case TIMER_HUD_VISUAL_MODE_HYBRID:
        default:
            return "TimerHUD history";
    }
}

static void sort_doubles(double* values, size_t count) {
    for (size_t i = 1; i < count; ++i) {
        double value = values[i];
        size_t j = i;
        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = value;
    }
}

static void build_timer_analysis(TimerHUDTimerAnalysis* analysis,
                                 const Timer* timer,
                                 const double* samples,
                                 size_t sample_count,
                                 double total_avg_ms) {
    double sorted[TIMER_HISTORY_SIZE];

    if (!analysis || !timer) {
        return;
    }

    memset(analysis, 0, sizeof(*analysis));
    analysis->avg_ms = timer->avg;
    analysis->min_ms = timer->min;
    analysis->max_ms = timer->max;
    analysis->stddev_ms = timer->stddev;
    analysis->sample_count = sample_count;
    if (sample_count > 0) {
        analysis->last_ms = samples[sample_count - 1];
        for (size_t i = 0; i < sample_count; ++i) {
            sorted[i] = samples[i];
        }
        sort_doubles(sorted, sample_count);
        {
            size_t p95_index = (sample_count * 95u) / 100u;
            if (p95_index >= sample_count) {
                p95_index = sample_count - 1;
            }
            analysis->p95_ms = sorted[p95_index];
        }
    } else {
        analysis->last_ms = timer->avg;
        analysis->p95_ms = timer->max;
    }

    analysis->spike_threshold_ms = timer->avg + (timer->stddev * 2.0);
    if (analysis->spike_threshold_ms < timer->avg * 2.0) {
        analysis->spike_threshold_ms = timer->avg * 2.0;
    }
    if (analysis->spike_threshold_ms < 0.1) {
        analysis->spike_threshold_ms = 0.1;
    }
    for (size_t i = 0; i < sample_count; ++i) {
        if (samples[i] >= analysis->spike_threshold_ms) {
            analysis->spike_count++;
        }
    }
    if (total_avg_ms > 0.0) {
        analysis->avg_percent_of_total = (timer->avg / total_avg_ms) * 100.0;
    }
}

static void build_timer_text(const TimerHUDSettings* settings,
                             TimerHUDVisualMode visual_mode,
                             const Timer* timer,
                             const TimerHUDTimerAnalysis* analysis,
                             char* out,
                             size_t out_cap) {
    char last_text[32];
    char avg_text[32];
    char min_text[32];
    char max_text[32];
    char p95_text[32];
    char stddev_text[32];

    if (!timer || !analysis || !out || out_cap == 0) {
        return;
    }

    hud_format_duration_ms(analysis->last_ms, last_text, sizeof(last_text));
    hud_format_duration_ms(analysis->avg_ms, avg_text, sizeof(avg_text));
    hud_format_duration_ms(analysis->min_ms, min_text, sizeof(min_text));
    hud_format_duration_ms(analysis->max_ms, max_text, sizeof(max_text));
    hud_format_duration_ms(analysis->p95_ms, p95_text, sizeof(p95_text));
    hud_format_duration_ms(analysis->stddev_ms, stddev_text, sizeof(stddev_text));

    if (visual_mode == TIMER_HUD_VISUAL_MODE_STATS) {
        snprintf(out,
                 out_cap,
                 "%s  last %s  avg %s  p95 %s",
                 timer->name,
                 last_text,
                 avg_text,
                 p95_text);
        return;
    }

    if (visual_mode == TIMER_HUD_VISUAL_MODE_SPIKES) {
        snprintf(out,
                 out_cap,
                 "%s  spk %d/%zu  p95 %s  max %s",
                 timer->name,
                 analysis->spike_count,
                 analysis->sample_count,
                 p95_text,
                 max_text);
        return;
    }

    if (visual_mode == TIMER_HUD_VISUAL_MODE_COMPARE) {
        snprintf(out,
                 out_cap,
                 "%s  avg %s  share %.0f%%",
                 timer->name,
                 avg_text,
                 analysis->avg_percent_of_total);
        return;
    }

    if (!settings || !settings->hud_compact_text) {
        snprintf(out,
                 out_cap,
                 "%s: avg %s (min %s / max %s / sigma %s)",
                 timer->name,
                 avg_text,
                 min_text,
                 max_text,
                 stddev_text);
        return;
    }

    int used = snprintf(out,
                        out_cap,
                        settings->hud_show_avg ? "%s avg %s" : "%s %s",
                        timer->name,
                        avg_text);
    if (used < 0 || (size_t)used >= out_cap) {
        out[out_cap - 1] = '\0';
        return;
    }

    if (settings->hud_show_minmax) {
        used += snprintf(out + used, out_cap - (size_t)used, " min %s max %s", min_text, max_text);
    }
    if (settings->hud_show_stddev) {
        (void)snprintf(out + used, out_cap - (size_t)used, " sd %s", stddev_text);
    }
}

static double compute_graph_scale_max(TimerHUDSession* session,
                                      const TimerHUDSettings* settings,
                                      size_t row_index,
                                      const double* samples,
                                      size_t sample_count) {
    double observed_max = 0.0;
    for (size_t i = 0; i < sample_count; ++i) {
        if (samples[i] > observed_max) {
            observed_max = samples[i];
        }
    }
    if (observed_max < 0.1) {
        observed_max = 0.1;
    }

    if (settings && strcmp(settings->hud_scale_mode, "fixed") == 0) {
        double fixed_max = (double)settings->hud_scale_fixed_max_ms;
        return fixed_max >= 0.1 ? fixed_max : 0.1;
    }

    {
        double target_max = observed_max * 1.10;
        if (session->display_max_ms[row_index] <= 0.0 ||
            session->display_max_ms[row_index] < target_max) {
            session->display_max_ms[row_index] = target_max;
        } else {
            double decay = settings ? (double)settings->hud_scale_decay : 0.94;
            session->display_max_ms[row_index] =
                session->display_max_ms[row_index] * decay + target_max * (1.0 - decay);
        }
    }

    if (session->display_max_ms[row_index] < 0.1) {
        session->display_max_ms[row_index] = 0.1;
    }
    return session->display_max_ms[row_index];
}

bool hud_snapshot_build(TimerHUDSession* session, TimerHUDRenderSnapshot* out_snapshot) {
    const TimerHUDSettings* settings = NULL;

    if (!session || !out_snapshot) {
        return false;
    }
    settings = ts_session_get_settings(session);
    if (!settings) {
        return false;
    }

    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->hud_enabled = settings->hud_enabled;
    out_snapshot->visual_mode = ts_session_get_hud_visual_mode_kind(session);
    if (out_snapshot->visual_mode == TIMER_HUD_VISUAL_MODE_INVALID) {
        out_snapshot->visual_mode = TIMER_HUD_VISUAL_MODE_TEXT_COMPACT;
    }
    snprintf(out_snapshot->mode_label, sizeof(out_snapshot->mode_label), "%s", mode_label(out_snapshot->visual_mode));
    out_snapshot->anchor = parse_anchor(settings->hud_position);
    out_snapshot->offset_x = settings->hud_offset_x;
    out_snapshot->offset_y = settings->hud_offset_y;
    out_snapshot->graph_width = settings->hud_graph_width;
    out_snapshot->graph_height = settings->hud_graph_height;

    if (!out_snapshot->hud_enabled) {
        return true;
    }

    {
        TimerManager* timer_manager = &session->timer_manager;
        int timer_count = timer_manager->count;
        bool graph_enabled = mode_has_graph(out_snapshot->visual_mode);
        bool compare_enabled = mode_has_compare_bar(out_snapshot->visual_mode);
        double total_avg_ms = 0.0;

        if (timer_count > MAX_TIMERS) {
            timer_count = MAX_TIMERS;
        }
        out_snapshot->row_count = timer_count;

        for (int i = 0; i < timer_count; ++i) {
            total_avg_ms += timer_manager->timers[i].avg;
        }

        for (int i = 0; i < timer_count; ++i) {
            const Timer* timer = &timer_manager->timers[i];
            TimerHUDRowSnapshot* row = &out_snapshot->rows[i];
            size_t requested_samples = (size_t)settings->hud_graph_samples;
            char scale_text[32];

            row->graph.sample_count = timer_copy_history(timer,
                                                         row->graph.samples,
                                                         requested_samples);
            build_timer_analysis(&row->analysis,
                                 timer,
                                 row->graph.samples,
                                 row->graph.sample_count,
                                 total_avg_ms);
            build_timer_text(settings,
                             out_snapshot->visual_mode,
                             timer,
                             &row->analysis,
                             row->text,
                             sizeof(row->text));

            row->has_graph = graph_enabled;
            row->has_compare_bar = compare_enabled;

            if (graph_enabled) {
                row->graph.scale_max_ms = compute_graph_scale_max(session,
                                                                  settings,
                                                                  (size_t)i,
                                                                  row->graph.samples,
                                                                  row->graph.sample_count);
                hud_format_duration_ms(row->graph.scale_max_ms, scale_text, sizeof(scale_text));
                snprintf(row->graph.scale_label, sizeof(row->graph.scale_label), "max %s", scale_text);
            }
        }
    }

    return true;
}
