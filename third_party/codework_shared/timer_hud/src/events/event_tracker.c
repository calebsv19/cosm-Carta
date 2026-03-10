#include "event_tracker.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char event_buffer[MAX_EVENTS_PER_FRAME][MAX_EVENT_NAME_LENGTH];
static size_t event_count = 0;

void event_tracker_init(void) {
    event_count = 0;
}

void event_tracker_frame_start(void) {
    event_count = 0;
}

void event_tracker_add(const char* tag) {
    if (event_count >= MAX_EVENTS_PER_FRAME) {
        fprintf(stderr, "[TimeScope] Max events per frame exceeded.\n");
        return;
    }
    strncpy(event_buffer[event_count], tag, MAX_EVENT_NAME_LENGTH - 1);
    event_buffer[event_count][MAX_EVENT_NAME_LENGTH - 1] = '\0';
    event_count++;
}

const char** event_tracker_get(size_t* count) {
    static const char* event_ptrs[MAX_EVENTS_PER_FRAME];
    for (size_t i = 0; i < event_count; i++) {
        event_ptrs[i] = event_buffer[i];
    }
    *count = event_count;
    return event_ptrs;
}

