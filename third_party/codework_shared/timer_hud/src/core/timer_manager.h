#ifndef TIMESCOPE_TIMER_MANAGER_H
#define TIMESCOPE_TIMER_MANAGER_H

#include "session_fwd.h"
#include "timer.h"

#define MAX_TIMERS 64

typedef struct TimerManager {
    Timer timers[MAX_TIMERS];
    int count;
} TimerManager;

void tm_init(TimerHUDSession* session);
Timer* tm_find_timer(TimerHUDSession* session, const char* name);
Timer* tm_find_or_create_timer(TimerHUDSession* session, const char* name);

#endif // TIMESCOPE_TIMER_MANAGER_H
