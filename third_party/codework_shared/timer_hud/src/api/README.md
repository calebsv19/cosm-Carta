# TimerHUD API

Public wrapper exposed to host applications. Keeps the core types opaque and
provides a stable function surface.

| File | Responsibility |
| --- | --- |
| `time_scope.h/c` | C API used by host apps: initialise/shutdown TimerHUD, manage session-scoped runtime state, start/stop timers, request HUD rendering, and flush logs. |

Preferred public include surface:
- `time_scope.h`
- `timer_hud_config.h`

Host code should not rely on the private settings-loader header for normal
runtime control.
