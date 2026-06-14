# TimerHUD Core

Low-level timing primitives that power the HUD.

| File | Responsibility |
| --- | --- |
| `timer.h/c` | Represents a single timing block (start/stop, owned timer name, running stats). |
| `timer_manager.h/c` | Registry of timers, lookup helpers, and aggregation logic. |
| `session.h/c` | Session-owned runtime state for timer registries, backend/log/event ownership, and default-session compatibility. |
| `session_fwd.h` | Shared internal forward declaration for `TimerHUDSession` to keep cross-module includes warning-free. |
| `frame_tracker.h/c` | Tracks per-frame timings and coordinates sampling. |
| `time_utils.h/c` | Platform utilities for high-resolution timing. |

These files are UI-agnostic and can be embedded without the HUD renderer.
