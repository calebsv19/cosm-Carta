# Carta shared control adoption

The retained Main Edit uses the exact shared `09ff89a` snapshot for the four
workspace tabs, normal Hiking & History actions, places/service toolbar, saved-route
buttons and history form type/state/preview/save actions. `kit_ui 0.18.0`
provides compact rounded appearance, style resolution, semantic surfaces,
pointer capture, release activation and keyboard focus. `vk_renderer 1.7.0`
renders native rounded geometry; SDL uses the optional shared rounded adapter.

## Ownership and behavior

`src/ui/control_surface.c` synchronizes a caller-owned `KitUiSurface` before
input and drawing. Domains distinguish workspace commands from history commands;
values identify fixed product actions rather than visible row positions. The
existing workspace enum and history action numbers retain their meaning when
layout rearranges controls. Places/route buttons call their product bodies directly rather than synthesizing
keyboard events. History form buttons activate on matching-owner release. The host's existing rectangle functions remain the
geometry source, clipped to the current window. Hidden detail/point buttons do
not register. Guarded Undo remains visible after its target leaves the filter.

A press arms its originating button; release inside that same button activates
once. Dragging out, releasing over another button, resize, selected-record
change, workspace/layout change or modal/authoring/staging takeover cancels the
armed action. Outstanding releases cannot activate a new owner. Tab/Shift-Tab
moves among available buttons, and Enter/Space activates on key release. Repeats
and modified activation keys do not run actions. Window/quit events still reach
the existing host lifecycle handler. Search editing retains its keys.

Existing workspace selection, history commands, guarded persistence, route fitting,
terrain selection/reload and import/export actions remain app-owned. The shared
surface emits a semantic key, and the app executes the existing body. There is
no new general command registry or persistence format. Surface state is transient
and is not written into the personal repository or saved layout.

`src/ui/control_presentation.c` resolves shared appearance against Carta's theme,
centers captions using actual active-font metrics, clips long labels and fits
vertical font zoom inside existing controls. Border/inset radii use shared math.
Text draws synchronously through the existing text bridge, so dynamic stack
captions never outlive their storage in a deferred render queue.

## Qualification and remaining hosts

Production-linked toolbar and history tests exercise down/up gestures, drag-out,
resize/focus/modal cancellation, keyboard release/repeat, search ownership,
import preview/cancel, guarded progress/Undo, filters and narrow layouts. Native
road/terrain and SDL fallback replays, build/regression/headless and Main Edit
package checks qualify each adoption checkpoint; private rollout receipts record
exact source/package identities and capture limits.

This initial control slice is extended by the [completion rollout](shared_ui_rollout.md):
Map HUD, sidebar and place-modal buttons, view/peak/terrain chooser and legend
controls now share presentation and activation. Product spatial list/slider/map
gestures remain authoritative within shared pane/window ownership. Shared editing
covers declared fields; combined field/button ordering and native platform
acceptance remain explicit boundaries. Canonical source, VERSION and release are
separate from the Main Edit comparison package.

See [text/modal adoption](shared_text_modal_adoption.md) for bounded editing,
focus restoration and native session/caret ownership.
