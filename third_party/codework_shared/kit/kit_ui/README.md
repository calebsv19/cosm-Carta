# kit_ui

Version 0.14.0 adds the optional `kit_ui_surface.h` snapshot bridge. Hosts collect
visible, clipped controls with semantic `(domain, uint64 value)` keys; opaque IDs
remain stable across row reorder. A bounded FIFO delivers at most one activation
per host frame. Modal scope changes cancel old owners while swallowing their
outstanding release. The 256-control limit is explicit and invalid collections
leave the published snapshot intact. Drawing, labels, application actions, text
entry, gestures and lifecycle policy remain host-owned. See
[the interaction contract](../../docs/UI_INTERACTION_CONTRACT.md).

`kit_ui` is the shared immediate-mode widget and layout helper kit built on top of `kit_render`.

It provides pane-hostable UI primitives that render through shared draw commands instead of owning any pane or runtime lifecycle.

## Current Scope (Immediate-Mode Helpers)

This module now provides:

- stack layout helpers (`vstack`, `hstack`)
- label drawing
- button drawing
- richer button spec/state/layout/style helpers for selected, pressed, focused, and variant-aware controls
- rounded/compact button appearance presets for HUD-style control surfaces
- HUD-style floating surface/button-row layout helpers with alpha-preserving
  dark floating defaults and nested-corner inset helpers
- optional SDL rounded-surface/button/readout/scrollbar adapter helpers through
  `kit_ui_sdl.h`
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
- clip-stack helpers layered onto the active `kit_render` frame
- text-measure and text-fit helpers that reuse `kit_render` metrics when available and fall back to lightweight estimates otherwise
- HUD control-row geometry and default alpha-aware chrome styles while callers
  keep action dispatch and domain policy
- nested rounded-surface radius math for inset control rows
- optional SDL rounded-surface drawing helpers for plain SDL hosts that are not
  yet submitting those overlays through `kit_render` command frames

`kit_ui` does not own:

- pane lifecycle
- event loop ownership
- retained widget trees
- application-wide focus, text entry, clipboard or IME ownership
- layout document loading
- settings or action persistence
- application-specific behavior
- mandatory SDL or event-loop dependencies in the default `kit_ui` library
- app theme-preset persistence or app-specific token-to-surface policy

## Contract Notes

- `KitUiContext.render_ctx` is borrowed and must outlive the UI context.
- All draw helpers are immediate-mode frame helpers. Borrowed label/text pointers only need to remain valid through the current render frame command consumption.
- `kit_ui` writes commands into the caller-owned `KitRenderCommandBuffer` attached to the active `KitRenderFrame`; drawing does not retain widget state across frames. The separate optional interaction context retains only focus/capture IDs and press state.
- Clip-stack depth is bounded by `KIT_UI_CLIP_STACK_MAX`.
- `kit_ui_fit_text_to_rect(...)` chooses the largest full-fit tier first, then falls back to the smallest height-fitting tier with ellipsis truncation.
- The Vulkan validation harness is a host-side debug harness, not part of the shared widget contract itself.

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
12. additive richer button semantics (`kit_ui_button_*`, `kit_ui_draw_button_spec(...)`) so hosts can share selected/pressed/focused/variant-aware button styling without inventing app-local contracts first
13. additive 1px button-outline hardening so shared button borders avoid corner overrun artifacts across snapped pixel densities by emitting edge rects instead of endpoint-sensitive line segments
14. additive rounded/compact button appearance presets (`kit_ui_button_appearance_*`, `kit_ui_draw_button_spec_appearance(...)`) for HUD-style transport controls without changing existing button callers
15. additive HUD button-row layout helpers (`kit_ui_hud_button_row_*`) plus
    alpha-aware floating HUD style defaults for compact transport/control
    surfaces
16. optional SDL adapter helpers (`kit_ui_sdl_*`) for hosts that still draw
    active overlays through `SDL_Renderer` instead of a retained `kit_render`
    backend
17. additive corner/inset helpers (`kit_ui_corner_radius_for_inset(...)`,
    `kit_ui_corner_radius_clamp(...)`, `kit_ui_rect_inset(...)`) so nested HUD
    controls can match an outer rounded panel by deriving the inner radius from
    `outer_radius - inset`

## Planned Growth

1. extend adoption of the optional focus and keyboard-navigation helpers
2. add binding adapters for settings/action/telemetry keys
3. add simple row/list helpers for inspectors
4. remain the common control surface for settings, graph inspectors, and debug panes
5. keep app-specific button preference layers above the shared `kit_ui_button_*` semantic contract unless a second host proves a reusable policy

## HUD Button Rows and SDL Adapter

Use `kit_ui_hud_button_row_layout(...)` when a host needs a compact floating
control bar with fixed-size buttons and an optional readout lane. The helper
solves the rounded panel, button rects, and readout rect while preserving the
caller-owned button labels, enabled state, selected state, and action dispatch.

Use `kit_ui_hud_style_dark_floating(...)` for the current translucent dark HUD
chrome. The style stores alpha directly in `KitRenderColor.a`, and
`kit_ui_color_with_alpha(...)` provides a small alpha override helper for hosts
that need to tune opacity without changing RGB values.

Hosts may also adapt an app-selected `core_theme` / custom palette into
`KitUiHudStyle` before drawing. In that shape, `kit_ui` still owns the style
fields, alpha-aware layout, and corner math, while the host owns which theme
preset is active and how app-local custom tokens map to HUD surfaces. DataLab
is the current proving host for this pattern: its session and playback HUDs use
shared HUD chrome, but DataLab owns file navigation, playback policy, and
theme/custom-palette persistence.

For nested HUD rows, derive control/readout radius with
`kit_ui_hud_button_row_control_corner_radius(...)` after the row config is
filled. This applies the shared corner rule: inner rounded controls match the
outer panel by using `panel_radius - row_pad`, then clamping to the available
control dimensions. That keeps the panel, padding, buttons, and readout visually
aligned across scale changes and narrow responsive layouts.

Plain SDL hosts can include `kit_ui_sdl.h` and compile `src/kit_ui_sdl.c` as an
optional adapter. The adapter draws rounded panel/buttons/readouts through
`SDL_Renderer`, preserves alpha on every fill, and delegates text measuring and
clipped text drawing back to the host through callbacks. This keeps font/cache
ownership app-local while sharing the HUD/button chrome.

This SDL adapter is intentionally a bridge, not a replacement for
`kit_render`: hosts that already submit overlay frames through `kit_render`
should continue to use command-frame UI helpers. The adapter exists for active
plain-SDL overlays where adopting the shared HUD expression first is cleaner
than forcing a renderer migration.

`kit_ui_sdl_scrollbar_layout(...)` and `kit_ui_sdl_draw_scrollbar(...)` mirror
the shared command-frame scrollbar geometry (6px track inset 8px from the
right edge and a 10% minimum thumb) for direct SDL hosts. The host keeps scroll
offset and pointer routing; `kit_ui` owns the common visual geometry.

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

## Button Appearance Presets

Use `kit_ui_button_appearance_preset(...)` with
`kit_ui_draw_button_spec_appearance(...)` when a host needs rounded or compact
button chrome while keeping action behavior app-owned.

Current presets:

- `KIT_UI_BUTTON_APPEARANCE_DEFAULT`: square legacy-compatible geometry values.
- `KIT_UI_BUTTON_APPEARANCE_ROUNDED`: rounded general-purpose controls.
- `KIT_UI_BUTTON_APPEARANCE_COMPACT_ROUNDED`: tighter HUD/control-bar buttons.
- `KIT_UI_BUTTON_APPEARANCE_PILL`: large radius that clamps to the control
  bounds at draw time.

The appearance struct is intentionally editable by callers: radius, border
thickness, and padding can be adjusted per host without adding app-specific
policy to `kit_ui`.

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

## Rounded Vulkan Image Gate (0.11.3)

```sh
make -C shared/kit/kit_ui KIT_RENDER_ENABLE_VK=1 test-rounded-vk
```

This bounded host-side test renders actual Vulkan images and compares pixels
against an independent rounded-shape oracle at 1x and 2x. It covers square and
rounded bounds, radius clamping/pills, clipping, alpha, nested borders, the
existing compact shared button appearance, forced vertex-buffer growth, frame
fence reuse, and resize recovery. Captures are written under `build/`. A Vulkan
surface and the Khronos validation layer must be available; this gate is
separate from the display-free `make test` contract tests.

The 0.11.3 harness build also includes and links sibling `vk_runtime`. The shared
button API and default appearance remain unchanged. The rounded Vulkan path
requires `kit_render` 0.14.5 and `vk_renderer` 1.4.0; the SDL adapter is unchanged.

## Direct SDL Appearance Adapter (0.12.0)

`kit_ui_sdl_draw_button_spec_appearance` expresses the existing
`KitUiButtonSpec`, `KitUiButtonTheme`, and `KitUiButtonAppearance` contract in
plain SDL hosts. It reuses the shared state resolver for idle, hover, selected,
pressed, focused, and disabled colors, draws nested rounded outline/fill geometry,
and centers measured captions through the host's synchronous `KitUiSdlTextApi`.
Scale radius and border once into drawable pixels; text callbacks use top-left
origins. Labels need to remain valid only through the draw call. The older HUD
SDL helper remains available with its existing fill-only behavior. Neither the
generic kit archive nor its default tests acquire an SDL dependency.

Run `make test-sdl-appearance` for 1x/2x software-surface pixel, state, caption,
clipping, narrow-control, and invalid-input checks. This optional adapter preserves
SDL rasterization; a Vulkan compatibility canvas that uploads those pixels is
still CPU-composed UI. Input routing, focus ownership, action policy, persistence,
and renderer lifetime stay with the host.

## Focus and pointer capture (0.13.0)

The optional `kit_ui_interaction.h` context owns focus and press-origin capture
for registered buttons, with normalized events, ordered Tab traversal and
release activation. Hosts own coordinates, scope changes, text entry and actions.
The generic archive has no SDL dependency; `kit_ui_interaction_sdl.h` is an
optional adapter. See [the interaction contract](../../docs/UI_INTERACTION_CONTRACT.md).

Patch 0.13.1: Clamp the focus marker inside positive-size controls, including tiny control bounds; optional SDL declarations preserve C linkage in C++ hosts.

The 0.14.1 surface patch supports C++ linkage and optional returned control storage; callers may register geometry without retaining the returned opaque handle.


## Bounded text editing and modal focus (0.15.0)

Optional `kit_ui_text_edit.h` borrows caller-owned UTF-8 storage and owns byte
positions at Unicode scalar boundaries, selection and bounded preedit composition.
Insertion and capacity/format failure are transactional. Selection replacement,
Home/End, Shift movement, Backspace/Delete and select-all are shared mechanics.
`kit_ui_text_edit_sdl.h` is an optional SDL normalization/clipboard adapter; it
returns Enter/Escape intents and stages composition without changing committed
text. Hosts own field eligibility, commit/cancel, SDL text-input sessions,
wrapping/hit geometry, filtering and persistence. UTF-8 scalars are not grapheme
clusters; complex shaping and native candidate-window acceptance remain later proof.

`kit_ui_focus_scope.h` supports one explicit modal over one host scope, preserves
semantic button focus on entry and restores it only if its enabled target is
still published on return. Nested modal changes fail explicitly. Scope changes
retain outstanding button release ownership. Text takeover clears button focus
and armed-key owner. Lifecycle cancellation clears restoration intent.

Run `make test test-text-edit-sdl`. The generic archive has no SDL dependency.

### 0.15.1 bounded editor correction

Selection extraction rejects output in the borrowed editing buffer; preedit ranges must fit its Unicode scalar count. Deterministic SDL clipboard fixtures verify failed cut and invalid/oversized paste preserve text without writing the system clipboard.

### 0.16.0 shared text presentation

Optional `kit_ui_text_presentation.h` measures scalar-safe text rows, hard wraps and explicit newlines, caret/selection/preedit geometry, caret-follow scrolling and shared hit mapping. The caller owns bounded presentation storage (8192 display bytes / 256 rows) through submission. Failed layout preserves the previous view; queued adapter failures restore command count and nested clip depth. `kit_ui_text_presentation_sdl.h` is optional and preserves SDL clip/color/blend state. Hosts supply actual font measurement and theme colors. No Core, domain commit, persistence, OS input-session or shaping policy is moved. See `docs/UI_TEXT_PRESENTATION_CONTRACT.md` and `make test-text-presentation`.

## Focus order and native caret anchoring (`v0.17.0`)

Optional `kit_ui_focus_order.h` supplies semantic mixed field/button traversal
for caller-defined visible order and scope. Hosts still bind text and dispatch
actions; existing button-only surfaces remain compatible. The optional
`kit_ui_native_text_sdl.h` adapter anchors native candidate UI to measured caret
geometry in SDL window coordinates while the host retains Start/Stop and focus
policy. `make test-focus-order` and `make test-native-text-sdl` qualify mechanics;
the native test uses a real window but does not certify human IME acceptance.

## 2026-10-05 window lifecycle

0.18.0 adds optional SDL window observation, explicit logical/render coordinate mapping, F11 desktop-fullscreen handling and an opt-in native lifecycle qualification driver. Lifecycle transitions cancel stale button ownership through the existing SDL interaction adapter; the host keeps the window, event loop and domain state.

See [shared window contract](../../docs/UI_WINDOW_LIFECYCLE_CONTRACT.md).
