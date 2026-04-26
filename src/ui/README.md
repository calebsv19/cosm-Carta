# ui Module

This module provides reusable UI helpers for runtime overlays and text rendering.

## Files

- `debug_overlay.c`: Debug overlay state tracking and render stub/hooks.
- `font.c`: Host-facing facade for text configuration and draw/measure entrypoints.
- `font_bridge.c`: Active logical-font cache plus shared external text-runtime source registration.
- `text_draw.c`: Thin runtime bridge to shared `kit_render_external_text.*` for Vulkan text draw/measure, with clipped draw intentionally kept local until the shared runtime exposes source-rect clipping semantics.

## How It Connects

- Used by app HUD/header/status rendering.
- Supports clear runtime diagnostics and readable interaction feedback.
