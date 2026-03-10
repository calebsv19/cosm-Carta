#ifndef TIMESCOPE_TIMER_MANAGER_H
#define TIMESCOPE_TIMER_MANAGER_H

#include "timer.h"

#define MAX_TIMERS 64

typedef struct TimerManager {
    Timer timers[MAX_TIMERS];
    int count;
} TimerManager;

// Global access
extern TimerManager g_timer_manager;

void tm_init(void);
void ts_start_timer(const char* name);
void ts_stop_timer(const char* name);
Timer* tm_get_timer(const char* name);

#endif // TIMESCOPE_TIMER_MANAGER_H

