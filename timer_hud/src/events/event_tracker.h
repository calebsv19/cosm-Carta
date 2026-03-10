#ifndef TIMESCOPE_EVENT_TRACKER_H
#define TIMESCOPE_EVENT_TRACKER_H

#include <stddef.h>

#define MAX_EVENTS_PER_FRAME 64
#define MAX_EVENT_NAME_LENGTH 64

void event_tracker_init(void);
void event_tracker_frame_start(void);		
void event_tracker_add(const char* tag);
const char** event_tracker_get(size_t* count);

#endif // TIMESCOPE_EVENT_TRACKER_H

