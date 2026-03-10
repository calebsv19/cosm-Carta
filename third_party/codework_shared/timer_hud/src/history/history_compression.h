#ifndef TIMESCOPE_HISTORY_COMPRESSION_H
#define TIMESCOPE_HISTORY_COMPRESSION_H

#include "../core/timer.h"

#define MAX_SUMMARIES 1024

typedef struct TimerSummary {
    double min;
    double max;
    double avg;
    double stddev;
    size_t sample_count;
    double timestamp;  // Optional: seconds since launch or frame count
} TimerSummary;

typedef struct TimerHistory {
    TimerSummary summaries[MAX_SUMMARIES];
    size_t count;
} TimerHistory;

// Adds a snapshot of the current Timer state to a history buffer
void history_add_summary(TimerHistory* history, const Timer* timer, double timestamp);

// Optional: clear history
void history_clear(TimerHistory* history);

#endif // TIMESCOPE_HISTORY_COMPRESSION_H

