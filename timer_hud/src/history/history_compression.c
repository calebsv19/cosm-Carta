#include "history_compression.h"
#include <string.h>

void history_add_summary(TimerHistory* history, const Timer* timer, double timestamp) {
    if (history->count >= MAX_SUMMARIES) {
        // Optional: overwrite oldest data or stop
        return;
    }

    TimerSummary* s = &history->summaries[history->count++];
    s->min = timer->min;
    s->max = timer->max;
    s->avg = timer->avg;
    s->stddev = timer->stddev;
    s->sample_count = timer->count;
    s->timestamp = timestamp;
}

void history_clear(TimerHistory* history) {
    history->count = 0;
}

