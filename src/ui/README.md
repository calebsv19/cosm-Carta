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
  Product-colored legend/status controls supply their fill and outline to this
  same painter. Each control has one surface and one caption; migration must
  remove its legacy chrome rather than painting a generic button over it.
  Group color strips and loading progress remain separate product indicators.

See `docs/shared_control_adoption.md` for activation/cancellation, scope limits
and qualification. Layout, product data, map/terrain policy and persistence remain
with the app. The bounded text adapter owns editing for the declared migrated fields.

- `text_input.c` / `text_presentation.c`: caller-owned bounded shared editor,
  measured clipped selection/preedit/caret, visible native-session ownership and
  window-space caret placement. Product submit/cancel/persistence remains local.
- `control_places.c`: places/service and saved-route semantic inventory and dispatch;
  route controls share direct product bodies with keyboard shortcuts.
- The existing control surface also hosts history modal type/state/preview/save
  buttons and eligible background focus restoration through shared focus scopes.

See `docs/shared_text_modal_adoption.md` for adopted text owners and remaining hosts.

## Completion adapters

- `control_remaining.c`: direct Map HUD, sidebar, metadata and route objective intent.
- `modal_controls.c`: per-modal visible surfaces, inventory invalidation and release routing.
- `text_hosts.c`: pin-name/Ingest per-event ownership above map shortcuts; window notifications never insert text.
- `pane_layout.c` / `panes.c`: core topology, composed geometry/hit and pointer ownership, subtle splitter and persisted ratio policy.
- `window.c` / `window_probe.c`: actual geometry/presentability, F11, domain cancellation and finite actual-loop qualification.
- `app/history/history_picker.c`: separated peak button inventory/presentation; the history host keeps goal creation and persistence.

See `docs/shared_ui_rollout.md`. These reuse the exact vendored shared snapshot;
application-specific policy remains outside the kit. Draws are synchronous and
per-item picker text has retained storage.
