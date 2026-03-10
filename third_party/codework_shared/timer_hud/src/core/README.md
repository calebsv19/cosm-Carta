# TimerHUD Core

Low-level timing primitives that power the HUD.

| File | Responsibility |
| --- | --- |
| `timer.h/c` | Represents a single timing block (start/stop, running stats). |
| `timer_manager.h/c` | Registry of timers, lookup helpers, and aggregation logic. |
| `frame_tracker.h/c` | Tracks per-frame timings and coordinates sampling (note: stray `frame_trackero` artifact is slated for removal). |
| `time_utils.h/c` | Platform utilities for high-resolution timing. |

These files are UI-agnostic and can be embedded without the HUD renderer.
