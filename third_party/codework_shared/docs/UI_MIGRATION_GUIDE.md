# Shared UI migration sequence and per-program recipe

The current proving trio is Orchestra, Echo and DataLab. A program can keep its
own layout, domain content and pane names while adopting the same mechanics.

## Iterations already established

1. Rounded surfaces and centered measured button labels: shared appearance,
   actual rendered output, existing SDL reference path and Vulkan proof.
2. Stable button surfaces: semantic IDs, press origin/release, capture,
   keyboard activation, disabled controls, focus and modal takeover.
3. Rendering contract fidelity: transform and textured-quad behavior, explicit
   capability/failure handling and captured output rather than linkage claims.
4. Bounded text editing/modal focus: UTF8 scalar-safe cursor/selection, transient
   preedit, clipboard adapter and host-owned save/cancel/session eligibility.
5. Shared measured text presentation: the same rows, caret/selection/preedit and
   hit geometry, with independent frame-lived field storage and clip restoration.
6. Pane composition foundation (kit_pane 0.4.0): stable IDs, validated shell,
   header/content and visible geometry, clipping and press-owner routing.
7. Pane host behavior (kit_pane 0.5.0): lifecycle/input takeover, nested
   core_layout edits and reusable bounded header slots, adopted in the trio.
8. Fullscreen/window lifecycle (kit_ui 0.18.0, vk_renderer 1.6.0): independent
   logical/drawable/render extents, transition cancellation, suspension and
   bounded swapchain recovery, with actual macOS trio loops/captures.

These are additive layers. Core owns pane topology/constraints, module/snapshot
meaning, domain state and revision semantics. Kits own reusable expression and
input mechanics. Hosts own layouts, providers, resources, actions, history,
persistence and native text-input eligibility. SDL remains optional; DataLab is
the SDL drawing reference even where Vulkan presents a composed canvas.

## Repeatable migration for one program

1. Read the Main Edit runbook. Record canonical/Main Edit identity, dirty ownership,
   worktree inventory, active processes and stable package identity. Use the
   retained writer; do not overwrite other feature work.
2. Inventory actual panes, nested views, editable fields, modal scopes, dividers,
   headers and domain actions. Separate source wiring from runtime adoption.
3. Select an immutable accepted shared pin. Sync through the managed subtree
   manifest, keeping the import commit separate from integration. Verify all
   adopted module files against that pin, including core_layout for pane edits.
4. Map durable pane IDs to the existing solved layout. Build one composition in
   render coordinates for drawing, clipping and input. Overlay panes retain their
   z-order; nested leaf regions route independently. Do not inject new geometry
   solely to imitate another program's layout.
5. Synchronize the pane host before routing. Let the existing kit_ui surface own
   control activation and text focus. Cancel old presses and drags on takeover,
   focus loss, hidden/removed owners and stale geometry. Rebuild after resize.
6. Wrap splitter begin/update/commit/cancel. Save app payload and history before
   mutation, preview with core_pane constraints, cancel to restore the payload
   and revision state, and persist only accepted changes. Preserve any outer
   authoring session; a round-trip/no-change drag produces no new revision.
7. Replace selected header action placement with shared bounded slots. Use the
   same visible slot rectangle for drawing/registration and existing semantic
   action IDs. Measure labels, apply scale once, keep title text clipped and
   preserve queued text lifetime. Retain omitted actions elsewhere when needed.
8. Verify in order: clean build; shared module + production-linked host replay;
   normal regressions/headless smoke and actual native resize/capture. Test
   cancel, cross-pane release, nested clips, disabled/hidden owners, no-op and
   accepted changes. Linkage or command emission alone is insufficient.
9. Align public/current-truth docs, private rollout/bucket controls, scaffold
   requirements and the selected Atlas records. Build/verify only the separate
   Main Edit comparison package, and prove stable bundles remain unchanged.
10. Stop at the reviewed program boundary. Canonical adoption, app VERSION,
    release/publication, Registry and remote platform acceptance are separate.

## Window lifecycle checkpoint

Observe actual SDL geometry before routing and after draining events. Use
`kit_ui_window_refresh_sdl`; apply render-coordinate conversion once and reuse the
same extent for painting, hits and caret placement. Cancel stale pane/control
capture at invalidating window events before text/control consumers. Suspend
submissions for hidden/minimized/nonpositive drawables and keep a bounded host
tick for restoration. Compare actual drawable extents independently of logical
layout and any bounded software render canvas. Recreate presentation resources
without replacing domain content. Rebuild all public Vulkan context consumers.

See [the window lifecycle contract](UI_WINDOW_LIFECYCLE_CONTRACT.md). Qualify unit
mapping/fence failures, production-linked cancellation, and the opt-in actual app
loop `CODEWORK_WINDOW_LIFECYCLE_PROOF=<directory>` through fullscreen enter/exit,
resize, hide/show and minimize/restore, plus captured output and continued frames.
Proof runtime data and capture files belong in an isolated task directory. DataLab
plain SDL can be selected with `DATALAB_RENDER_BACKEND=sdl`; a drawable beyond its
bounded canvas can be tested with `CODEWORK_WINDOW_PROOF_LARGE=1`.

## Next boundary

The trio fullscreen/window slice is qualified on macOS. Apply this recipe to one
selected program at a time, retaining its layouts/domain behavior and proving
its actual controls, cancellation, rendering and native window lifecycle before
moving on. Mixed traversal is kit preparation; human OS IME sessions, external
monitor/DPI movement, native Linux/Windows and exclusive fullscreen require their
own acceptance. Generalized docking/module-provider lifecycle is later work.

## Operational adoption workflow

Use this workflow for each existing app. It is also the UI-contract checklist
for a new app after its baseline scaffold exists. The historical iterations
above describe how the shared contracts matured; a new adopter need not replay
that development history. Adopt the accepted behavior through small vertical
slices. Do not build an entire new framework before proving a host integration.

### A. Establish the current baseline

Read the retained Main Edit runbook; freshly record canonical/Main Edit branches,
commits, versions, dirt, divergence, worktrees and process ownership. Preserve
existing writers and unrelated work. A clean lane today is not a reservation
for a later implementation. Record normal app/package/runtime identity and use
isolated fixtures for proof; package/refresh only when authorized.

Inventory all real surfaces: startup/pickers, main HUD, tabs/toolbars, nested
panes, scrolling lists, canvas/viewport, overlays, dialogs and native file panels.
For each, record renderer path, coordinate extent, input owner, actual semantic
actions, editable fields, modal/takeover rules, capture cancellation, persistence
and existing tests. Include unusual modes and disabled/hidden controls. Shared
linkage or a matching module VERSION does not establish active adoption.

Deliverable: a source-backed gap ledger plus one proposed first slice, with
explicit required commands and stop condition. Skip complete/not-applicable
areas; leave unknowns visible rather than inventing features or a required rewrite.

### B. Establish dependency and presentation parity

Choose an immutable accepted shared commit and record module versions/API/source
files. Review the full managed subtree diff before import, including non-UI
modules: an accepted shared commit can still be a large jump from an older app.
Do not absorb unknown unrelated API changes silently. Use the managed subtree
manifest; commit shared source first and keep imports separate from host fixes.
An accepted source/documentation baseline is listed below; recheck it at adoption.

Clean-rebuild consumers when public structures or dependency headers change.
Prove the existing renderer/default/oracle paths, fonts, clips, texture filtering,
transform/UV/tint behavior where used, and window/logical/drawable/render geometry.
Adopt shared window observation, invalidation, fullscreen and suspension/recovery
before relying on new geometry for interactions. Retain domain/cache owners.
A composed SDL canvas presented with Vulkan is a supported path; native GPU UI
migration and performance optimization are separately measured work.

Deliverable: renderer/window parity and a reusable native proof route through the
actual host loop. No control redesign is needed to pass this stage.

### C. Adopt surfaces and semantic controls by region

Start at the common button painter, then migrate one named control region at a
time (for example main HUD, one panel tab, toolbar or dialog). Reuse rounded
appearance, actual text measurement and shared label placement. Preserve action
meaning, theme scale and palette; draw and register the same visible rectangle.
Give controls stable semantic keys and use press-origin/release activation,
keyboard focus, disabled/hidden eligibility and takeover cancellation. Do not
attach button activation semantics to brushes, panning or continuous sliders;
those tools retain their own capture and history policies.

Prove press-inside/release-outside, reorder, hidden/disabled owners, modal
interception, focus loss and stale release after resize. Compare actual 1x/2x
rendered output. Once a region passes, cut over that region and remove its
superseded duplicate helper; keep a clean checkpoint before the next region.

Deliverable: region coverage recorded in the ledger, including the main HUD.
A styled button alone does not mark semantic control behavior complete.

### D. Adopt text and modal behavior where present

Inventory actual editable fields first. Use bounded caller-owned text storage,
shared UTF-8 scalar editing and measured presentation, with the same row geometry
for drawing, hits, caret, selection and preedit. Give simultaneous fields
independent submission-lived backing storage. Hosts retain validation, numeric
policy, save/cancel, domain undo, field eligibility and native input-session
lifetime. Preedit is transient and never a domain commit.

Prove field/button/shortcut precedence, modal takeover and semantic restoration,
clipboard policy, capacity failure and canceled composition. Existing one-modal
restoration is bounded; nested modal stacks and product-wide mixed traversal
need explicit host policy. Mark absent text entry not applicable; do not add new
editing features solely to consume a library. Human OS IME workflow and shaping
claims require their own native acceptance beyond synthetic event replay.

Deliverable: field/modal inventory and per-field evidence, with unsupported
behavior explicitly deferred.

### E. Adopt pane composition and transactions

Map existing durable pane IDs/topology to one composition snapshot in the chosen
render coordinates. Use shared header/content/visible clips, half-open hits,
nested z-order and pointer ownership. Keep program-specific layouts and provider
meaning. A zero-height shared header can preserve an existing product header.

Wrap splitter edits with begin/preview/commit/cancel. Save domain payload and
revision/history state before mutation; restore both on cancellation. Persist
accepted changes only. A no-op drag produces no extra revision. Nest in existing
workspace authoring sessions without resetting or committing the outer draft.
Use bounded shared header slots only for existing eligible actions.

Prove nested clips, cross-pane capture, hidden/removed owners, tiny layouts,
canceled/no-op/accepted drags and save/reopen parity. Docking, arbitrary provider
insertion, layout persistence formats and new pane types are separate features.

Deliverable: actual pane/provider coverage and transaction proof, not merely a
shared pane include or a new common-looking layout.

### F. Qualify complete product workflows and delivery

Run verification in order: clean compile; touched shared and production-linked
host tests; broad product/headless checks at a meaningful checkpoint. Capture
actual output and execute real window resize/fullscreen enter/exit, hide/show,
minimize/restore, continued rendering and canceled gestures. Include retained
content/cache recovery and large drawables where a bounded canvas exists.

Exercise representative product modes with isolated copies of fixtures: edits,
undo/redo, save/reopen, import/export and empty state. Check that migration has
not changed domain output, indexed/raster fidelity, revisions or preferences.
Synthetic input tests and a first-frame screenshot cover different claims.
Native OS, multi-monitor/DPI, exclusive fullscreen and IME acceptance remain
individually recorded evidence, not inferred from macOS source or headless tests.

Align public contracts/current truth, private bucket status and scaffold/Atlas
projections. Only then build/verify the separate Main Edit comparison package
when authorized; verify installed source/binary identity, signature, namespaces
and stable-app preservation. Canonical adoption, app VERSION, production
packaging/publication and Registry remain separate subsequent boundaries.

Deliverable: a clean checkpoint, compact closure record and explicit next region
or next program. Stop rather than adding adjacent cleanup after the slice passes.

## Per-program acceptance ledger

Keep one small ledger in the owning private bucket; link public adopted behavior
and existing test/native receipts. Use the existing scaffold status vocabulary:
`complete`, `partial`, `gap`, `not_applicable`, `deferred`, `unknown`.

| Contract / region | Source owner + renderer/coordinates | Shared pin/API | State | Actual evidence | Next bounded action |
|---|---|---|---|---|---|
| Renderer/window | Host adapter and loop | Accepted dependency pin | unknown initially | clean build, fault/mapping, native captures | One stated gap |
| Appearance/buttons | Main HUD, each panel/dialog | kit_ui painter/surface | per region | output + press/release replay | Next region |
| Text/modal | Each real field/scope | editor/presentation/native adapter | per field or N/A | replay + separately native session | Named boundary |
| Panes/splitters | Each real pane/provider | kit_pane / core_layout | per pane | clipping/capture/transaction/reopen | Named boundary |
| Product/delivery | Domain workflows and package | Host contracts | per mode/platform | output parity + installed identity | Adoption decision |

Record imported, wired and tested evidence explicitly. Mark `complete` only when
required actual-host gates pass. One region/platform can be complete while
another remains partial. The ledger is an implementation checklist, not a new
Registry, release-approval or universal conformance enforcement system.

## Shared refinement feedback loop

For a newly discovered mismatch choose exactly one reuse disposition:
`reuse-adopted` if the accepted API fits; `reuse-extend` for a small additive
shared contract; `reuse-deferred` if the behavior is product-specific or the
abstraction is premature. Core stays UI-free; optional kit adapters express
mechanics; app code retains policy, resources, history and persistence.

For an extension: write the smallest contract and meaningful failure/output
fixtures, implement/version/document shared code first, prove one host, then rerun
the affected Orchestra/Echo/DataLab reference gates before declaring a new
baseline. Run all trio gates when the boundary or common behavior changes;
docs-only or isolated local wiring does not require every expensive native suite.
Import the committed pin into selected adopters through the managed lane. Update
this guide's baseline/history and per-program ledgers; later adopters get the
refinement through an explicit reviewed update. Do not create one-off copies of
shared mechanics in each app or silently force all programs to track shared HEAD.

## Connection to scaffold and refinement passes

This workflow sits in contract alignment, above the baseline scaffold and below
production release. IR1 covers intake/normalize/route/invalidate; RS1 covers
update/render-derive/render-submit. The UI stages are not seven new scaffold
passes and do not reopen a completed R0-R6 cycle automatically.

Use existing refinement passes when evidence warrants: R0 for module placement,
R1 for duplicated mechanics/reuse, R2 for ownership and draft/commit state,
R3 for actionable renderer/capacity/recovery failures, R5 for production-linked
proof, and R6 for captured product workflows. R4 remains the separately scoped
security pass; routine UI adoption is not a mandatory new security audit. Select
one primary pass/layer per implementation slice and stop at its contract.

## Accepted baseline and proving responsibilities

The current accepted source is
`854b51ff57c756b4565021fe759c27a459efbfd3`; documentation checkpoint is
`9043b2fe4b14b76fe7b94d4cc64cb817d59b41f5`. The sketCh pilot imports the
committed code plus workflow checkpoint `7b37ad8c6ca7eafa679ab820aab9cac033d46c11`
with identical accepted UI code (recheck the full delta before import). Accepted modules:
kit_ui 0.18.0, kit_pane 0.5.0, core_layout 0.2.1, kit_render 0.14.6,
vk_renderer 1.6.0 and vk_runtime 0.6.0. Use exact code/source checks: unrelated
uncommitted renderer work may carry the same numeric VERSION.

Orchestra proves command UI plus workspace/layout authoring; Echo proves nested
panels, controls and text; DataLab proves SDL drawing and composed Vulkan
presentation, including bounded canvas scaling. Their completed macOS evidence
is a reference cohort, not a claim that every future program or platform passes.

Contract references:
[controls](UI_INTERACTION_CONTRACT.md), [text editing/focus](UI_TEXT_FOCUS_CONTRACT.md),
[text presentation](UI_TEXT_PRESENTATION_CONTRACT.md),
[pane composition](UI_PANE_COMPOSITION_CONTRACT.md),
[pane host behavior](UI_PANE_HOST_CONTRACT.md),
[window lifecycle](UI_WINDOW_LIFECYCLE_CONTRACT.md),
[render fidelity](RENDER_COMMAND_FIDELITY.md).

## First pilot feedback: sketCh

The retained Drawing Program Main Edit pilot now exercises this sequence.
Its canonical source and stable app remain separate. The host reuses existing
kit_ui/kit_pane contracts; no new core/kit API or version was required.

1. Review the full subtree delta, including linked non-UI modules. Record
   **imported**, **linked**, **wired**, **tested** and **native-qualified**
   separately. sketCh imports MemDB/scene-compile changes without linking them;
   its linked authored-texture additions retain existing export/indexed tests.
2. Establish one render extent function used by drawing, upload, input mapping
   and capture. Native drawable metrics stay physical. A bounded canvas must
   scale uniformly to preserve aspect ratio; test a Retina extent above its cap.
3. Bind discrete controls to operation IDs and domain identity. Equal labels
   in different sections are legitimate: font/theme “DAW Default” exposed this
   in the actual pilot. Value chips are readouts, never focus targets. Verify
   domain eligibility in rendering/focus as well as legacy action dispatch:
   indexed sketCh intentionally disables Layer instead of exposing an inert tab.
4. Audit nested clipping before adding a composition parent. Old queue painters
   that clear SDL clipping can escape the pane. Intersect child clips and restore
   parent clip/color/blend state; test the production adapter, not a duplicate.
5. Keep splitter payload rollback separate from shared revision rollback. A
   canceled nested drag must preserve its outer draft; no-op and accepted runtime
   drags need distinct revision assertions. Existing pane titles may remain
   content-owned until header/content extraction is actually in scope.
6. Qualify automatic renderer recovery against packaged resources, not only
   explicit resize helpers. sketCh now scopes automatic pipeline recovery to its
   resolved shader root as well. Compile-time workspace paths are insufficient
   evidence for an installed package.
7. Prove failures survive cleanup. The pilot exposed a host loop that replaced
   its render/probe failure with successful shutdown. Preserve the original
   result, and require probe completion plus fresh captures; exit zero alone
   cannot close an acceptance row. Clear only task-owned previous captures.
8. Run standard and alternate profiles through the actual loop. A text/IME row
   can be N/A when no inline fields exist; do not invent editors to fill a matrix.
   Keep native dialogs and continuous drawing gestures in their existing lanes.
9. Package from a checkpointed Main Edit identity, verify the installed bytes
   and normal launcher resource roots, and compare the stable app manifest.
   Classify release impact using the logical program owner even when the source
   checkout resides under `_worktrees`; this is not release authorization.

[sketCh UI contract](../../_worktrees/drawing_program_main_edit/docs/ui_contract.md)
and the [pilot ledger](../../docs/private_program_docs/drawing_program/audits/ui_adoption_readiness_20261005.md)
record scope, fixtures, exact module pin and acceptance. Header extraction was
deferred in the first pilot and is closed for sketCh by the follow-on below.
Generalized docking/providers, new inline editors, cache optimization, human IME,
external monitor movement and native Linux/Windows remain explicit later rows.
The proving trio remains the baseline cohort for shared contract changes;
app-only adapter adoption does not require rerunning unchanged trio binaries.

## sketCh pane header follow-on feedback

Existing kit_pane 0.5.0 composition/header APIs cover the next adoption step; no
new shared API or subtree update is needed. The app now owns header policy in a
focused adapter and delegates title/action allocation and content partitioning
to the kit. FIT and LAYOUT keep their existing domain/workspace meaning.

- Choose the coordinate contract before extraction. Content callbacks, hit
  classification, fitting and projection must all use content bounds. Remove
  old title offsets from content layout helpers in the same cutover; a clipped
  painter still using shell offsets can draw and hit different rows. sketCh's
  side controls retain their positions while canvas projection uses the content
  center below its header.
- Use measured title/action widths and shared priority omission. A narrow or
  font-enlarged pane may have a title and no action. Test that omitted, hidden
  and disabled slots cannot dispatch, and that a stale action cannot reach a
  differently bound module. Keep operation identity separate from labels.
- Feed real pointer events to pane ownership before semantic control routing.
  Do not feed a release-triggered synthetic product press into pane capture;
  that leaves ownership stuck after the real release. Header actions dispatch
  directly by operation/pane ID instead of synthesizing a content click.
- Test the production frame with a deliberately overpainting content callback.
  Assert header pixels, clipped content bounds, domain hit exclusion, FIT parity
  and exact modal/layout restoration. Then qualify pointer rejection, keyboard
  LAYOUT, modal exclusion and restored focus in actual standard/indexed loops.
- Treat a fixed header policy table as app policy. It is not a dynamic provider
  registry, module picker or docking contract. Those need their own bounded
  mount/remount/resource/persistence slice after the visual comparison.

The current acceptance is the retained sketCh Main Edit and its isolated Desktop
comparison. Canonical adoption/release, other programs and additional native
platforms stay separate. Unchanged shared APIs do not require another trio
build; future shared changes still use Orchestra/Echo/DataLab plus sketCh.

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
