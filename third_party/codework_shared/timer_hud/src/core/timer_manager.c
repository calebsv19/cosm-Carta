#include "timer_manager.h"
#include "session.h"
#include <string.h>
#include <stdio.h>

void tm_init(TimerHUDSession* session) {
    if (!session) {
        return;
    }
    session->timer_manager.count = 0;
}

Timer* tm_find_timer(TimerHUDSession* session, const char* name) {
    if (!session || !name || !name[0]) {
        return NULL;
    }
    for (int i = 0; i < session->timer_manager.count; i++) {
        if (strcmp(session->timer_manager.timers[i].name, name) == 0) {
            return &session->timer_manager.timers[i];
        }
    }
    return NULL;
}

static Timer* tm_create_timer(TimerHUDSession* session, const char* name) {
    if (!session || !name || !name[0]) {
        fprintf(stderr, "[TimeScope] Refusing to create timer with empty name.\n");
        return NULL;
    }
    if (session->timer_manager.count >= MAX_TIMERS) {
        fprintf(stderr, "[TimeScope] Max timer limit reached (%d)\n", MAX_TIMERS);
        return NULL;
    }

    Timer* timer = &session->timer_manager.timers[session->timer_manager.count++];
    timer_init(timer, name);
    return timer;
}

Timer* tm_find_or_create_timer(TimerHUDSession* session, const char* name) {
    Timer* existing = tm_find_timer(session, name);
    if (existing) return existing;
    return tm_create_timer(session, name);
}
