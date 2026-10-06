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

## Shared control bridge

- `control_surface.c` / `include/ui/control_surface.h`: caller-owned shared
  semantic surface for workspace navigation and normal hiking/history actions.
  Synchronizes current visible geometry before input and drawing; app action
  bodies stay authoritative. Ownership/geometry changes invalidate presses.
- `control_presentation.c`: shared compact rounded style, native backend
  expression and measured synchronous caption placement through existing fonts.

See `docs/shared_control_adoption.md` for activation/cancellation, scope limits
and qualification. Layout, product data, map/terrain policy and persistence remain
with the app. This adapter does not turn legacy forms into shared text editors.
