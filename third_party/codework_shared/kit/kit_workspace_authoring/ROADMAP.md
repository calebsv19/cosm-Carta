# kit_workspace_authoring Roadmap

## Current
- host-agnostic key trigger mapping and `Alt+C+V` entry chord helper
  - entry chord now requires `Alt` without `Shift/Ctrl/GUI` modifiers
- authoring pane-overlay active-state helper
- callback-driven action execution helper with close-picker policy hook
- callback-driven text-size step apply/adjust/reset helpers
- root-bounds helper for pane layout/view solve inputs
- shared overlay UI primitives:
  - top-bar overlay button layout
  - overlay button hit-testing
  - pane drop-intent and ghost-rect geometry helpers
  - HUD overlay-button draw composition via `kit_render`
- shared render composition helpers:
  - pane/font-theme overlay-visibility policy helper
  - frame clear helper with theme-token resolve
  - splitter preview draw helper
- shared host-adapter seam helpers:
  - derive-frame helper for per-frame authoring state snapshot
  - submit-frame helper for draw + rebuild-ack sequencing
  - conflict-matrix coverage extended in kit tests for modifier-suppressed pane triggers and chord collision cases

## Next (additive only)
- publish reusable host attach checklist lane:
  - codify host obligations for theme preset/text zoom state handoff and persistence
  - codify top-level picker/shell theming parity requirement (not overlay-only reactivity)
  - keep host adapters thin and callback-only around shared `ui` seam
