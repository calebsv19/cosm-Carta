# kit_pane

`kit_pane` is the shared pane-shell presentation kit built on top of `kit_render`.

It provides pane chrome draw helpers and authoring-friendly splitter visuals without owning pane topology or workspace policy.

## Current Scope (Scaffold / Chrome Baseline)

This initial scaffold provides:

1. pane chrome rendering (`kit_pane_draw_chrome(...)`),
2. default pane style presets (`kit_pane_style_default(...)`),
3. splitter rendering for hover/active states (`kit_pane_draw_splitter(...)`),
4. reusable splitter hover/drag interaction state layered over `core_pane` (`KitPaneSplitterInteraction`).

## Boundary

`kit_pane` owns:

1. pane shell presentation,
2. authoring-mode visual affordances,
3. UI-level pane chrome consistency,
4. host-side splitter hover/drag interaction state.

`kit_pane` does not own:

1. pane topology solve or drag math (`core_pane`),
2. module lifecycle or assignment policy,
3. workspace preset persistence.

## Progress

Implemented now:

1. baseline style contract (`KitPaneStyle`),
2. pane chrome contract (`KitPaneChrome`),
3. chrome and splitter draw helpers,
4. reusable splitter interaction controller for hover/begin-drag/update-drag/end-drag,
5. null-backend test coverage for command emission and splitter interaction state.

## Contract Notes

1. `KitPaneChrome.title` is borrowed text; callers must keep the pointed string alive until queued frame commands are consumed.
2. pane id labels are generated through a shared static ring buffer sized for ordinary per-frame command emission, not persistent storage.
3. hover updates intentionally no-op while drag is active so hosts do not churn hover state mid-drag.
4. cached-hit entry points only consume caller-supplied `CorePaneSplitterHit` geometry; topology validation stays in `core_pane`.

## Recent Changes (`v0.3.1`)

1. Truth-locked borrowed title, generated id-label, and drag-phase hover contracts.
2. Hardened drag updates against non-finite points and stale cached-hit metadata so failed updates do not poison later drag state.
3. Expanded tests across style clamping, chrome/splitter visual-state semantics, invalid draw requests, and cached-hit interaction edge cases.

## Previous Changes (`v0.3.0`)

1. Added cached-hit interaction entry points so hosts can drive hover/drag from explicit splitter registries (`kit_pane_splitter_interaction_set_hover_from_hits(...)` and `kit_pane_splitter_interaction_begin_drag_from_hits(...)`).
2. This matches the IDE-style divider registry model without forcing editor-specific split-tree behavior into shared code.

## Previous Changes (`v0.2.0`)

1. Added baseline pane chrome/splitter rendering helpers.
2. Added reusable `KitPaneSplitterInteraction` hover/drag state.
3. Added null-backend interaction coverage.

## Planned Growth

1. header action-slot rendering helpers,
2. explicit cursor/state visual hooks,
3. structural overlays for authoring labels and constraints,
4. tighter integration helpers for workspace host runtime.

## Build

```sh
make -C shared/kit/kit_pane
```

## Test

```sh
make -C shared/kit/kit_pane test
```

## Composition (`v0.4.0`)

Optional `kit_pane_composition.h` adds stable pane identity, validated atomic
shell/header/content derivation, viewport clipping, half-open hit regions and
press ownership/cancellation. An optional SDL content-clip adapter restores
existing clipping. Hosts keep layout solving, module purpose and appearance.
See `../../docs/UI_PANE_COMPOSITION_CONTRACT.md`. Run `make test` and
`make test-composition-sdl`; the latter checks exact clipped pixels at 1x/2x.

## 0.5.0 pane host behavior

`kit_pane_host.h` adds stable pane mount/unmount/resize dispatch, pointer capture
and focus invalidation on takeover, and drag-sized `core_layout` transactions
that preserve an existing outer authoring session. Header action slots reserve
bounded title space and use the host's existing `kit_ui` surface for activation.
The kit retains no domain payload, topology, label storage or SDL resources.
Requires core_layout 0.2.1; `make test-host` exercises this contract.
