# Pane composition contract

`kit_pane >= 0.4.0` adds an optional caller-owned `KitPaneComposition` snapshot.
`core_pane` continues to own layout solve, constraints and splitter math;
`core_pane_module` and `core_pane_snapshot` retain their registry/schema roles.
Applications own module dispatch, topology edits, persistence and product purpose.

Build descriptors after authoritative layout solve. Use stable nonzero pane IDs,
not row positions. The bounded snapshot derives shell, header and padded content
rectangles and intersects all three with the host viewport. Header height zero
supports an existing product header inside content without imposing new chrome.
A build rejects duplicate IDs, invalid/non-finite geometry and capacity overflow
without modifying the previous snapshot. Empty and collapsed panes are valid.

Draw pane chrome inside visible shell clipping; draw content inside visible
content clipping. Queued hosts use their existing nested `kit_ui` clip stack.
SDL hosts may use the optional `kit_pane_composition_sdl` begin/end adapter, which
intersects and restores the existing clip. Coordinates, renderer scale and font
metrics are supplied by the host. Rectangular clipping does not promise a curved
content mask. Borrowed queued text must remain alive through frame submission.

Use the same visible snapshot for pane-level input. Half-open boundaries assign
shared edges to one pane. Last descriptor is topmost; disabled shells occlude
lower panes. `KitPanePointerOwner` captures the press pane through release even
outside it. The host checks the target control's release condition. Modal,
splitter, focus-loss and hidden/disabled-owner takeover must cancel capture.
This primitive does not replace control capture or invent domain actions.

The initial trio adapters preserve product layouts: Orchestra leaf modules,
Echo navigation/detail/graph, and DataLab picker/browser/preview/directories.
Pane reassignment, docking, unified persistence and a general pane host are later
contracts, not implied by this geometry and input composition slice.

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
