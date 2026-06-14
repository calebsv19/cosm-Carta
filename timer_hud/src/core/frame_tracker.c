#include "frame_tracker.h"
#include "../events/event_tracker.h"
#include "../logging/logger.h"
#include "../core/session.h"

void ts_session_frame_start(TimerHUDSession* session) {
    if (ts_session_is_event_tagging_enabled(session)) {
        event_tracker_frame_start(session);
    }
}

void ts_session_frame_end(TimerHUDSession* session) {
    if (ts_session_is_log_enabled(session)) {
        logger_log_frame(session);
    }
}
