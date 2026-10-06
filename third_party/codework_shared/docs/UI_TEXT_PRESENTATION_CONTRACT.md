# Shared text presentation contract

Optional `kit_ui 0.16.0` builds on the bounded edit/focus contract. Core owns domain state, the kit owns measured presentation, and hosts own field eligibility, viewport/font/theme policy, product actions and platform input sessions.

## Layout and storage

`KitUiTextPresentation` owns its display and NUL-terminated row text. The complete object remains alive and unchanged until queued frame submission completes. Independent fields need independent storage. The layout never mutates the committed buffer. Invalid inputs, nonfinite metrics, display overflow (8192 bytes) or more than 256 rows fail explicitly and preserve the previous presentation. Measurement callbacks receive valid UTF-8 runs, return finite nonnegative widths, and use the viewport coordinate space. Hosts apply font zoom/DPI exactly once.

Single-line presentation follows the caret horizontally. Multiline editing supports explicit newlines and measured hard wraps at scalar boundaries; it does not discard spaces or split UTF-8 characters. Hosts choose vertical scroll and whether a changed caret is revealed. Shared hit mapping uses the same measured rows and accounts for scrolling/composition replacement. Cursor affinity at soft wrap belongs to the following row. Composition displays in place of selected committed text without committing it; its entire range is underlined and the current composition segment is selected. Escape/input submission still follow the edit contract.

## Painting

Selection backgrounds precede their row text; preedit underlines and caret follow. Geometry intersects the field viewport. The queued adapter additionally composes the field clip with the enclosing UI clip, restores it on success, and restores command count/clip depth on recording failure. Its row text pointers borrow presentation storage, never loop-local scratch. The optional SDL adapter invokes the host text drawer synchronously and restores enclosing clip, draw color and blend mode. Theme accent/selection/text colors are supplied by the host.

## Qualification and limits

Run generic edit/presentation tests, optional platform adapter tests, real host field replays and actual captured output at 1x/2x. Test selection, composition isolation, scroll reveal, newline/wrap positions, clipping, field independence and text lifetime. Linkage/command inspection does not prove renderer output. Captured output does not prove OS IME candidate/session behavior.

The reference rollout is Orchestra ingest root, Echo search/title/body/DB/numeric fields, and DataLab picker path/filter. Native Linux/Windows, OS clipboard/IME candidate placement, mixed focus traversal, text undo, nested modals, grapheme movement, bidi and shaping remain separate boundaries. Body editing now uses measured scalar hard wraps; read-only document wrapping remains host-owned.

Row origins are top-left positions. The queued adapter converts row y to the existing kit_render vertically centered text origin; the SDL callback consumes top-left positions. Both use the same measured line height.
