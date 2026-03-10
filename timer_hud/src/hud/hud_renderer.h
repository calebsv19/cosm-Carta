#ifndef TIMESCOPE_HUD_RENDERER_H
#define TIMESCOPE_HUD_RENDERER_H

#include "timer_hud/timer_hud_backend.h"

void hud_set_backend(const TimerHUDBackend* backend);
void hud_init(void);
void hud_shutdown(void);

// Render all timer HUD elements each frame
void ts_render(void);

#endif // TIMESCOPE_HUD_RENDERER_H
