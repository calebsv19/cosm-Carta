# TimerHUD API

Public wrapper exposed to host applications. Keeps the core types opaque and
provides a stable function surface.

| File | Responsibility |
| --- | --- |
| `time_scope.h/c` | C API used by host apps: initialise/shutdown TimerHUD, start/stop timers, request HUD rendering, and flush logs. |

Include `time_scope.h` from your application to embed TimerHUD without digging
into internal headers.
