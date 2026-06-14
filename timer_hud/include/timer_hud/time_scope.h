#ifndef TIMER_HUD_TIME_SCOPE_H
#define TIMER_HUD_TIME_SCOPE_H

#include <stdbool.h>

#include "timer_hud/timer_hud_config.h"
#include "timer_hud/timer_hud_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

TimerHUDSession* ts_session_create(void);
void ts_session_destroy(TimerHUDSession* session);
TimerHUDSession* ts_default_session(void);

// --- Backend wiring ---
void ts_register_backend(const TimerHUDBackend* backend);
void ts_session_register_backend(TimerHUDSession* session, const TimerHUDBackend* backend);

typedef struct TimerHUDInitConfig {
    const char* program_name;
    const char* output_root;
    const char* settings_path;
    const char* default_settings_path;
    bool seed_settings_if_missing;
} TimerHUDInitConfig;

// Optional overrides (all are interpreted relative to ts_output_root unless absolute).
void ts_set_settings_path(const char* path);
void ts_set_output_root(const char* path);
void ts_set_program_name(const char* name);
bool ts_seed_settings_file(const char* default_settings_path, const char* runtime_settings_path);
bool ts_apply_init_config(const TimerHUDInitConfig* config);
void ts_session_set_settings_path(TimerHUDSession* session, const char* path);
void ts_session_set_output_root(TimerHUDSession* session, const char* path);
void ts_session_set_program_name(TimerHUDSession* session, const char* name);
bool ts_session_apply_init_config(TimerHUDSession* session, const TimerHUDInitConfig* config);

// --- Initialization / Shutdown ---
void ts_init(void);              // Call once on app start
void ts_shutdown(void);          // Call on app exit
void ts_session_init(TimerHUDSession* session);
void ts_session_shutdown(TimerHUDSession* session);

// --- Timer Controls ---
void ts_start_timer(const char* name);  // Begin named timer
void ts_stop_timer(const char* name);   // End named timer
void ts_record_duration_ms(const char* name, double duration_ms);
void ts_session_start_timer(TimerHUDSession* session, const char* name);
void ts_session_stop_timer(TimerHUDSession* session, const char* name);
void ts_session_record_duration_ms(TimerHUDSession* session, const char* name, double duration_ms);

// --- Frame Markers (optional) ---
void ts_frame_start(void);       // Mark beginning of frame (future use)
void ts_frame_end(void);         // Mark end of frame and flush logs/stats
void ts_session_frame_start(TimerHUDSession* session);
void ts_session_frame_end(TimerHUDSession* session);

// --- Event Tagging ---
void ts_emit_event(const char* tag);  // Optional Phase 4
void ts_session_emit_event(TimerHUDSession* session, const char* tag);

// --- HUD Rendering ---
void ts_render(void);
void ts_session_render(TimerHUDSession* session);

// --- HUD Controls ---
// Returns true only when the HUD consumes the pointer event.
bool ts_handle_pointer_down(int x, int y, int button);
bool ts_session_handle_pointer_down(TimerHUDSession* session, int x, int y, int button);

#ifdef __cplusplus
}
#endif

#endif // TIMER_HUD_TIME_SCOPE_H
