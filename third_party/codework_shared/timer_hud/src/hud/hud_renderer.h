#ifndef TIMESCOPE_HUD_RENDERER_H
#define TIMESCOPE_HUD_RENDERER_H

#include "../core/session_fwd.h"
#include "timer_hud/timer_hud_backend.h"

void hud_set_backend(TimerHUDSession* session, const TimerHUDBackend* backend);
void hud_init(TimerHUDSession* session);
void hud_shutdown(TimerHUDSession* session);

// Render all timer HUD elements each frame
void hud_render(TimerHUDSession* session);

#endif // TIMESCOPE_HUD_RENDERER_H
