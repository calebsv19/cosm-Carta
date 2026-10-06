#ifndef KIT_UI_WINDOW_PROBE_SDL_H
#define KIT_UI_WINDOW_PROBE_SDL_H
#include "kit_ui_window_sdl.h"
/* Explicit opt-in qualification driver; inactive unless the host passes the
 * CODEWORK_WINDOW_LIFECYCLE_PROOF output directory. Never routine UI policy. */
typedef struct KitUiWindowProbe {
    const char *directory, *program;
    int phase, captured, status, width, height;
    uint64_t started_ms, phase_ms, phase_frames, capture_frame;
} KitUiWindowProbe;
typedef int (*KitUiWindowProbeCapture)(void *user, const char *path);
void kit_ui_window_probe_init_sdl(KitUiWindowProbe *probe, const char *program);
/* 0 inactive/running, 1 complete, -1 failed; hosts tick while suspended too. */
int kit_ui_window_probe_tick_sdl(KitUiWindowProbe *probe, SDL_Window *window,
    uint64_t presented_frames, KitUiWindowProbeCapture capture, void *user);
#endif
