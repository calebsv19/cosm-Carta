# TimerHUD Renderer

Draws the on-screen heads-up display.

| File | Responsibility |
| --- | --- |
| `hud_snapshot.h/c` | Builds a read-only render snapshot from session-owned runtime state and HUD settings. |
| `hud_renderer.h/c` | Lays out timer blocks, text labels, and graph primitives through the host-provided backend draw hooks. |
