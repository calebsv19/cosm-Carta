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
- shared font/theme authoring panel model:
  - standard font/theme/custom-stub button IDs
  - responsive panel layout derivation
  - button hit-testing
  - core font/theme preset mapping
  - button-to-action classification for host adapters
- shared render composition helpers:
  - pane/font-theme overlay-visibility policy helper
  - frame clear helper with theme-token resolve
  - splitter preview draw helper
- shared host-adapter seam helpers:
  - derive-frame helper for per-frame authoring state snapshot
  - submit-frame helper for draw + rebuild-ack sequencing
  - conflict-matrix coverage extended in kit tests for modifier-suppressed pane triggers and chord collision cases

This shared surface is already broadly adopted across the current proving hosts. The roadmap is no longer about first migration into one app; it is about keeping the shared boundary narrow and stable while hosts own persistence, preview mutation, shell parity, and app-specific pane/module behavior.

## Next (additive only)
- keep host-adoption docs aligned as new apps roll onto the shared surface
- add only generic authoring controls that repeated hosts actually share
- keep host adapters thin and callback-only around the shared `ui` seam
- defer any custom-theme editor, module insertion flow, or host persistence growth unless it proves generic across multiple adopters

## Explicit Non-Goals For This Lane

- no shared accepted-only persistence layer
- no shared shell/theme parity enforcement
- no shared custom-theme editor
- no app-specific module topology or module insertion behavior
