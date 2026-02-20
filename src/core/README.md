# core Module

This module contains foundational runtime utilities used across subsystems.

## Files

- `input.c`: Per-frame input state init/reset and SDL event translation.
- `log.c`: Consistent info/error logging helpers.
- `time.c`: Monotonic timing helper for frame/update timing.

## How It Connects

- Used by app loop, loaders, and render/route modules for stable frame logic and diagnostics.
- Keeps low-level concerns centralized so domain modules stay focused on map behavior.

