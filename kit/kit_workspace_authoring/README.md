# kit_workspace_authoring

`kit_workspace_authoring` is a shared host-agnostic authoring-runtime kit.

It standardizes:
1. authoring key trigger mapping and entry-chord checks (`Alt+C+V`, with `Shift/Ctrl/GUI` excluded),
2. pane-overlay active-state checks,
3. host-callback action execution wrappers,
4. host-callback text-size step apply/adjust/reset helpers,
5. viewport root-bounds helper for pane-layout calls,
6. shared overlay UI primitives (top-bar buttons, hit-testing, drop-intent geometry, and HUD button draw composition),
7. shared font/theme authoring panel model primitives (responsive layout, hit IDs, preset mappings, and action classification),
8. shared render composition helpers (surface clear token resolve, splitter preview draw),
9. shared derive/submit adapter seam helpers for host render loop wiring.

The kit is now broadly adopted. Its job is to standardize shared authoring interaction seams and overlay primitives, not to absorb host-specific preview mutation, persistence, or shell behavior.

## Host Integration Contract (Theme + Font)

For host programs wiring this kit:
1. Keep **entry/trigger/action routing** on shared helpers (`Alt+C+V`, trigger mapping, action dispatch wrappers).
2. Treat `text_zoom_step` and theme preset as **host-owned state**; shared kit does not own host persistence.
3. Ensure top-level host surfaces (startup picker, reopen picker, or equivalent shell UI) consume the same active theme preset used by authoring overlay so visual changes are externally provable.
4. Persist and reload theme preset + text zoom via host runtime prefs so relaunch behavior is deterministic.
5. Apply/cancel semantics should be baseline-driven at host boundary:
   - `Apply` commits current text/theme as new entry baseline
   - `Cancel` restores entry baseline
6. Any caller-supplied overlay labels remain borrowed text and must outlive frame submission because `kit_render` text commands borrow the pointer.

Recommended minimum host checklist:
1. authoring overlay controls are interactive (`text +/-/reset`, theme preset selection),
2. non-authoring top-level picker/shell chrome reflects active theme preset,
3. relaunch preserves selected theme + text zoom,
4. keybind conflict matrix (`Alt+C+V`, `Tab`, `Enter`, `Esc`) is documented and verified.

Public cross-host adoption reference:
- `/Users/calebsv/Desktop/CodeWork/shared/docs/WORKSPACE_AUTHORING_HOST_ADOPTION_GUIDE.md`

## Boundary

`kit_workspace_authoring` owns:
1. app-neutral authoring interaction glue logic
2. callback-driven wrappers for host action and text-step routes
3. host-agnostic overlay UI primitives used by proving/host adapters
4. shared font/theme panel layout, hit-testing, preset mapping, and standard action IDs
5. shared render seam helpers for host derive/submit integration

`kit_workspace_authoring` does not own:
1. renderer/window backend behavior
2. app-specific module-picker data models
3. pane topology/runtime persistence semantics
4. host persistence or live renderer state mutation for theme/font changes
5. top-level shell parity or visual acceptance policy
6. custom theme editor UX beyond the current shared status stubs

## Current Contract Notes

1. Entry-chord semantics stay `Alt+C` then `Alt+V`, with `Shift`, `Ctrl`, and `GUI` excluded from the entry chord itself.
2. Non-entry pane trigger mapping currently suppresses `Shift` and `Alt` for pane-action keys; `Ctrl` and `GUI` are currently passed through and are now truth-locked as the shared contract.
3. Overlay hit tests, drop-intent hit tests, and font/theme button hit tests treat right and bottom rect boundaries as inclusive.
4. `kit_workspace_authoring_root_bounds(...)` clamps negative dimensions to zero-sized bounds.
5. Shared custom-theme buttons are status stubs only; create/edit behavior remains host-owned.
6. Splitter-preview and overlay draw helpers are immediate-mode helpers only; they do not own retained overlay state or persistence.

## Internal source lanes
Current internal source layout is split by role:
1. `src/` root: input/runtime helper glue
2. `src/ui/`: overlay UI primitives and geometry helpers

## Build

```sh
make -C shared/kit/kit_workspace_authoring
```

## Test

```sh
make -C shared/kit/kit_workspace_authoring test
```
