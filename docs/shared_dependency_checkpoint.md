# Carta shared renderer checkpoint

Qualified 2026-10-06 in retained Main Edit before toolbar/hiking UI migration.

## Exact dependency

The managed archive import uses shared commit
`09ff89a0a32d80981010b15a7d45a2cdf8e5fd9f` and subtree tree
`c5a9cfed9ac92388ee071804990dc982b1449639`.
The dedicated import commit is `4f1105d`; application integration fixes follow
separately. The selected snapshot contains `vk_renderer 1.7.0`,
`vk_runtime 0.6.0`, `kit_ui 0.18.0`, `kit_pane 0.5.0`, `kit_render 0.14.6`,
and `kit_workspace_authoring 0.6.1`. Uncommitted upstream work was excluded.

The single-target manifest pins the full commit, rather than a moving branch.
`bin/update_shared_subtrees.sh --method archive` now resolves full immutable
commit IDs directly after fetch. Its fixture tests qualify both normal branch
imports and an older exact commit after the branch advances, including a clean
no-change repeat. Carta's prior vendor backports remain recoverable at `d12a1d7`;
no hand-edited vendor patch is layered over this exact snapshot.

## Integration boundary

Carta links the newly separate `kit_render_command_validation.c` translation
unit in the application and direct-source tests. Vulkan frame synchronization,
retired resources and surface recovery belong to shared `vk_renderer`.
Carta delegates failed frame acquisition to `vk_renderer_recover_surface` rather
than rebuilding shared frame internals. App-owned geography, camera policy,
terrain readers, tile residency, route drawing, input and diagnostics remain
local. These small compatibility repairs do not migrate toolbar or hiking UI.

## Qualification

A fresh `build/ui-adoption-09ff89a` avoids stale shared ABI/object reuse.
Focused viewport, terrain raster/vector, history, pin layout, idle and Main Edit
contract tests pass, along with shared texture-corner, frame-submission and render
command-validation tests. Native 32-second road and rotated hiking/terrain
replays each cover eight phases, with zero Vulkan validation messages and no
tile-presenter/lifecycle invariant failures. The terrain replay exercises
resident raster tiles, vector contours and labels. Lossless captures confirm
rotated texture/route alignment and sidebar clipping.

The same terrain replay runs continuously on SDL dummy/software fallback;
that lane establishes execution and invariants, not Vulkan/SDL pixel equality.
A separate native resize/readback proof establishes Retina drawable extents
`2560x1440 -> 2880x1600`, one surface recreation and zero validation warnings
or errors. Fullscreen, external monitor/DPI changes, device loss and native
Linux/Windows presentation are not newly qualified by this slice.

See the workspace private MapForge adoption audit for aggregate/headless and
clean-source package results, exact receipts and preservation checks.

## Next boundary

The four workspace tabs and normal Hiking & History controls adopt shared
presentation/semantic activation through the host described in
`docs/shared_control_adoption.md`. Product actions, layout and persistence remain
local. Next migrate bounded text/modal focus, then pane/window hosts; other
Map/places HUD controls remain explicit follow-ups. Availability of newer kit
modules is not complete adoption. Requalify map/terrain after each integration.

Application VERSION remains 0.3.0. Canonical source, installed Desktop apps,
personal data and published region packs remain separate from these local
source/package checkpoints. Local proof does not establish a release or a
Desktop replacement.
