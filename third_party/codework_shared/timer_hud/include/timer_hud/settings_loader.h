#ifndef TIMESCOPE_SETTINGS_LOADER_H
#define TIMESCOPE_SETTINGS_LOADER_H

#include "timer_hud/timer_hud_config.h"

typedef TimerHUDSettings TimeScopeSettings;

TimerHUDSettings* ts_legacy_settings_mutable(void);
#define ts_settings (*ts_legacy_settings_mutable())

bool ts_session_load_settings(TimerHUDSession* session, const char* filepath);
void ts_session_save_settings_to_file(TimerHUDSession* session, const char* path);
bool ts_load_settings(const char* filepath);
void save_settings_to_file(const char* path);

#endif // TIMESCOPE_SETTINGS_LOADER_H
