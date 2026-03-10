#include "timer_manager.h"
#include <string.h>
#include <stdio.h>

TimerManager g_timer_manager;

void tm_init(void) {
    g_timer_manager.count = 0;
}

static Timer* tm_find_existing(const char* name) {
    for (int i = 0; i < g_timer_manager.count; i++) {
        if (strcmp(g_timer_manager.timers[i].name, name) == 0) {
            return &g_timer_manager.timers[i];
        }
    }
    return NULL;
}

static Timer* tm_create_timer(const char* name) {
    if (g_timer_manager.count >= MAX_TIMERS) {
        fprintf(stderr, "[TimeScope] Max timer limit reached (%d)\n", MAX_TIMERS);
        return NULL;
    }

    Timer* timer = &g_timer_manager.timers[g_timer_manager.count++];
    timer_init(timer, name);
    return timer;
}

Timer* tm_get_timer(const char* name) {
    Timer* existing = tm_find_existing(name);
    if (existing) return existing;
    return tm_create_timer(name);
}

void ts_start_timer(const char* name) {
    Timer* t = tm_get_timer(name);
    if (t) timer_start(t);
}

void ts_stop_timer(const char* name) {
    Timer* t = tm_get_timer(name);
    if (t) timer_stop(t);
}

