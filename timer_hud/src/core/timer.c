#include "timer.h"
#include <time.h>
#include <math.h>
#include <string.h>

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void timer_init(Timer* timer, const char* name) {
    memset(timer, 0, sizeof(Timer));
    timer->name = name;
}

void timer_start(Timer* timer) {
    if (!timer->running) {
        timer->start_time = get_time_ns();
        timer->running = true;
    }
}

void timer_stop(Timer* timer) {
    if (!timer->running) return;

    uint64_t end_time = get_time_ns();
    double duration_ms = (end_time - timer->start_time) / 1e6;

    timer->durations[timer->index] = duration_ms;
    timer->index = (timer->index + 1) % TIMER_HISTORY_SIZE;
    if (timer->count < TIMER_HISTORY_SIZE) {
        timer->count++;
    }

    timer->running = false;
    timer_update_stats(timer);
}

void timer_update_stats(Timer* timer) {
    if (timer->count == 0) return;

    double sum = 0.0;
    double min = timer->durations[0];
    double max = timer->durations[0];

    for (size_t i = 0; i < timer->count; i++) {
        double d = timer->durations[i];
        sum += d;
        if (d < min) min = d;
        if (d > max) max = d;
    }

    timer->avg = sum / timer->count;
    timer->min = min;
    timer->max = max;

    double variance = 0.0;
    for (size_t i = 0; i < timer->count; i++) {
        double diff = timer->durations[i] - timer->avg;
        variance += diff * diff;
    }

    timer->stddev = sqrt(variance / timer->count);
}

size_t timer_get_history_count(const Timer* timer) {
    if (!timer) return 0;
    return timer->count;
}

double timer_get_history_sample(const Timer* timer, size_t sample_index) {
    if (!timer || sample_index >= timer->count) return 0.0;

    size_t oldest_index = (timer->index + TIMER_HISTORY_SIZE - timer->count) % TIMER_HISTORY_SIZE;
    size_t ring_index = (oldest_index + sample_index) % TIMER_HISTORY_SIZE;
    return timer->durations[ring_index];
}

size_t timer_copy_history(const Timer* timer, double* out_samples, size_t out_capacity) {
    if (!timer || !out_samples || out_capacity == 0) return 0;

    size_t n = timer->count;
    if (n > out_capacity) n = out_capacity;
    if (n == 0) return 0;

    size_t start = timer->count - n;
    for (size_t i = 0; i < n; ++i) {
        out_samples[i] = timer_get_history_sample(timer, start + i);
    }
    return n;
}
