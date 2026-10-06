# Carta shared UI rollout

Carta Main Edit adopts the existing shared UI contracts from immutable dependency
`09ff89a0a32d80981010b15a7d45a2cdf8e5fd9f`: kit_ui 0.18.0, kit_pane 0.5.0,
core_pane, kit_render 0.14.6, vk_renderer 1.7.0 and vk_runtime 0.6.0.
This is development adoption. Canonical source and application VERSION 0.3.0
remain separate from the Main Edit comparison app and any future release.

## Adopted hosts

| Host | Shared behavior | Product ownership |
| --- | --- | --- |
| Workspace tabs; Hiking & History actions | Rounded measured captions, visible semantic inventory, release activation, repeat suppression, eligible focus restoration | Workspace selection, filters, progress, guarded Undo, GPX actions |
| Places/service and saved-route toolbar | Same presentation and activation; direct commands | Filtering, search, saved route intent, storage and guards |
| Map HUD | Explicit CAR/WALK selection; layer toggles and modifier panel commands; zoom and route alternative buttons | Routing, layer readiness, opacity/fade sliders, playback and camera |
| Sidebar | Open/close, section tabs, Pins/Ingest action row, pin metadata chips | Draft Save/Cancel/Delete, import/source selection, collections and service editing |
| Place/profile/pet/route/backup modals | Visible scoped semantic action inventory, direct product dispatch, bounded shared fields | Draft validation, service roles, route management, restore preview and confirmation |
| History view chooser, legend, peak picker, terrain manager | Shared modal/button surfaces, clipped eligible rows, identity/geometry invalidation, release activation | Group union/visibility/color, peak goal creation, coverage and protected removal |
| Map preparation status | Scoped Cancel button, shared release/keyboard activation and centered presentation; Escape cancels directly | Existing background staging/cancel/join and current-map retention |
| Pin name and Ingest path | Shared UTF-8 scalar editing, selection/clipboard/preedit/caret, visible native session | Existing capacities and explicit Save/Apply policies |
| History/peak/place/search/route fields | The same bounded text and presentation adapters | Existing field order, capacities, preview, validation and transactions |

Buttons synchronize their current geometry before routing. Modal takeover,
window changes, selected pin/history target, store revision, route objective
bindings, and visible item changes cancel pending activation. Outstanding releases
remain swallowed. Accepted commands execute once without synthetic keys or
manufactured pointer coordinates. Dynamic modal rows are bound to the current
visible inventory; scrolling, refresh and accepted mutations invalidate captures.

Spatial controls retain their product gestures: pin/list selection and reorder,
Ingest double-click, history scrolling/point picking, sliders, map endpoint dragging,
and explicit workspace authoring. A list selection is not converted into a button
transaction. The shared pane/window owners bound these gestures and cancel them
on invalidation without saving, deleting or changing draft bytes.

## Pane contract

`core_pane` solves sidebar/map topology. `kit_pane` composes shell/header/content,
clipping and topmost hit regions, retains pointer ownership and handles the splitter.
Map mode keeps its full projection beneath the overlay sidebar. Hiking & History
reserves the composed right-hand map viewport so fitted routes stay visible.
Map input uses the composed hit/gesture owner. A drag originating in the sidebar
cannot become map panning merely by crossing the divider.

Ordinary divider resizing has a **14 logical pixel hit region** around a **one
logical pixel visible line**. It does not enter workspace authoring, expose pane
borders or show the authoring HUD. The app retains 280/480 preferred pane minima;
core_pane fits constrained small windows. Optional `sidebar_ratio` is saved in the
existing view preference JSON on release, leaving old preferences compatible.
Pin/list, header and form layout remain local policies within composed bounds.

## Window contract

F11 uses the shared desktop fullscreen adapter; repeated F11 does not toggle again.
The shared observer reads actual logical size, drawable size, display/mode and
presentability. Geometry is refreshed before subsequent event routing. SDL rendering
uses one logical coordinate space with drawable scaling applied once. Both native
creation paths request high-DPI support and retain a 320 by 240 minimum window.

Resize, display/mode changes, focus loss, hide and minimize cancel transient button,
pane, list, slider, endpoint and modal ownership and composition while retaining
product drafts. Hidden/minimized windows do not present frames; resumed rendering
uses actual geometry and counts only successful presentations. Quit reaches the
runtime even when a modal owns input. Shared Vulkan keeps swapchain/resource
recovery ownership; Carta does not duplicate renderer internals.

An opt-in `CODEWORK_WINDOW_LIFECYCLE_PROOF=<existing-directory>` runs the shared
finite probe through Carta's actual event/update/render loop. It qualifies initial,
resize, fullscreen, windowed, hidden, shown, minimized and restored stages, actual
presented-frame progress, suspension, coordinate mapping and native captures.
Use an isolated runtime fixture for engineering qualification.

## Verification and remaining boundaries

Build first, then `test-carta-rollout`, text/pin/layout/window/map, production-linked
utility/history/place controls, followed by aggregate `test`, `run-headless-smoke`,
`main-edit-package-contract-checks`, `visual-artifact`, native Vulkan/SDL lifecycle
and map/terrain replays. Direct-source fixtures require their explicit linked kit
translation units; the application uses header dependencies and a fresh build root.
Exact results and source/package identities belong to the private rollout receipt.
Timing-sensitive profile gates and native captures are separate evidence planes.

Remaining refinement is small: combined field/button Tab ordering where a field
currently retains keyboard ownership, actual OS IME candidate/clipboard acceptance
and multilingual font coverage. External-monitor/DPI moves, Windows/Linux native
presentation and catastrophic device-loss recovery require platform qualification.
Current swapchain lifecycle proof does not certify those cases. Adoption of these
contracts does not create a docking editor, rewrite product layouts or authorize
canonical adoption, version change, publication or Registry promotion.

See [control adoption](shared_control_adoption.md), [text/modal adoption](shared_text_modal_adoption.md)
and [dependency checkpoint](shared_dependency_checkpoint.md).
