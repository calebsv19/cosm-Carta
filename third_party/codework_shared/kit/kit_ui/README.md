# kit_ui

`kit_ui` is the shared widget and layout helper kit built on top of `kit_render`.

It provides pane-hostable UI primitives that render through shared draw commands instead of owning any pane or runtime lifecycle.

## Current Scope (Immediate-Mode Helpers)

This module now provides:

- stack layout helpers (`vstack`, `hstack`)
- label drawing
- button drawing
- checkbox drawing
- slider drawing
- scrollbar drawing for viewport-style panels
- segmented control drawing
- input-state, hit-testing, and simple behavior evaluation helpers
- theme-driven colors through `kit_render` and `core_theme`
- font-role-aware text through `kit_render` and `core_font`

The implementation is still intentionally immediate-mode. The goal is reusable, testable shared controls for graph inspectors and tool panels, not a full retained-mode framework.

## Boundary

`kit_ui` owns:

- widget rendering helpers
- widget visual states
- simple layout allocation helpers
- reusable control composition
- light interaction helpers for shared tools

`kit_ui` does not own:

- pane lifecycle
- event loop ownership
- layout document loading
- settings or action persistence
- application-specific behavior

## Progress

Implemented now:

1. base UI context and style defaults
2. stack layout allocation helpers
3. button, checkbox, slider, scrollbar, segmented, and label rendering helpers
4. command emission through `kit_render`
5. input-state, hit-testing, and simple behavior evaluation helpers
6. a live Vulkan validation harness for interactive widget checks
7. additive custom label/button drawing variants so callers can pick font role and text size tier when the default control text is too rigid
8. additive theme-scale style sync via `kit_ui_style_apply_theme_scale(...)` so contexts can derive density from the active shared theme preset
9. additive top-anchor content-height helper for virtualized row lists (`kit_ui_scroll_content_height_top_anchor(...)`)
10. additive clip-stack helpers with nested intersection and pixel-snapped clip rects (`kit_ui_clip_push(...)`, `kit_ui_clip_pop(...)`, `kit_ui_clip_stack_reset(...)`) for pane-safe scrolling and overlap prevention
11. additive text-measure and text-fit helpers (`kit_ui_measure_text(...)`, `kit_ui_fit_text_to_rect(...)`) for reusable width/height-aware label fitting with tier selection and ellipsis fallback

## Planned Growth

1. add focus and keyboard-navigation helpers
2. add binding adapters for settings/action/telemetry keys
3. add simple row/list helpers for inspectors
4. remain the common control surface for settings, graph inspectors, and debug panes

## Theme-Scale Style Sync

`kit_ui_context_init(...)` now seeds `KitUiStyle` from the active theme scale in the attached `KitRenderContext`.

Apps that switch theme presets at runtime can re-sync density by calling:

```c
kit_ui_style_apply_theme_scale(&ui_ctx);
```

This keeps spacing and control sizing aligned with shared theme presets instead of relying only on hardcoded app-local constants.

## Text Fit Helpers

Use `kit_ui_fit_text_to_rect(...)` when control text needs to adapt to tight bounds without app-local heuristics.

The helper:

1. picks the largest candidate text tier that fits the target rect
2. falls back to measured ellipsis fitting when width is constrained
3. reports the final tier/metrics/truncation through `KitUiTextFitResult`

This keeps list rows, graph nodes, and chip/button labels consistent across apps.

## Build

```sh
make -C shared/kit/kit_ui
```

## Test

```sh
make -C shared/kit/kit_ui test
```

## Validation Harness

Build the live Vulkan UI harness:

```sh
make -C shared/kit/kit_ui clean validation-harness KIT_RENDER_ENABLE_VK=1
```

Run it:

```sh
./shared/kit/kit_ui/build/kit_ui_vk_validation
```

Expected behavior:

1. a left control panel with a title, segmented mode switcher, button, checkbox, slider, and status text
2. a scrollable event log panel on the lower left; use the mouse wheel while the cursor is over that panel
3. a right visualization panel that changes between bars, trend lines, and mixed mode based on the segmented control
4. clicking `Run Action` causes a brief visual accent change
5. clicking `Enable Overlay` toggles the `LIVE OVERLAY` label
6. dragging the slider changes the visualization intensity and the status percentage text

Press `Esc` or close the window to exit.
