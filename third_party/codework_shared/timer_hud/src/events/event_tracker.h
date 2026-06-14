#ifndef TIMESCOPE_EVENT_TRACKER_H
#define TIMESCOPE_EVENT_TRACKER_H

#include "../core/session_fwd.h"

#include <stddef.h>

#define MAX_EVENTS_PER_FRAME 64
#define MAX_EVENT_NAME_LENGTH 64

void event_tracker_init(TimerHUDSession* session);
void event_tracker_frame_start(TimerHUDSession* session);
void event_tracker_add(TimerHUDSession* session, const char* tag);
const char** event_tracker_get(TimerHUDSession* session, size_t* count);

#endif // TIMESCOPE_EVENT_TRACKER_H
