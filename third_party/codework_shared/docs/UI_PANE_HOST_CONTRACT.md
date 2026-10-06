# Pane host behavior contract

kit_pane 0.5.0 extends the 0.4.0 composition contract. It depends on
core_layout 0.2.1 and core_pane 0.3.1. No SDL or product dependency is introduced
into the generic archive.

## Lifecycle and dispatch

Build a validated composition in render coordinates, then synchronize a
KitPaneHost before input dispatch. Stable IDs identify domain panes across
resize/reordering. Mount, unmount and resize notifications let a host attach its
own controllers/resources. Hidden, disabled and removed owners lose pointer
capture and keyboard ownership before unmount. Modal or splitter takeover cancels
capture and pane focus; returning cannot revive the old press. A host may restore
an eligible text/control focus through its existing focus scope, never a press.
Pointer press selects the pane keyboard owner; motion/release remain with the
press owner even outside its bounds. Activation still requires the button's
existing kit_ui surface policy. A release with no press cannot activate a pane.
The adapter must forward focus-loss/cancel, rebuild snapshots after resize and
route nested leaf regions independently. Wheel and text events use the current
pane owner; global shortcuts and modal input remain host policy.

## Splitter transaction

Save app-owned ratios/topology and begin KitPaneLayoutEdit before mutation.
Preview uses existing core_pane constraints. Update marks a changed draft.
Commit applies one runtime revision, or leaves changes in an existing outer
layout-authoring draft. Cancel restores the saved app payload and the exact
prior CoreLayoutState, preserving preexisting authoring changes. No-op commit
creates no revision. A stale revision is rejected instead of overwriting newer
state. Escape, focus loss, modal takeover, invalid resize or owner removal must
cancel; pointer release commits. Persistence is triggered only by accepted
commit. The host retains domain history, topology validation and rebuild work.

## Header slots

KitPaneHeaderAction carries a stable action ID, render-coordinate width and
enabled state. kit_pane_header_layout reserves title_min and padding first, then
places a bounded prefix of priority-ordered actions at the right edge with an
explicit gap. Omitted actions have no input registration. Disabled actions keep
their slot but cannot activate. Use the same slot rectangle for drawing, clipping
and kit_ui surface registration. The host applies UI scale, supplies measured
labels with frame-safe lifetime, and dispatches the resulting domain action.

## Verification

`make -C kit/kit_pane test test-composition-sdl` covers lifecycle order, capture,
takeover, disabled/unmounted owners, no-op/runtime/nested/cancel/stale transactions,
header priority and exact SDL nested clipping at 1x/2x. A host additionally proves
its actual nested render/input boundaries and restores its own payload on cancel.

## Quiet runtime splitter resizing

A runtime splitter transaction is not entry into the full workspace authoring UI.
KitPaneLayoutEdit may borrow CORE_LAYOUT_MODE_AUTHORING internally while
`active && owns_authoring` identifies a runtime-owned temporary edit. Hosts must
suppress authoring HUD, pane-ID overlays and all-pane authoring outlines for this
case. Keep explicit session entry and its product-specific authoring UI separate.
Before explicit takeover, cancel the pending runtime drag and then capture the
accepted baseline. Closing an outer authoring session cancels unfinished nested
splitter edits before Apply/Cancel/Exit. Existing revision, no-op and cancellation
contracts remain unchanged.

Keep divider painting independent of the hit registry. sketCh's proven default
is a 16-logical-pixel centered hit band, with a 2-render-pixel hover/drag line.
Scale the hit width once from logical window to render coordinates on each axis,
using the bounded render canvas when applicable. Route runtime divider presses
before neighboring content controls, invalidating their pending activation on
takeover. Explicit header action slots retain their own visible click bounds,
so a thin header row does not make FIT/LAYOUT unreachable inside the wider band. Check both sides of vertical and horizontal edges, 1x/Retina mapping,
quiet drag painting, single commit, exact rollback and explicit authoring entry
through the production loop. Shared APIs/versions are unchanged by this adoption
correction; fixed product policies remain in the app adapter.

## Fixed-module lifecycle and accepted persistence — sketCh 2026-10-06

The sketCh Main Edit follow-on reuses core_pane/core_pane_module/core_layout and
kit_pane 0.5.0 without shared code/API/VERSION changes. Prepare and validate all
bindings before changing controllers. Treat (pane, instance, module, config,
flags) as controller identity: changing it requires cancel/blur/unmount before
mount at the same pane ID. Hidden/disabled/empty/removed panes have no input or
paint target; geometry-only changes preserve controller identity. A modal cancels
input without unmounting otherwise visible controllers. Apps retain module purpose,
visibility/configuration policy and resource owners; this is fixed-module adoption,
not a generalized docking/plugin registry.

Separate accepted geometry from both explicit authoring drafts and temporary
runtime splitter edits. Saving an unfinished runtime drag exports its pre-drag
nodes/revision; it does not finish the gesture. Preserve valid accepted bindings
on reopen instead of resetting every non-default arrangement. Root index and
visibility must round-trip with the host's versioned pane payload; do not change
core_pack framing for an app-only payload extension. Stage snapshot reads before
publishing the document/layout. Failure retains live document, controllers and
resources; a successful replacement cancels against old state and drops pending
resource work that borrows old storage before releasing it. Renderer-owned shared
textures/fonts may remain cached during layout preview, hiding and Cancel.

Qualify real ownership per renderer/profile. sketCh's RGBA view uses cached surface
textures; its indexed view borrows indexed raster storage and paints directly.
A linked legacy texture getter is insufficient resource evidence. Require
production-linked remount/failure/Cancel/save/reopen tests, pending-cache lifetime
checks, actual-loop/native captures, normal package identity and installed readback.
Finite opt-in probes must request their own frames so idle policies cannot stall
their watchdog. No probe is part of routine product operation. See the
[sketCh contract](../../_worktrees/drawing_program_main_edit/docs/ui_contract.md).

The fixed-pane macOS UI baseline can close independently of generalized docking,
full replacement of remaining legacy content-action bridges, GPU-native composition,
other-platform/monitor/IME acceptance and canonical/production adoption. Record
those boundaries explicitly rather than treating every future improvement as an
unfinished baseline requirement.

## sketCh direct-command pilot feedback — 2026-10-06

The retained Main Edit follow-on reuses the existing kit_ui semantic key and
kit_pane composition contracts; shared code/API/module versions do not change.
Registered content/header/authoring activations carry operation plus domain
identity directly to app commands. Do not resolve an accepted command to a button
center, manufacture a pointer event or hit-test another row to recover meaning.
Keep app action bodies authoritative and shared by typed semantic and applicable
spatial entry points. Claim activations once; invalidate pending meaning on scope
or geometry changes. Reject modal/hidden/disabled/stale targets without fallback.

Bind object-inspector edits to object IDs; resolve dynamic lists by current domain
identity. An operation explicitly named for the active target may retain that
product policy, with current eligibility rechecked. Preserve real coordinates for
color/picker/opacity/drawing/scrolling controls. Test semantic dispatch with a hit
hook that fails on spatial fallback, plus positive domain effects, stale/reordered
targets, keyboard/pointer parity and native modal transition checks. UI rendering
and GPU composition remain separate qualification boundaries.
