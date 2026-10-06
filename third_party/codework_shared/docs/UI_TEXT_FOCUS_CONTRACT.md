# Bounded text editing and modal focus contract

The optional `kit_ui 0.15.1` APIs extend the button interaction contract without
changing legacy callers or coupling Core to UI. This first slice supports one
modal over one host scope, bounded caller-owned UTF-8 buffers and Unicode scalar
movement. It is not a retained widget tree or a shaped rich-text editor.

## Ownership

Core retains domain meaning, state and persistence. The kit owns reusable edit,
selection, preedit and button-focus mechanics. Apps own field identity and
eligibility, Enter/Escape commit/cancel actions, numeric policy, text layout and
hit mapping, wrapping, undo transactions and persistence. SDL text-input session,
IME candidate rectangle, clipboard availability and window lifecycle belong to
the host or optional adapter, never the generic static archive.

## Text mechanics

`KitUiTextEdit` borrows a NUL-terminated buffer with explicit capacity. Cursor and
anchor are byte offsets at validated UTF-8 scalar boundaries. Bind/mutations
validate storage and input. Insert replaces selection atomically; invalid UTF-8,
single-line line breaks, digit-policy violations or insufficient capacity leave
the prior buffer/selection intact. Whole input is rejected rather than silently
truncated. Left/Right, Home/End, Shift selection, Select All and Backspace/Delete
share these boundaries. Grapheme clusters, bidi and font shaping are deferred.

Composition is transient and separate from committed text. SDL_TEXTEDITING stages
bounded preedit; SDL_TEXTINPUT commits through the same transactional insertion.
Navigation/submit shortcuts do not mutate committed text during active preedit.
Escape first cancels preedit; a subsequent Escape returns host cancel intent.
Repeat Enter/Escape cannot cause duplicate app actions. Ordinary held deletion or
movement follows SDL repeat. Clipboard copy/cut publishes only selected text; cut
mutates only after successful clipboard publication, and paste uses the same
validation/capacity policy. Clipboard scratch and SDL allocations are released.

## Focus and modal scopes

Hosts register only visible eligible controls. Text takeover clears button focus
and armed button-key owner while preserving outstanding release consumption.
Entering a modal saves the semantic button key, clears background controls and
leaves product actions with the host. Returning restores focus only when the
original scope and enabled semantic target still exist. Removed/disabled targets
stay unfocused. A different root scope discards restoration. Nested modal changes
fail explicitly in this first slice. Focus loss/hide/quit cancel preedit and
restoration; no stale restoration may run after lifecycle cancellation.

Tab can hand keyboard ownership to the existing active button list; clicking a
field reclaims text ownership. Pointer caret hit placement remains a host layout
operation; offsets must be clamped to scalar boundaries before mutation. Mixed
field/button traversal and wrapped selection painting are later contract growth.

## Adoption requirements

1. Inventory fields, active owner, modal boundaries and commit/cancel policy.
2. Add a caller-owned edit state and a small optional platform/host adapter.
3. Route text only to an eligible visible owner before product shortcuts.
4. Stage preedit separately; cancel it when owner/scope or lifecycle changes.
5. Preserve action/persistence owners and queued text lifetime through submission.
6. Verify UTF-8, selection replacement, capacity refusal, clipboard/IME events,
   focus restoration/removal and modal background exclusion in real host paths.
7. Import reviewed shared bytes, then qualify source, runtime, package and human
   native IME acceptance separately. No linkage-only conformance claim.

Standalone gates: `make -C shared/kit/kit_ui test test-text-edit-sdl`. Host adoption
and coverage are recorded in each Main Edit's `docs/ui_text_focus.md` and the
supporting private trio inventory. Native OS candidate-window acceptance, Linux
qualification and canonical/release rollout remain independent evidence.


## 2026-10-05 shared text presentation

The measured companion is [Shared text presentation](UI_TEXT_PRESENTATION_CONTRACT.md), `kit_ui 0.16.0`. The trio now adopts measured caret/selection/preedit geometry and actual-font row/hit presentation. Host field eligibility, domain actions, native sessions and persistence retain their owners; mixed traversal/native IME acceptance and panes remain follow-ons. Earlier checkpoint references are historical.

## Optional mixed order and native anchoring (0.17.0)

`kit_ui_focus_order.h` supports a visible, semantic field/button order in one
scope, reverse/wrapped traversal, disabled entries, reorder/removal and atomic
invalid-input rejection. This is a reusable primitive; the trio's existing
button-only traversal is retained until product-specific mixed-order adoption.
`kit_ui_native_text_rect_sdl` maps measured render-space caret geometry into SDL
window coordinates and clips native candidate anchors to the field/window. It
never starts a text-input session. Host focus, lifecycle and field eligibility
remain authoritative. Real macOS SDL session/rectangle tests are adapter proof;
OS IME composition, candidate placement, international keyboard behavior and
Linux/Windows native qualification require separate observed acceptance.
