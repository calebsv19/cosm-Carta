# TimerHUD Core Source

The main TimerHUD implementation is split into cohesive modules so the HUD can
be embedded piecemeal.

| Subdirectory | Responsibility |
| --- | --- |
| [`core/`](core/README.md) | Timer lifecycle management and frame orchestration. |
| [`api/`](api/README.md) | Public API wrappers exported to host applications. |
| [`config/`](config/README.md) | Settings loader/saver (JSON). |
| [`events/`](events/README.md) | Event tagging utilities. |
| [`history/`](history/README.md) | Aggregated history and statistics helpers. |
| [`hud/`](hud/README.md) | Rendering routines for the on-screen HUD. |
| [`logging/`](logging/README.md) | Logging sinks (CSV/JSON). |

Each subdirectory contains a README with file-level context.
