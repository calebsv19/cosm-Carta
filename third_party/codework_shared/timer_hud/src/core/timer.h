#ifndef TIMESCOPE_TIMER_H
#define TIMESCOPE_TIMER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TIMER_HISTORY_SIZE 128
#define TIMER_NAME_MAX 96

typedef struct Timer {
    char name[TIMER_NAME_MAX];
    double durations[TIMER_HISTORY_SIZE];
    size_t index;
    size_t count;
    bool running;
    uint64_t start_time;

    // Cached stats
    double min;
    double max;
    double avg;
    double stddev;
} Timer;

void timer_init(Timer* timer, const char* name);
void timer_start(Timer* timer);
void timer_stop(Timer* timer);
void timer_record_duration_ms(Timer* timer, double duration_ms);
void timer_update_stats(Timer* timer);
size_t timer_get_history_count(const Timer* timer);
double timer_get_history_sample(const Timer* timer, size_t sample_index);
size_t timer_copy_history(const Timer* timer, double* out_samples, size_t out_capacity);

#endif // TIMESCOPE_TIMER_H
