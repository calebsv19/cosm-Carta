#ifndef TIMER_HUD_TIME_SCOPE_H
#define TIMER_HUD_TIME_SCOPE_H

#include "timer_hud/settings_loader.h"
#include "timer_hud/timer_hud_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Backend wiring ---
void ts_register_backend(const TimerHUDBackend* backend);

// Optional overrides (all are interpreted relative to ts_output_root unless absolute).
void ts_set_settings_path(const char* path);
void ts_set_output_root(const char* path);
void ts_set_program_name(const char* name);
void ts_set_hud_visual_mode(const char* mode);

// --- Initialization / Shutdown ---
void ts_init(void);              // Call once on app start
void ts_shutdown(void);          // Call on app exit

// --- Timer Controls ---
void ts_start_timer(const char* name);  // Begin named timer
void ts_stop_timer(const char* name);   // End named timer

// --- Frame Markers (optional) ---
void ts_frame_start(void);       // Mark beginning of frame (future use)
void ts_frame_end(void);         // Mark end of frame and flush logs/stats

// --- Event Tagging ---
void ts_emit_event(const char* tag);  // Optional Phase 4

// --- HUD Rendering ---
void ts_render(void);

#ifdef __cplusplus
}
#endif

#endif // TIMER_HUD_TIME_SCOPE_H
