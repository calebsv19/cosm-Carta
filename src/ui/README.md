# ui Module

This module provides reusable UI helpers for runtime overlays and text rendering.

## Files

- `debug_overlay.c`: Debug overlay state tracking and render stub/hooks.
- `font.c`: Font setup, text measurement, and text draw/cache helpers for active renderer backend.

## How It Connects

- Used by app HUD/header/status rendering.
- Supports clear runtime diagnostics and readable interaction feedback.

