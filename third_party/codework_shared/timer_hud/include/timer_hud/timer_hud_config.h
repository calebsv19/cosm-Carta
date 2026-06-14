#ifndef TIMER_HUD_CONFIG_H
#define TIMER_HUD_CONFIG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TIMESCOPE_TIMER_HUD_SESSION_TYPEDEF_DONE
#define TIMESCOPE_TIMER_HUD_SESSION_TYPEDEF_DONE 1
typedef struct TimerHUDSession TimerHUDSession;
#endif

typedef struct TimerHUDSettings {
    bool hud_enabled;
    bool log_enabled;
    bool event_tagging_enabled;

    char log_filepath[256];
    char log_format[16];

    char render_mode[16];
    float render_threshold;

    char hud_position[32];
    int hud_offset_x;
    int hud_offset_y;

    char hud_visual_mode[24];
    bool hud_compact_text;
    int hud_graph_samples;
    int hud_graph_width;
    int hud_graph_height;

    char hud_scale_mode[16];
    float hud_scale_fixed_max_ms;
    float hud_scale_decay;

    bool hud_show_avg;
    bool hud_show_minmax;
    bool hud_show_stddev;
} TimerHUDSettings;

typedef enum TimerHUDVisualMode {
    TIMER_HUD_VISUAL_MODE_INVALID = -1,
    TIMER_HUD_VISUAL_MODE_TEXT_COMPACT = 0,
    TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH = 1,
    TIMER_HUD_VISUAL_MODE_HYBRID = 2,
    TIMER_HUD_VISUAL_MODE_STATS = 3,
    TIMER_HUD_VISUAL_MODE_SPIKES = 4,
    TIMER_HUD_VISUAL_MODE_COMPARE = 5,
} TimerHUDVisualMode;

const TimerHUDSettings* ts_get_settings(void);
void ts_get_settings_copy(TimerHUDSettings* out_settings);
bool ts_apply_settings(const TimerHUDSettings* settings);
void ts_get_default_settings_copy(TimerHUDSettings* out_settings);

const TimerHUDSettings* ts_session_get_settings(const TimerHUDSession* session);
void ts_session_get_settings_copy(const TimerHUDSession* session, TimerHUDSettings* out_settings);
bool ts_session_apply_settings(TimerHUDSession* session, const TimerHUDSettings* settings);

void ts_set_hud_enabled(bool enabled);
bool ts_is_hud_enabled(void);
void ts_session_set_hud_enabled(TimerHUDSession* session, bool enabled);
bool ts_session_is_hud_enabled(const TimerHUDSession* session);

void ts_set_log_enabled(bool enabled);
bool ts_is_log_enabled(void);
void ts_session_set_log_enabled(TimerHUDSession* session, bool enabled);
bool ts_session_is_log_enabled(const TimerHUDSession* session);

void ts_set_event_tagging_enabled(bool enabled);
bool ts_is_event_tagging_enabled(void);
void ts_session_set_event_tagging_enabled(TimerHUDSession* session, bool enabled);
bool ts_session_is_event_tagging_enabled(const TimerHUDSession* session);

const char* ts_visual_mode_name(TimerHUDVisualMode mode);
TimerHUDVisualMode ts_visual_mode_from_string(const char* mode);
TimerHUDVisualMode ts_get_hud_visual_mode_kind(void);
TimerHUDVisualMode ts_session_get_hud_visual_mode_kind(const TimerHUDSession* session);
const char* ts_get_hud_visual_mode(void);
const char* ts_session_get_hud_visual_mode(const TimerHUDSession* session);
const char* ts_get_log_filepath(void);
const char* ts_session_get_log_filepath(const TimerHUDSession* session);
bool ts_set_hud_visual_mode(const char* mode);
bool ts_set_hud_visual_mode_kind(TimerHUDVisualMode mode);
bool ts_session_set_hud_visual_mode(TimerHUDSession* session, const char* mode);
bool ts_session_set_hud_visual_mode_kind(TimerHUDSession* session, TimerHUDVisualMode mode);

#ifdef __cplusplus
}
#endif

#endif // TIMER_HUD_CONFIG_H
