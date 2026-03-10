#include "frame_tracker.h"
#include "../events/event_tracker.h"
#include "../logging/logger.h"
#include "timer_hud/settings_loader.h"

void ts_frame_start(void) {
    if (ts_settings.event_tagging_enabled) {
        event_tracker_frame_start();
    }
}

void ts_frame_end(void) {
    if (ts_settings.log_enabled) {
        logger_log_frame();
    }
}
