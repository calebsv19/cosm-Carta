# Carta shared text editing and modal focus

The retained Main Edit adopts the existing `kit_ui 0.18.0` text-editing, SDL event,
text-presentation, native caret and focus-scope contracts from exact shared
`09ff89a`. No shared API/module version or persisted data format changed.

## Adopted owners

History import/export/edit fields and peak search, place details/pet fields,
saved-route rename and backup/restore path forms, ordinary place/history search
and saved-route naming now use caller-owned `CartaTextInput`. Capacities remain
those declared by each product field; place forms retain the existing 1024-byte
working edit bound. Product validation, preview/commit tokens, repository writes,
filters and cancel/save decisions remain in their existing application hosts.

`src/ui/text_input.c` binds and routes the shared editor. UTF-8 scalar boundaries,
selection, Home/End/arrows/Delete/Backspace, Ctrl/Cmd+A/C/X/V, atomic bounded insert
and composition semantics come from the kit. Select-all selects rather than
clearing saved text; replacement occurs on an actual edit. Submit/cancel are
intents, never repository mutations. Repeat Enter and clipboard activation are
suppressed. Escape first cancels preedit, then performs the app's cancel policy.
History field Tab/Shift-Tab wraps its declared field order; place forms traverse
currently drawn fields and commit the prior value only into the cancelable draft.
Field and button mixed traversal remains a separate host integration.

`src/ui/text_presentation.c` measures using the active font once, draws synchronous
caller-owned selection/preedit/caret and clips horizontal scrolling through an
explicit nested renderer viewport. It restores the enclosing viewport afterward.
Accepted presentation and exact font scale drive pointer positioning. SDL caret
rectangles map from Carta's logical render coordinates to window coordinates;
Retina scaling is not multiplied twice. The active visible field owns native text
input. Ownership changes cancel the old composition; focus loss/hiding stops the
native session and suspends editing, focus return resumes, and resize cancels
composition while preserving the buffer. Sessions are no longer globally started
at application startup. Legacy pin-name/Ingest hosts retain a focused session
until their dedicated editing migrations.

The background button surface uses `KitUiFocusScope`: modal entry saves the
semantic button key, and exit restores it only if currently eligible. Geometry,
workspace, selected-record or modal changes cancel pending activation while
retaining its release guard. Lifecycle invalidation clears restoration state
without using full CANCEL, which would forget that outstanding release. Window
and quit events still reach the host lifecycle path. Forms consume editing before
map shortcuts. History type/state/preview/save buttons and normal places/route
buttons use stable semantic identities and release activation; existing app
bodies remain authoritative. Route buttons no longer manufacture keyboard input.

## Qualification and limits

Production-linked text/history/place/control fixtures exercise Unicode boundaries,
selection and composition, clipboard through isolated SDL dummy storage, capacity
refusal, field order, modal restoration, cancel/save, preview invalidation,
press/release ownership and hidden-target eligibility. Shared text/native tests,
application build/regression/headless/visual, native Vulkan/SDL replay and package
checks qualify the checkpoint. Exact receipts and captures are in the private
MapForge rollout audit. The opt-in finite S5 text replay never submits its draft.
Native session/mapping and simulated preedit do not certify OS IME candidate UI,
Vulkan/SDL pixel equality, external-monitor/DPI transitions or all-platform behavior.

Pin-name and Ingest path now adopt the same bounded editor and presentation.
Remaining HUD, place-modal, chooser/legend, pane and window hosts are implemented
in the [completion rollout](shared_ui_rollout.md). Mixed field/button ordering,
actual OS IME acceptance and multilingual glyph coverage remain separate refinement
and qualification. App VERSION remains 0.3.0; canonical adoption and release remain
separate from Main Edit source and comparison package.
