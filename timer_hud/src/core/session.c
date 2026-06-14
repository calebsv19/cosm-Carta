#include "session.h"
#include "timer_hud/timer_hud_config.h"

#include <stdlib.h>
#include <string.h>

static TimerHUDSession g_default_session;
static bool g_default_session_ready = false;

static void session_zero(TimerHUDSession* session) {
    if (!session) {
        return;
    }
    memset(session, 0, sizeof(*session));
    ts_get_default_settings_copy(&session->settings);
    session->log_format = LOG_FORMAT_JSON;
}

TimerHUDSession* ts_session_create_internal(void) {
    TimerHUDSession* session = (TimerHUDSession*)calloc(1, sizeof(*session));
    if (!session) {
        return NULL;
    }
    session_zero(session);
    return session;
}

void ts_session_destroy_internal(TimerHUDSession* session) {
    free(session);
}

TimerHUDSession* ts_default_session_internal(void) {
    if (!g_default_session_ready) {
        session_zero(&g_default_session);
        g_default_session_ready = true;
    }
    return &g_default_session;
}
