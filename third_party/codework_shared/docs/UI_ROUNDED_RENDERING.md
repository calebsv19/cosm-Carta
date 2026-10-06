# Shared Rounded UI Rendering

Last updated: 2026-10-04

The first shared UI slice preserves the immediate-mode command model while
making recorded corner radii functional on the Vulkan backend. Its accepted
source snapshot is `e5887348657f782e6094cdeeb095259dedb32bf8`.

| Owner | Version | Responsibility |
| --- | --- | --- |
| vk_runtime | 0.6.0 | Device, queue, validation, and runtime lifecycle |
| vk_renderer | 1.4.0 | Rounded solid geometry; drawable-scale edge coverage; safe frame-buffer retirement |
| kit_render | 0.14.5 | Forward positive rectangle radius and float bounds into the Vulkan primitive |
| kit_ui | 0.11.3 | Existing compact rounded appearance/state semantics; bounded real-image test |
| Application | Existing app version | Hit bounds, actions, labels, layout, preferences, and persistence |

`vk_renderer_fill_rounded_rect(...)` clamps radius to half the smaller extent.
It uses the solid pipeline with a one-drawable-pixel alpha coverage fringe and
requires no per-shape texture upload. Replaced vertex allocations remain alive
until their owning frame completes. Static-library consumers must rebuild.

## Verification

```sh
make -C kit/kit_render test
make -C kit/kit_ui test
make -C kit/kit_ui KIT_RENDER_ENABLE_VK=1 validation-harness
make -C kit/kit_ui KIT_RENDER_ENABLE_VK=1 test-rounded-vk
make -C vk_renderer test-live
```

The image gate renders square, rounded, clamped/pill, clipped, translucent,
nested-border, and shared-button cases at 1x/2x; checks pixels against an
independent geometry oracle; forces buffer growth and frame-fence reuse; and
requires clean Vulkan validation. Live gates need a Vulkan surface and validation
layer. Current proof is Apple M2 / macOS / MoltenVK; PC/Linux proof remains open.

## First Hosts

WorkspaceSandbox Main Edit uses the shared compact appearance in Font/Theme,
the floating Workspace HUD and top-level authoring controls. Shared HUD follow-up
`51b331a` adds `kit_workspace_authoring 0.5.2`, replacing separate square border
strips with the existing `kit_ui` appearance and correct centered caption origins.
Its widget dependency is confined to the UI target. MemConsole Main Edit adopts
it through its common DB/browser/graph inspector button adapter. Its captions
use measured horizontal centering with the existing font roles and vertical
anchor; interaction policy stays host-owned. Both use managed subtree imports of the
accepted shared commit, with app adapter commits kept separate. Source tests,
headless checks, native Vulkan resize/restart checks, and isolated development
packages pass. Canonical source adoption, app VERSION changes, and publication
are separate operations.

Later work should extend one proven rendering contract and one host surface at
a time. Transform correctness, texture UV/tint, keyboard focus/navigation, pane
composition, and remaining local control helpers are outside this slice.

## Host Import Discipline

The HUD follow-up is a patch to the existing authoring kit, not a new widget
framework. Rebuild changed shared archives after managed subtree imports; an app
linking a previously built archive is not proof of new source adoption. Preserve
separate dependency-import and host-integration commits.

## SDL Appearance Contract (0.12.0)

`kit_ui 0.12.0` adds the optional direct-SDL
`kit_ui_sdl_draw_button_spec_appearance` entry point. SDL hosts can now reuse the
same button spec, theme/state resolver, compact rounded preset, and nested
outline/fill model as command-frame hosts. The older fill-only HUD adapter remains
available. The new adapter uses synchronous host measurement/drawing with
top-left text origins; it scales no coordinates internally.

`make -C kit/kit_ui test-sdl-appearance` checks 1x/2x software-surface pixels,
state precedence, measured caption coordinates, clipping, radius clamping,
borderless controls, and invalid inputs. This proof is independent of the
existing native Vulkan geometry gate. Direct SDL drawing remains CPU rasterization
even when the resulting canvas is presented through Vulkan.

## DataLab Main Edit Host

DataLab Main Edit imports the committed SDL appearance snapshot `5b017d4` and
uses a single host adapter for playback, authoring top-level controls,
Font/Theme/custom-theme buttons, and Recent Directories. The kit's 1x/2x SDL
pixel/state/text checks and DataLab build, input/panel/profile contracts, full
stable/headless smoke, exact-source native host checks, and isolated package
checks pass. Native host capture verifies the UI compatibility canvas is still
presented correctly through Vulkan at 2x scale. This is CPU-composed UI and
remains distinct from DataLab's native image-session path. Other programs keep
their previously imported snapshots until an explicit host migration.

## Transform And Texture Follow-Up

The same three Main Edit proving hosts now import shared `f80f91d`, with
`kit_render 0.14.6` and `vk_renderer 1.5.0`. Command-local translation and signed
independent scale, float rounded geometry, texture UV crop/reversal and RGBA
tint now have a common host-linked native 1x/2x image oracle. This supersedes
the earlier transform/UV/tint gap for this bounded command contract. See
[Render command fidelity](RENDER_COMMAND_FIDELITY.md) for exact semantics,
validation/recovery, source pins and proof commands. DataLab retains
`kit_ui 0.12.0` SDL button drawing as the reference; Vulkan presentation and
native image drawing remain separate from that CPU UI composition.


## Optional button interaction follow-up

The retained Main Edit cohort now imports shared `ad3b83b`, with
`kit_ui 0.13.1` / `kit_workspace_authoring 0.6.1`, on the common Font/Theme
authoring controls. Appearance and input ownership stay separate: the optional
context adds focus, press-origin capture and release activation while the host
owns actions and persistence. DataLab uses the same rounded SDL drawing path;
the two Vulkan command hosts append the shared focus marker. Broader controls,
modal/text entry, panes and native Linux remain later slices. See
[Button interaction](UI_INTERACTION_CONTRACT.md).
