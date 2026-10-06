# Shared button interaction contract

## Optional surface snapshots (kit_ui 0.14.1)

`KitUiSurface` bridges immediate drawing and event-driven hosts without a widget
tree. A host begins a scope, registers visible controls with full-width semantic
keys, and ends the collection to publish a validated snapshot. Registration
order is focus/paint order; clipping intersects hit bounds with visible output.
The 256-control capacity fails explicitly. Opaque handles preserve identity when
rows move and cannot alias a captured or queued owner. Invalid collections keep
the prior published snapshot; a host must report the error and repair it before
continuing to use changed geometry.

Normalized events route through the existing interaction engine. Activations are
queued in event order (32 maximum), and `take_activation` permits one per host
frame. Hosts with drawing-time actions schedule another redraw while pending;
direct event hosts collect per event and dispatch the returned semantic key.
Hidden/disabled controls prune pending actions. A new modal scope removes old
focus/capture/action owners but consumes their outstanding pointer/key release.
Lifecycle cancellation clears the snapshot's pending activation queue.

Hosts own scope IDs, coordinates, fresh geometry after resize/layout changes,
action dispatch and persistence. Text editing takes keyboard ownership while
pointer controls remain usable; host shortcuts with Ctrl/Alt/GUI pass through.
Canvas selection, scrollbars and drag handles remain separate gesture contracts.

`kit_ui >= 0.13.1` provides an optional, caller-owned interaction context.
`kit_workspace_authoring >= 0.6.1` registers the common Font/Theme surface.
This is an additive button contract; legacy stateless `kit_ui_eval_*` callers
keep their existing behavior until explicitly migrated.

## Ownership and ordering

The host normalizes platform events into control coordinates, registers visible
controls with stable nonzero semantic IDs, and routes each event in arrival order.
Use one context per active input scope. The context stores only focus/capture
IDs, the armed activation key, and pointer position; it stores no widget tree,
borrowed labels, layout pointers, product actions or persistent preferences.
The host dispatches the returned activated ID during Update. RenderDerive reads
the context; RenderSubmit draws appearance and a shared focus underline.

Controls use half-open bounds. Last registered control wins overlapping hits,
including disabled controls that block click-through. Duplicate IDs, invalid
geometry and nonfinite pointer coordinates fail before ownership mutation.
The control list is bounded to 64 entries and borrowed only for the call.

## Pointer and keyboard behavior

A left press inside an enabled control acquires capture and focus. Moving
outside preserves capture, but clears its pressed appearance. Release activates
only that owner, only if still enabled/registered and released inside its current
bounds. Release without a matching press, or press A/release B, cannot activate.
Removed/disabled owners lose focus/capture; their pending release stays consumed.

Tab/Shift-Tab traverse enabled registered controls in visual registration order,
with wraparound. Key repeat does not move focus or duplicate activation.
Unmodified Enter/Space arm the focused button on key-down and activate once on
matching key-up. Modifier shortcuts remain host-owned. Escape first cancels an
armed press; otherwise it passes to the host's existing cancel action. Enter
without button focus passes to the existing host apply action.

The optional SDL adapter normalizes left-button, navigation and activation
events without adding SDL to the generic archive. It emits cancellation for
focus loss, minimize/hide, resize and quit. Hosts must also reset the context
when switching surface, opening a modal/text editor, leaving authoring or
replacing control ownership. Logical capture routes received events; window/
OS pointer delivery remains host-owned.

## Adoption and proof

The retained Main Edit trio now registers its inventoried button surfaces:

- orChestra: top authoring controls, ingest HUD/tabs/root actions/authoring shortcuts, common Font/Theme and module-picker assignment rows.
- eCho: left browse/project/item controls, graph settings/action HUD, relationships, legend filters, DB modal and common/top authoring controls.
- DataLab: playback, Recent directories, common and custom-theme authoring controls, top controls and picker Recent roots/artifacts.

Active modal scopes exclude background controls and cancel their old targets while
consuming outstanding releases. Product actions, database/catalog operations,
Apply/Cancel, pane editing, text/caret/clipboard/IME and persistence remain
host-owned. No portfolio-wide or generic text-edit contract is implied. See each
host's `docs/ui_interaction.md` for actual routing, scopes and proof.

Shared gates:

```sh
make -C shared/kit/kit_ui clean all test test-interaction-sdl
make -C shared/kit/kit_workspace_authoring clean all test
```

These cover press origin, cross-control release, outside/back capture, missing
or disabled controls, focus order, modifier passthrough, repeat, release pairing,
cancellation, invalid-registration preservation, clipped registration, and real
SDL focus-marker pixels at 1x/2x. Host gates must exercise real action adapters;
linkage or a synthetic controller demo alone is insufficient adoption proof.

Next boundaries are the separate text-edit/IME and modal-focus composition
contract, pane/layout composition, human workflow review and native Linux qualification. The
renderer command contract remains [Render command fidelity](RENDER_COMMAND_FIDELITY.md).


The initial Vulkan hosts additionally capture their actual focused Font/Theme
panel commands through the normal application archives. The native marker
interior matches the contract at 1x/2x with zero validation warnings/errors.
This local output proof is separate from human workflow review and native Linux
qualification. The source-linked reproducer and captures are retained under
`_private_workspace_artifacts/ui_unification/interaction_20261004/`.


The 0.14.1 patch supports C++ linkage and optional returned control storage. The current source pin is `b1c67d7`. Host source checks verify immutable Git snapshot bytes; `--require-current-canonical` additionally checks the mutable upstream checkout. The expanded surface replay and ordinary HUD capture evidence is retained in `_private_workspace_artifacts/ui_unification/surfaces_20261004/`. Queue-time caption ownership is a host responsibility; eCho uses a bounded UI-frame arena, while DataLab draws captions synchronously through SDL.


## 2026-10-05 bounded text editing and modal focus

The accepted text companion is [Bounded text and modal focus](UI_TEXT_FOCUS_CONTRACT.md) at `kit_ui 0.15.1` / `0cc23aa`. The trio now adopts its editor-model and one-modal semantic restoration mechanics; host text layout/native IME, field transaction policy, product actions and persistence remain separate. Earlier checkpoint pins remain historical. Next prove shared caret/selection/preedit presentation, then native IME acceptance and pane composition.


## 2026-10-05 shared text presentation

The measured companion is [Shared text presentation](UI_TEXT_PRESENTATION_CONTRACT.md), `kit_ui 0.16.0`. The trio now adopts measured caret/selection/preedit geometry and actual-font row/hit presentation. Host field eligibility, domain actions, native sessions and persistence retain their owners; mixed traversal/native IME acceptance and panes remain follow-ons. Earlier checkpoint references are historical.

## 2026-10-05 pane host behavior adoption

Accepted shared source `ddc9fee6e17482dcd64cf777d7a105b7ed9b157d` adds `kit_pane 0.5.0`, with
`core_layout 0.2.1` supplying revisions and existing authoring transactions.
The generic pane host adds stable mount/unmount/resize dispatch, pointer ownership,
pane focus invalidation, and takeover cancellation. Drag-sized edits nest inside
an existing authoring draft; hosts restore their own topology/ratios on cancel,
retain domain actions/history, and persist only accepted changes. Shared bounded
header slots reserve title space and register only visible actions through the
existing kit_ui surface. Header labels use the existing centered button painter.
No rendering backend is replaced: kit_render 0.14.6, vk_renderer 1.5.0,
vk_runtime 0.6.0 and kit_ui 0.17.0 remain at their accepted versions.

Orchestra wraps its existing snap/rewrite splitter controller and adds a MODULE
header slot opening the existing picker. Echo isolates nested metadata,
relationships and body input/paint clips, fixes parent-span ratio clamping,
commits preferences on accepted release, restores all four ratios on Escape,
focus loss or takeover, and adds a GRAPH-header REFRESH action. DataLab keeps its
actual viewer canvas with source-control/header overlays, gives those regions
stable ownership and SDL clipping, uses a RECENT DIRECTORIES header slot, and
wraps its existing authoring projection drag in a nested layout transaction.
DataLab's fixed authoring projection remains a projection; this does not turn
all profile viewers into a generic movable pane tree.

The proving scope remains the three retained Main Edit lanes. Canonical program
source and VERSION, production bundles, release/Registry and remote hosts are
unchanged. Fullscreen lifecycle qualification is next; docking, generalized
pane provider insertion/persistence, product-wide mixed field/button traversal,
human OS IME candidate/commit/cancel acceptance, native Linux/Windows and other
programs remain separate. See the pane host contract and migration guide.
