#include "event_tracker.h"
#include "../core/session.h"
#include <string.h>
#include <stdio.h>

void event_tracker_init(TimerHUDSession* session) {
    if (!session) {
        return;
    }
    session->event_count = 0;
}

void event_tracker_frame_start(TimerHUDSession* session) {
    if (!session) {
        return;
    }
    session->event_count = 0;
}

void event_tracker_add(TimerHUDSession* session, const char* tag) {
    if (!session || !tag) {
        return;
    }
    if (session->event_count >= MAX_EVENTS_PER_FRAME) {
        fprintf(stderr, "[TimeScope] Max events per frame exceeded.\n");
        return;
    }
    strncpy(session->event_buffer[session->event_count], tag, MAX_EVENT_NAME_LENGTH - 1);
    session->event_buffer[session->event_count][MAX_EVENT_NAME_LENGTH - 1] = '\0';
    session->event_count++;
}

const char** event_tracker_get(TimerHUDSession* session, size_t* count) {
    if (!session || !count) {
        return NULL;
    }
    for (size_t i = 0; i < session->event_count; i++) {
        session->event_ptrs[i] = session->event_buffer[i];
    }
    *count = session->event_count;
    return session->event_ptrs;
}
