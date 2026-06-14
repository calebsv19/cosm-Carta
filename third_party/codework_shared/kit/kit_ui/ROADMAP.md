# kit_ui Roadmap

`kit_ui` is currently a shared immediate-mode control layer on top of `kit_render`.

## Implemented Now

- stack layout helpers
- clip-stack helpers
- label, button, checkbox, slider, scrollbar, and segmented control drawing
- simple input evaluation helpers
- theme-scale style sync
- text measurement and text-fit helpers
- validation harness coverage for live Vulkan-backed checks
- rounded/compact button appearance presets for HUD-style controls
- HUD button-row layout helpers with alpha-aware floating surface defaults
- optional SDL adapter helpers for rounded HUD/button/readout drawing
- nested rounded-corner inset helpers for matching inner control radius to
  outer panel radius and padding

## Deferred

1. keyboard focus and navigation helpers
2. retained row/list helpers for higher-level inspectors
3. settings/action binding adapters
4. richer text input or editor controls
5. host-specific event-loop, pane, or persistence behavior

## Hardening Notes

- Version `0.11.1` hardens HUD corner behavior with shared inset/radius helpers
  so nested button/readout corners derive from the outer panel radius minus row
  padding and clamp to responsive control dimensions. DataLab now proves this
  across both the bottom playback HUD and top-left session HUD, with app-owned
  theme/custom palette adaptation into `KitUiHudStyle`.
- Version `0.11.0` adds renderer-neutral HUD button-row layout helpers,
  alpha-preserving floating HUD style defaults, and an optional `kit_ui_sdl`
  adapter for plain SDL hosts. App-specific actions, playback policy, font
  cache ownership, and event-loop routing remain host-owned.
- Version `0.10.0` adds rounded/compact button appearance presets and
  appearance-aware spec drawing for HUD-style controls. The API is additive and
  leaves existing button-spec callers on their current command path.
- Version `0.9.1` keeps the `0.9.0` button semantics contract intact while hardening 1px shared button borders against corner overrun artifacts on snapped pixel grids.
- Version `0.9.0` adds additive button spec/state/layout/style helpers plus shared spec-driven button drawing without broadening retained-state ownership.
- Large-file decomposition is now partially applied through the extracted `kit_ui_button.c` lane; broader retained-row/editor decomposition remains deferred.
