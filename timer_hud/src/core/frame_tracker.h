#ifndef TIMESCOPE_FRAME_TRACKER_H
#define TIMESCOPE_FRAME_TRACKER_H

#include "session_fwd.h"

void ts_session_frame_start(TimerHUDSession* session);  // Call at beginning of main loop
void ts_session_frame_end(TimerHUDSession* session);    // Call at end of main loop

#endif // TIMESCOPE_FRAME_TRACKER_H
