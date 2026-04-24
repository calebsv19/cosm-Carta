# kit_render

`kit_render` is the shared rendering abstraction kit for visual drawing and frame submission.

It sits above `core_*` contracts and below higher-level kits such as `kit_ui` and `kit_graph`.

## Current Scope (Scaffold / Null Backend)

This initial scaffold defines:

- a backend-agnostic render command model
- frame lifecycle entrypoints
- command recording for clear/clip/rect/line/polyline/textured-quad/text
- theme token color resolution through `core_theme`
- font role/tier references through `core_font`

The current implementation is intentionally a null backend that records commands into a caller-owned command buffer. This gives the ecosystem a stable API surface before real GPU backends are attached.

An optional Vulkan bridge skeleton is also present. It is compiled in a stub-safe mode by default and can be enabled for real `shared/vk_renderer` integration with:

```sh
make -C shared/kit/kit_render KIT_RENDER_ENABLE_VK=1
```

## Boundary

`kit_render` owns:

- draw command abstraction
- backend adapter boundary
- clip/transform API shape
- frame recording and submission contracts

`kit_render` does not own:

- panes
- widget logic
- layout documents
- settings/action policy
- persistence formats
- global runtime policy

## Progress

Implemented now:

1. null backend frame lifecycle
2. command recording for clear, clip, rect, line, polyline, textured quad, and text
3. theme color resolution through `core_theme`
4. font-role-aware text command model through `core_font`
5. internal backend adapter split with the null backend routed through backend ops
6. compile-time Vulkan backend skeleton that can attach an external renderer handle
7. explicit backend ownership control for attached native handles plus public context shutdown
8. Vulkan text rendering through shared `core_font` role/tier resolution and SDL_ttf glyph rasterization, uploaded as transient textures through `vk_renderer`
9. bitmap text fallback path retained for resilience when runtime font loading/rasterization fails
10. additive runtime theme/font preset switching through `kit_render_set_theme_preset(...)` and `kit_render_set_font_preset(...)`
11. runtime font-path resilience improvements in the Vulkan text backend so TTF font loading can resolve shared relative font paths when apps are launched from nested working directories
12. additive text measurement API through `kit_render_measure_text(...)`, with TTF-backed metrics on Vulkan and deterministic fallback metrics on null/bitmap paths
13. Vulkan text crispness upgrade: render-scale-aware raster font selection, nearest-filter upload for downscaled raster glyph textures, and logical-size-correct destination sizing for high-DPI/zoom clarity
14. additive shared text-runtime policy resolution through `kit_render_resolve_text_run(...)` so non-`kit_ui` hosts can consume one shared role/tier/zoom/render-scale contract before full render-command adoption
15. additive external Vulkan text runtime through `kit_render_external_text.*` so non-`kit_ui` hosts can reuse shared SDL_ttf font-source registration, per-point-size font caching, persistent uploaded-texture caching, and UTF-8 measure/draw helpers without app-local cache ownership
16. Vulkan `KIT_RENDER_CMD_TEXT` now delegates to that same extracted shared runtime, removing the duplicate internal raster-font cache path and giving bridge hosts and full command-frame hosts one shared SDL_ttf/cache implementation
17. additive wrapped UTF-8 draw support through `kit_render_external_text_draw_utf8_wrapped(...)`, so bridge hosts can reuse the same shared cached Vulkan text runtime for wrapped labels instead of keeping one last app-local wrapped-text path

## Planned Growth

Near-term implementation goals:

1. keep the null backend as a stable test harness
2. add transform stack helpers beyond per-command transforms
3. improve rounded-rect fidelity beyond plain rect fallback
4. become the draw substrate for `kit_ui` and `kit_graph`
5. improve glyph/text caching and layout quality for dense UI text workloads
6. continue visual parity tuning on top of the now-unified shared Vulkan text runtime

## Ownership Model

`kit_render` now supports two explicit attachment modes for native backend handles:

1. injected attachment:
   - `kit_render_attach_external_backend(...)`
   - `kit_render` does not destroy the native handle

2. adopted attachment:
   - `kit_render_adopt_external_backend(...)`
   - `kit_render` takes ownership and calls the provided release callback during `kit_render_context_shutdown(...)`

This keeps backend lifetime explicit and avoids hidden ownership transfer.

## Build

```sh
make -C shared/kit/kit_render
```

## Test

```sh
make -C shared/kit/kit_render test
```

## Validation Harness

Build the Vulkan validation harness:

```sh
make -C shared/kit/kit_render clean validation-harness KIT_RENDER_ENABLE_VK=1
```

This target rebuilds `shared/vk_renderer` with an absolute baked shader root so the harness can be launched from the repository root without shader-path failures.

Run it:

```sh
./shared/kit/kit_render/build/vk/kit_render_vk_validation
```

Expected visual result:

1. dark blue-gray full-window background
2. clipped main content area on the left
3. overlapping blue and orange rectangles near the upper-left
4. one bright diagonal white line
5. one mint-colored polyline near the upper-right of the main content area
6. a checker-textured square on the right side of the main content area
7. visible text labels: `KIT RENDER` near the lower middle and `PANEL` inside the right-side panel
8. a darker vertical side panel on the far right that is not clipped away

## Text Behavior

Current Vulkan text support now uses a TTF-first render path with shared `core_font` selection:

- `core_font` role and size tier resolve font path + point size
- text draw now derives rasterization scale from swapchain extent vs logical size
- `core_theme` color tokens drive glyph color
- SDL_ttf rasterizes UTF-8 text into surfaces that are uploaded and drawn via textured quads
- when rasterizing above logical point size, upload uses nearest filtering and draw destination is downscaled back to logical metrics
- backend-local bitmap fallback remains available when TTF path resolution or rasterization fails
- Vulkan backend text policy now consumes the same public `kit_render_resolve_text_run(...)` contract instead of keeping a separate internal copy of role/tier/zoom/raster policy

Current limitations:

- current path is still per-draw transient texture upload (no persistent glyph atlas yet)
- dense long-text views will benefit from glyph/string caching follow-on work
- fallback path is intentionally simple and not a final atlas/SDF implementation

`kit_render` now also exposes additive text metrics:

- `kit_render_measure_text(...)` returns width/height for a text run using the current context font preset
- Vulkan backend resolves the same TTF role+tier and uses SDL_ttf measurement (`TTF_SizeUTF8`) for caret/click alignment
- if TTF lookup fails, backend falls back to deterministic bitmap-estimated metrics so callers still get stable behavior

`kit_render` now also exposes additive shared text-runtime policy resolution:

- `kit_render_resolve_text_run(...)` returns the shared role/tier text contract for the active font preset
- the resolved payload includes the chosen `CoreFontRoleSpec`, zoom-adjusted logical point size, raster point size for a requested render scale, kerning policy, hinting mode, and upload-filter guidance
- this is the bridge surface for hosts such as `physics_sim` and `ray_tracing` that need the shared text/runtime policy before they move fully onto `kit_render` frame submission

`kit_render` now also exposes additive external text-runtime helpers for bridge hosts that still draw directly through `VkRenderer` / SDL_ttf:

- `kit_render_external_text_register_font_source(...)`
- `kit_render_external_text_unregister_font_source(...)`
- `kit_render_external_text_measure_utf8(...)`
- `kit_render_external_text_draw_utf8(...)`
- `kit_render_external_text_draw_utf8_at(...)`
- `kit_render_external_text_reset_renderer(...)`

These helpers are intended for apps such as `physics_sim` and `ray_tracing` that are not yet issuing full `KIT_RENDER_CMD_TEXT` frames but still need shared font-source tracking, render-scale-aware point-size reuse, persistent uploaded-texture caching, and consistent UTF-8 measure/draw behavior.

The Vulkan backend's internal `KIT_RENDER_CMD_TEXT` path now uses this same shared runtime for font-source registration, UTF-8 measurement, per-point-size raster-font reuse, and persistent uploaded-texture caching. That keeps bridge hosts and full command-buffer hosts on one text implementation technique inside `kit_render`.

## Runtime Preset Switching

`kit_render` now supports additive runtime preset updates on an existing render context:

- `kit_render_set_theme_preset(...)`
- `kit_render_set_font_preset(...)`

These calls are intended for between-frame use so apps can respond to runtime preset-cycle shortcuts without rebuilding the render context.

Press `Esc` or close the window to exit.
