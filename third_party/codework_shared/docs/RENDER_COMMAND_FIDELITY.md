# Render Command Fidelity

Last updated: 2026-10-04

This contract is implemented by `kit_render 0.14.6` and `vk_renderer 1.5.0`.
`vk_runtime 0.6.0` retains device/queue/lifetime ownership. It extends the
existing command vocabulary; it introduces no new core or mandatory GUI kit.

## Coordinates And Transforms

`KitRenderTransform` maps each command's local coordinates into logical frame
coordinates: `x_frame = x_local * sx + tx`, `y_frame = y_local * sy + ty`.
Translation and independent signed X/Y scales apply to rectangles, rounded
rectangles, line/polyline stroke geometry, textured quads and text geometry.
Negative scales mirror; zero scales collapse coverage. Initialize ordinary
commands with `kit_render_identity_transform()`; zero-filled scales are not
identity. The vocabulary has no rotation, shear or transform stack.

Rounded radii are clamped in local space before transformation; non-uniform
scales therefore produce elliptical corners. The coverage fringe accounts for
the command's absolute scale and the drawable/logical scale. Line thickness
is local geometry, so non-uniform scales affect the stroke along with its points.
Text retains the existing measured, pixel-snapped local layout and font upload
filter; its resulting glyph geometry is transformed. Text transforms do not
change font policy, layout measurement, hit testing or input coordinates.

Clip commands remain in frame space. Native scissors cover floor(left/top)
through ceil(right/bottom), intersected with the frame; zero-area clips stay
empty. CLEAR and clip commands have no command-local transform. Per-command
transform state resets after drawing, including failure paths, so later
commands and direct host draws do not inherit it.

## Textured Quads

The native backend consumes all quad fields: float destination bounds,
`texture_id`, normalized `uv_min`/`uv_max`, RGBA byte tint and transform.
Reversed UV endpoints flip sampling independently of geometry reflection.
The renderer's sampler clamps UVs outside the texture edges; equal endpoints
sample a constant coordinate. Filtering remains a property of the texture.
Tint multiplies sampled RGBA, then the existing straight-alpha blend applies.
Use `{255,255,255,255}` for an untinted quad; zero tint is transparent.

On Vulkan, `texture_id` borrows a `VkRendererTexture *` converted through
`uintptr_t`. Its CPU descriptor must remain valid through submission and its
GPU resources through frame completion. The texture owner retains destruction
responsibility. Text and polyline pointers retain their existing frame-borrowed
lifetime rules. Queue transient destruction or establish GPU completion before
destroying resources; returning from frame recording is insufficient.

`vk_renderer_draw_textured_quad(...)` adds the float/UV/tint primitive and
returns `VkResult` for invalid inputs, missing GPU resources or allocation
failure. The legacy integer `vk_renderer_draw_texture(...)` delegates with a
white tint and preserves source-region/default-destination behavior. Immediate
native primitives can use `vk_renderer_set_draw_transform(...)` and
`vk_renderer_reset_draw_transform(...)`; explicitly affine mesh calls keep
their own transform. Frame begin resets immediate transform state.

## Validation And Recovery

Recording rejects non-finite geometry/transforms/UVs, overflowing mapped bounds,
negative extents, invalid stroke thickness and missing required payloads.
Frame dimensions and clip coordinates fit the native signed integer range.
The entire borrowed command stream is validated again before native draw
emission. Invalid recording does not append a command. Invalid CPU payload
at submission returns `CORE_ERR_INVALID_ARG` and leaves the frame open: repair
or remove that payload and resubmit the same frame. Device/resource failures
are reported separately; recovery remains with the renderer/host lifecycle.

## Repeatable Proof

From the shared root:

```sh
make -C kit/kit_render test
make -C kit/kit_render KIT_RENDER_ENABLE_VK=1 fidelity-harness
make -C kit/kit_render KIT_RENDER_ENABLE_VK=1 test-fidelity-vk
make -C kit/kit_ui test-sdl-appearance
make -C kit/kit_ui KIT_RENDER_ENABLE_VK=1 test-rounded-vk
make -C vk_renderer test-live
```

Use a cold renderer/Vulkan-kit rebuild when compiler options or snapshot
identity changes; static archives do not establish that newly selected flags
were applied. Header dependency files are tracked by both modules. Native
gates require a logged-in graphics session and Vulkan validation layers.

The image gate captures 1x and 2x frames, verifies analytic inverse-transform
geometry and nearest texture samples with independent alpha/color composition,
and checks translated text pixels plus scaled/mirrored ink bounds and placement.
It exercises fractional translation, non-uniform and mirrored rounded geometry,
zero scale, strokes, UV crop/reversal/edge clamp, RGB/alpha tint, clipping,
empty clips, invalid-command rejection before drawing, same-frame recovery,
buffer growth, fence reuse and resize. Current proof is macOS/MoltenVK with
zero Vulkan warnings/errors. Native Linux qualification remains open; platform
flag selection alone is not runtime or portability evidence.

## Reference Cohort

The bounded rollout uses orChestra and eCho as shared-command/native Vulkan
hosts and DataLab as the SDL appearance reference. Each retained Main Edit
imports a committed shared snapshot before host integration. Build, captured
output, domain/headless and isolated package evidence remain distinct.
DataLab's existing native image path and Vulkan-presented SDL UI canvas retain
their separate ownership. The common Font/Theme controls now adopt the optional
[button interaction contract](UI_INTERACTION_CONTRACT.md); broader focus/capture,
editable pane topology and other program migrations remain follow-on slices.

The 2026-10-04 local development cohort imports shared `f80f91d` and uses:

| Main Edit host | Integration checkpoint | Proof wiring |
| --- | --- | --- |
| orChestra / WorkspaceSandbox | `4b0f57d` | Application Vulkan kit/renderer/runtime archives |
| eCho / MemConsole | `4a6e093` | Application target-specific copied archives |
| sCope / DataLab | `a291e7d` | Application native renderer/runtime objects plus an isolated Vulkan-kit harness archive; normal UI stays SDL |

Each host exposes `make render-fidelity-self-test` and accepts
`RENDER_FIDELITY_OUTPUT_DIR=<path>`. Source verifiers compare the five selected
kit/renderer/runtime modules against the accepted commit, not only version
strings. All three pass the common 1x/2x oracle, their product/headless and
native lifecycle gates, and isolated Main Edit package identity/signature
checks. Installed Main Edit bundles match their packages; stable Desktop
bundles and canonical program source/version state remain unchanged.

The image gate checks 301,545 geometry/texture pixels and 2,106 text samples at
1x, and 465,019 geometry/texture pixels and 8,424 text samples at 2x. Validation
warnings and errors are zero on Apple M2/MoltenVK. These are bounded contract
checks, not general interaction acceptance or native Linux qualification.
The shared build and all three independently linked host harnesses produce
byte-identical 1x and 2x captures for this fixture on the verified local host.


## Interaction follow-up

The trio now imports shared `ad3b83b` with `kit_ui 0.13.1` and
`kit_workspace_authoring 0.6.1`; the renderer/runtime module files and fidelity
semantics above are unchanged. The new optional Font/Theme interaction contract
is proved separately through host replay and SDL focus-marker pixels. It does
not widen native render command fidelity into general UI acceptance. See
[Button interaction](UI_INTERACTION_CONTRACT.md) and each host's
`docs/ui_interaction.md` for the current pin and later adoption boundaries.
