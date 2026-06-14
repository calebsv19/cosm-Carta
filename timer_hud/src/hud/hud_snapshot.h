#ifndef TIMESCOPE_HUD_SNAPSHOT_H
#define TIMESCOPE_HUD_SNAPSHOT_H

#include "timer_hud/timer_hud_config.h"
#include "../core/timer.h"
#include "../core/session_fwd.h"
#include "../core/timer_manager.h"

#include <stdbool.h>
#include <stddef.h>

#define TIMER_HUD_ROW_TEXT_MAX 256
#define TIMER_HUD_GRAPH_LABEL_MAX 64

typedef enum TimerHUDAnchor {
    TIMER_HUD_ANCHOR_TOP_LEFT = 0,
    TIMER_HUD_ANCHOR_TOP_RIGHT = 1,
    TIMER_HUD_ANCHOR_BOTTOM_LEFT = 2,
    TIMER_HUD_ANCHOR_BOTTOM_RIGHT = 3,
} TimerHUDAnchor;

typedef struct TimerHUDGraphSnapshot {
    size_t sample_count;
    double scale_max_ms;
    char scale_label[TIMER_HUD_GRAPH_LABEL_MAX];
    double samples[TIMER_HISTORY_SIZE];
} TimerHUDGraphSnapshot;

typedef struct TimerHUDTimerAnalysis {
    double last_ms;
    double avg_ms;
    double min_ms;
    double max_ms;
    double p95_ms;
    double stddev_ms;
    double spike_threshold_ms;
    double avg_percent_of_total;
    int spike_count;
    size_t sample_count;
} TimerHUDTimerAnalysis;

typedef struct TimerHUDRowSnapshot {
    char text[TIMER_HUD_ROW_TEXT_MAX];
    bool has_graph;
    bool has_compare_bar;
    TimerHUDTimerAnalysis analysis;
    TimerHUDGraphSnapshot graph;
} TimerHUDRowSnapshot;

typedef struct TimerHUDRenderSnapshot {
    bool hud_enabled;
    TimerHUDVisualMode visual_mode;
    char mode_label[32];
    TimerHUDAnchor anchor;
    int offset_x;
    int offset_y;
    int graph_width;
    int graph_height;
    int row_count;
    TimerHUDRowSnapshot rows[MAX_TIMERS];
} TimerHUDRenderSnapshot;

bool hud_snapshot_build(TimerHUDSession* session, TimerHUDRenderSnapshot* out_snapshot);

#endif // TIMESCOPE_HUD_SNAPSHOT_H
