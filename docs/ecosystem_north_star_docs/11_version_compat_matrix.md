# Version Compatibility Matrix

Minimum supported shared-module versions per app.
Last updated: 2026-08-20

`vk_runtime 0.6.0` now proves headless lifecycle, deterministic one-shot
compute, persistent device-local chains with explicit barriers, and
capability-conditional timestamp timing across a seven-size parity sweep on
Apple M2/MoltenVK. It now also exports and explicitly consumes deterministic,
source-bound precompiled SPIR-V for compiler-free hosts. Linux llvmpipe now
passes compiler-free S2-S4 with exact parity, lifecycle/timestamp evidence, and
no observed crossover. A stale report-version literal exposed by that run was
replaced with canonical `VERSION`-derived library identity; corrected remote
identity. The checksum-bound RTX 3060 profile now passes capability, parity,
residency, timing, deterministic repeats, and validation-required variants
with zero warnings/errors; its exact result and 12 reports are retained and
independently verified. Execution-only crossover begins at 16,384 values,
while end-to-end crossover is not observed through 1,048,576 values.
`vk_renderer 1.3.2` consumes the runtime lifecycle through
compatibility wrappers and passes automated validation/readback/resize/capture
proof. Workspace Sandbox retains the first live-shared host/package proof and
currently vendors `vk_runtime 0.6.0` / `vk_renderer 1.3.2`, while MapForge is
the first managed default application adopter of `vk_runtime 0.6.0` /
`vk_renderer 1.3.1`. MapForge's source and packaged self-tests prove strict
validation, runtime/renderer handle identity, readback, real resize/recovery,
capture dimensions, and the existing 2x Retina drawable path. Memory Console,
PhysicsSim, LineDrawing, DAW, Gravity Orbit Sim, GrowthSim, IDE, Dungeon,
Video Editor/Capture, BehaviorSim, Drawing Program, DataLab, and Connected
Mechanics Sim now commit the same managed-default `0.6.0` / `1.3.1`
presentation pair with validation-clean source/package, readback, real resize,
restart, capture, and measured 2x Retina proof. None of these presentation adoptions uses the runtime compute,
residency, or timing workload APIs.

| App | core_base | core_io | core_data | core_pack | core_scene | core_space | core_trace | core_math | core_theme | core_font | core_time | core_queue | core_sched | core_jobs | core_workers | core_wake | core_kernel | core_sim | core_authored_texture | kit_render | kit_viz | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ball_bounce_sim | 1.0.1 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 2.0.1 | 1.0.2 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 0.4.0 | N/A | N/A | N/A | The compact persistent simulation picker directly adopts `core_theme >= 2.0.1` semantic palette tokens and `core_font >= 1.0.2` role/tier contracts through an app-local style adapter; `core_base >= 1.0.1` is their support dependency. The Clang picker plus complete seeded-room, seeded-pair, imported-single, imported-pair, collision-scenario, generated-scene-playback, and generic rigid-3D host families actively adopt exact-source `vk_runtime 0.6.0` beneath `vk_renderer 1.3.2` through one reusable high-DPI SDL compatibility canvas. Proof covers validation-clean shared-handle lifecycle, logical menu input, native capture/readback, resize/recreation, drawable CPU-depth recreation, 2x Retina output, profile/room identity, and explicit SDL fallback for all 49 compatible 3D profiles. The interactive simulation shell continues to adopt `core_sim >= 0.4.0` for fixed-step control-plane state and pass dispatch. The separate 2D random-scene host, scenarios, physics formulas, headless artifacts, worker schemas, Vulkan compute, and `fisiCs` overlay/render proof behavior remain SDL/app-local. Package and managed-subtree proof remain deferred. |
| video_editor | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 2.0.0 | 1.0.0 | 1.0.1 | 1.0.1 | 1.0.1 | 1.0.1 | 1.0.1 | 1.0.2 | 1.0.1 | N/A | N/A | 0.14.0 | N/A | Foundation-level desktop shell links `core_base`, `core_theme`, `core_font`, `kit_render`, `kit_ui`, and `kit_workspace_authoring` from vendored `third_party/codework_shared`, adopts `core_pane >= 0.3.1`, and partially adopts the full execution-core primitive set. `VEWA1-S1/S2/S3/S4/S5` closes shared workspace-authoring entry, active-input capture, pane-overlay, font/theme, accepted-only persistence, and routing refinement while Capture retains app semantics. The committed default presentation path now adopts `vk_runtime >= 0.6.0` beneath `vk_renderer >= 1.3.1` through an app-local high-DPI SDL compatibility canvas with checksum-bound validation/readback/resize/Retina/restart, package, and installed-app proof. GPU video composition, decoder-texture interop, runtime compute, preview/proxy/export meaning, and release state remain outside this adoption. |
| physics_sim | 1.0.0 | 1.0.0 | 1.0.0 | 1.1.0 | 1.0.0 | N/A | 1.0.0 | N/A | 2.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 0.2.0 | N/A | 0.11.0 | 1.0.0 | Strong core spine + kit_viz; trace/data paths are additive/tooling-oriented. Editor shell now also adopts `core_pane >= 0.2.0` for left/center/right pane geometry and `kit_pane >= 0.2.0` for live splitter hover/drag interaction, the retained-scene `2D` editor viewport now partially adopts `core_viewport2d >= 0.2.1` for fit-reset, cursor-anchor zoom, drag-pan, and screen/content transforms while `3D` orbit camera behavior stays app-local, truthful `vf3d` export now depends on `core_pack >= 1.1.0`, the font bridge now depends on `kit_render >= 0.11.0` for shared text-run policy resolution, bounded menu/editor button semantics now depend on `kit_ui >= 0.9.1` through the app-local `physics_sim_ui_button` wrapper, runtime stepping now partially adopts `core_sim >= 0.2.0` for scene-level substep pass routing plus the 3D solver first-pass shell while domain math and render/HUD meaning remain app-local, runtime mesh preview diagnostics now depend on `core_mesh_preview >= 0.4.0` for runtime-scene mesh sidecar path/probe/metadata attachment plus transform-aware editor/runtime overlay preview bounds, default-solid mesh fluid obstacles now load actual `core_mesh_asset` runtime geometry for app-local voxelization into 3D obstacle occupancy instead of using preview payloads as solver truth, and mesh instances can switch into the existing emitter flow as attached runtime-mesh emitters that clear solid occupancy and emit density/velocity/heat through actual mesh footprints. `PSWA1-S1/S2/S3/S4/S5` now depend on `kit_workspace_authoring >= 0.5.0` for the shared entry chord, active reserved-trigger capture, shared active pane-overlay button layout/hit testing, shared full-screen Font/Theme layout/hit/action semantics, accepted-only host persistence routing, and closeout as the next graphics/editor proving host before RayTracing. |
| daw | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | 1.0.0 | N/A | 2.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.1 | 1.0.1 | 1.0.0 | N/A | N/A | 0.14.0 | 1.0.0 | Runtime/theme/font + mainthread execution-core path (`queue/sched/jobs/wake/kernel`) adopted; Slice 2 pack/data parity guard, Slice 3 additive data-contract hardening, Slice 4 shared `core_trace` diagnostics lane, and Slice 5 shared `core_workers` async diagnostics lane gate are active. The centralized UI font lane now also depends on `kit_render >= 0.14.0` for vendored `kit_render_external_text.*` draw/measure runtime adoption while preserving the DAW-specific preset baseline. `DWA1-S0/S1/S2/S3/S4/S5` now uses `kit_workspace_authoring >= 0.5.0` for entry/toggle semantics, active overlay button geometry, shared full-screen Font/Theme layout, shared hit/action mapping, app-owned live text/font/theme preview mutation, accepted-only theme/font/text-size persistence, and closeout; default vendored builds still require the separate TimerHUD session/snapshot subtree follow-up. `gravity_orbit_sim` is queued as the next recommended authoring host. |
| dungeon | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | 2.0.0 | 1.0.1 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | 0.2.0 | 0.2.0 | 0.14.0 | N/A | SR2 adopts `core_time >= 1.0.0` for monotonic frame timing and dirty/wake loop diagnostics. SR3 adopts `core_sim >= 0.2.0` for fixed-step pass routing and frame outcomes. SR4 adopts `core_base >= 1.0.0`, `core_theme >= 2.0.0`, `core_font >= 1.0.1`, and `kit_render >= 0.14.0` for render/text policy and null-backend command recording while draw semantics remain app-local. ITF1-ITF3 adopt managed `core_authored_texture >= 0.2.0` for exact indexed palette and fixed-cell atlas validation while Dungeon owns JSON, stable tile keys, resources, diagnostics, and procedural fallback. The save/session/config slice adopts `core_io >= 1.0.0`. The committed default Clang/package presentation path adopts `vk_runtime >= 0.6.0` beneath `vk_renderer >= 1.3.1` through a high-DPI SDL compatibility canvas with canonical-source, validation/readback/resize/Retina/restart, package, and live indexed-tileset application proof. Gameplay rules, SDL input, renderer policy, and compute workloads remain app-local or deferred. |
| datalab | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | 2.0.0 | 1.0.1 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 0.14.0 | 1.0.0 | Focused on base/io/data/pack + kit_viz ingestion/render paths, with shared `core_font` text rendering now active in UI overlays. `WASR-S3` now also depends on `core_theme >= 2.0.0`, `kit_render >= 0.14.0`, and `kit_workspace_authoring >= 0.5.0` for the shared font/theme authoring panel layout, hit IDs, preset mappings, and button-to-action classification. Active runtime HUDs now also depend on vendored `kit_ui >= 0.11.1` for alpha-aware floating HUD style, button/readout row layout, nested corner/inset sizing, and the optional SDL rounded panel/button/readout adapter; DataLab maps the active theme/custom palette into `KitUiHudStyle` while playback policy, session content, file stepping, manual edge-wrap behavior, and runtime prefs stay app-local. DataLab now also adopts vendored `kit_graph_timeseries >= 0.2.2` for bounded trace graph view computation, zoom, hover inspection, plot draw commands, and hover overlay commands while trace sample/session meaning, cursor policy, and SDL replay stay app-local. Protected `0.3.6` source defaults picker and session presentation to `vk_runtime >= 0.6.0` beneath `vk_renderer >= 1.3.1` through a high-DPI SDL compatibility canvas; exact-source validation/readback/resize/capture/restart, package, and real-host proof is green while SDL fallback, profile semantics, compute, and release state remain separate. |
| drawing_program | 1.0.0 | N/A | N/A | 1.0.0 | N/A | N/A | N/A | N/A | 2.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 0.1.1 | 0.14.0 | N/A | Centralized SDL text/runtime lane keeps the shared `ide` baseline while the host defaults to vendored `third_party/codework_shared`; pane shell support depends on `core_pane >= 0.3.0`, `core_layout >= 0.1.0`, and `core_pane_module >= 0.1.0`, the live splitter-resize lane uses `kit_pane >= 0.3.0`, completed WA1 host attach uses `kit_workspace_authoring >= 0.5.0` and `kit_render >= 0.14.0`, and authored-texture export uses `core_authored_texture >= 0.1.1`. The committed default Clang/package presentation path now adopts `vk_runtime >= 0.6.0` beneath `vk_renderer >= 1.3.1` through an app-local high-DPI SDL compatibility canvas with exact-source, validation/readback/resize/Retina/restart, real-frame, and package proof. SDL/fisiCs remain oracles; compose policy and compute workloads remain app-local. |
| ray_tracing | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 2.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | 1.0.0 | N/A | N/A | 0.2.0 | 0.1.1 | 0.14.0 | 1.0.0 | Uses scene/space/time + trace tooling; data/pack are partly additive diagnostics/import helpers. Pre-`TP-S3` runtime contract preflight lane is wired, shared `core_scene_compile` baseline is available in build/test surface, the font/runtime bridge plus wrapped-text path depend on `kit_render >= 0.14.0`, and the scene editor pane shell depends on `core_pane >= 0.2.0` plus `kit_pane >= 0.2.0`. The object list now depends on vendored `kit_ui >= 0.11.2` for scroll evaluation and top-anchor content sizing while row drawing and selection stay app-local. The native editor navigation bridge adopts managed `core_viewport3d >= 0.1.0` for double-target pan, anchor zoom, and controlled orbit transitions while projector construction, zoom-domain policy, input, picking, overlays, preview rendering, final geometry, and BVHs remain RayTracing-owned. Runtime frame routing uses `core_sim >= 0.2.0`, runtime mesh preview diagnostics use `core_mesh_preview >= 0.4.0`, the native tile scheduler uses `core_queue >= 1.0.0` plus `core_workers >= 1.0.0`, dirty-rect preview uses `shared/vk_renderer >= 1.1.1`, Workspace Authoring uses `kit_workspace_authoring >= 0.5.0`, and authored-texture validation uses `core_authored_texture >= 0.1.1` while their host policies remain app-local. |
| line_drawing | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.1.0 | N/A | 1.0.0 | 1.0.0 | 2.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 0.14.0 | N/A | Export lane validates/emits canonical primitive payloads through the promoted `core_scene` typed contract surface, the shared font-runtime bridge depends on `kit_render >= 0.14.0`, and the pane-resize host depends on `core_pane >= 0.2.0` plus `kit_pane >= 0.2.0`. The imported STL/runtime mesh viewport depends on `core_mesh_preview >= 0.5.0`; the free-view navigation bridge adopts managed `core_viewport3d >= 0.1.0` for effective-target pan/orbit/anchor-zoom/frame transitions while LineDrawing retains camera/Grid storage, projection, depth rasterization, input arbitration, authoring, picking, colors, renderer caching, and quality-settle policy. `LDWA1-S1/S2/S3/S4` depend on `kit_workspace_authoring >= 0.5.0` for shared workspace entry, overlay layout/hit testing, font/theme action classification, and accepted-only persistence routing. The committed managed presentation baseline now uses `vk_runtime >= 0.6.0` beneath `vk_renderer >= 1.3.1`; validation/readback/capture/resize/Retina/restart proof is green while compute APIs and app-owned editor/render policy remain outside this adoption. |
| workspace_sandbox | 1.0.0 | N/A | N/A | 1.1.0 | N/A | N/A | N/A | N/A | 2.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 0.14.0 | N/A | Active UI text renders through shared `kit_render`, and the vendored subtree host depends on `core_config >= 0.1.0`, `core_layout >= 0.1.0`, `core_action >= 0.1.0`, `core_pane >= 0.2.0`, `core_pane_module >= 0.1.0`, `core_pane_snapshot >= 0.1.0`, `kit_pane >= 0.1.0`, and `kit_workspace_authoring >= 0.5.0`. Its managed subtree now carries `vk_runtime 0.6.0` / `vk_renderer 1.3.2`; rollout proof covers validation-clean lifecycle, resize recovery, readback/capture, package behavior, and packaged shader-root resolution. Compute APIs remain unused. |
| mapforge | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | 1.0.0 | 1.0.0 | N/A | 2.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.1 | 1.0.0 | N/A | 0.14.0 | N/A | Tile-loader execution-core lane is integrated; Slice 2 routes vk-asset ready handoff through shared `core_queue`, Slice 3 routes `vk_poly_prep` in/out queues through shared `core_queue`, Slice 4 adds deterministic diagnostics contract gates for `meta.dataset.json` and trace lanes/chunks, Slice 5 hardens strict trace pack parity (exact chunk count/order and deterministic payload sizes), runtime perf diagnostics now require `kit_runtime_diag >= 0.1.0`, the pin-workflow shell now also depends on `core_pane >= 0.3.1` for left-pane + constrained-viewport split solve under the header, and `MFWA1-S1/S2/S3/S4` now depends on `kit_workspace_authoring >= 0.5.0` plus `kit_render >= 0.14.0` for shared authoring semantics while SDL/Vulkan rendering and map runtime behavior remain app-local. The managed default subtree now directly adopts `vk_runtime >= 0.6.0` and `vk_renderer >= 1.3.1`; source and package proofs cover validation, lifecycle-handle identity, readback, real resize/recovery, capture dimensions, and 2x Retina drawable extents while Carta retains SDL fallback and presentation policy. |
| ide | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | 2.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.1 | 1.0.0 | N/A | N/A | N/A | Full execution-core stack integrated in current loop path. `IDEWA1-S0/S1/S2/S3/S4/S5` depends on `kit_workspace_authoring >= 0.5.0` while IDE owns drawing, pane labels, and preference storage. IDE also depends on vendored `core_pack`, `core_pane_snapshot >= 0.2.0`, and `core_pane_module >= 0.2.0` for the IDEWAP4-S3 fail-closed presentation-profile adapter; layout, profile-file policy, and runtime mutation remain IDE-local. IDE also depends on `kit_graph_struct >= 0.8.1`, `core_viewport2d >= 0.2.1`, and vendored `kit_ui >= 0.11.1`. The committed default presentation path now adopts `vk_runtime >= 0.6.0` beneath `vk_renderer >= 1.3.1` with exact-source, validation-clean lifecycle, readback, real resize/recovery, capture, 2x Retina, restart, and packaged-app proof. Editor/compiler behavior and runtime compute remain app-local. |
| behavior_sim | 1.0.0 | N/A | N/A | P | N/A | N/A | 1.0.0 | N/A | 2.0.0 | 1.0.1 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 0.4.0 | 0.14.0 | N/A | Current UI lane adopts shared `core_theme`, `core_font`, `kit_render`, `kit_ui >= 0.9.1`, `core_pane >= 0.2.0`, and `kit_pane >= 0.2.0` through the refreshed vendored subtree host. Runtime pass execution depends on `core_sim >= 0.4.0` for persistent loop state, pass-order validation, status/reason diagnostics, frame records, and behavior-preserving ordered stub-pass routing through a 30ms simulation shell; headless diagnostics now also depend on `core_sim_trace >= 0.1.0` over `core_trace >= 1.0.0` for shared control-plane sample/marker counts while behavior-domain world/entities/metrics stay app-local. `BWA1-S1/S2/S3/S4/S5` now also depend on `kit_workspace_authoring >= 0.5.0` for the shared `Alt+C`/`Alt+V` entry chord, active authoring reserved-trigger handling, shared overlay button layout/hit testing for active-only pane mode, the shared font/theme layout/hit/action surface for text-size preview, accepted-only persistence routing through BehaviorSim session capture, and closeout as the fourth proving host; active SDL draw ownership plus sim-domain behavior remain app-local. |
| gravity_orbit_sim | 1.0.0 | 1.0.0 | N/A | P | N/A | N/A | 1.0.0 | N/A | 2.0.0 | 1.0.1 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 0.4.0 | 0.14.0 | N/A | Current vendored-subtree host depends on `core_sim >= 0.4.0`, `core_sim_trace >= 0.1.0`, `core_pane >= 0.2.0`, `kit_pane >= 0.2.0`, `core_io >= 1.0.0`, `core_viewport2d >= 0.1.0`, `core_theme >= 2.0.0`, `core_font >= 1.0.1`, `kit_render >= 0.14.0`, `kit_ui >= 0.9.1`, and `kit_workspace_authoring >= 0.5.0` for the established simulation-control, trace, pane, persistence, camera, policy, button, and authoring seams. The committed clang/package default now uses `vk_runtime >= 0.6.0` beneath `vk_renderer >= 1.3.1`; validation-clean source/package proof covers readback, real resize/recovery, capture, 2x Retina, and restart. SDL keeps window/events and the `fisiCs` renderer oracle; app-specific pane/viewport/render policy, simulation meaning, text/palette policy, and persistence stay local. Compute, residency, and timing workload APIs remain unused. |
| growth_sim | 1.0.0 | P | 1.0.0 | 1.1.1 | N/A | N/A | 1.0.0 | N/A | 2.0.0 | 1.0.1 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 0.4.1 | 0.14.0 | N/A | Current vendored-subtree host depends on `core_sim >= 0.4.1` for Mold and Fire pass execution plus current frame-record/validation behavior, `core_sim_trace >= 0.1.1` over `core_trace >= 1.0.0` for deterministic control-plane trace samples/markers, `core_data >= 1.0.0` for copied `FIELD2D_F32` typed Mold/Fire field snapshots, `core_pack >= 1.1.1` for producer-side Mold/Fire field-frame export and validation, `core_pane >= 0.2.0` plus `kit_pane >= 0.2.0` for the pane shell, `kit_ui >= 0.9.1` for richer button semantics, and `core_theme`, `core_font`, and `kit_render` for visual text/theme policy. The committed default Clang/package presentation path additionally adopts `vk_runtime >= 0.6.0` beneath `vk_renderer >= 1.3.1`; validation-clean source/package proof covers lifecycle identity, readback, real resize/recovery, capture, 2x Retina, restart, shared text, and changing Mold frames. SDL keeps window/events and fisiCs keeps the direct-SDL renderer oracle. Rulesets, fields, launcher/capture policy, solver storage, and palette meaning remain app-local; runtime compute/residency/timing workload APIs remain unused. |
| fisiCs | 1.0.0 | 1.0.0 | 1.0.0 | 1.0.0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | Core usage remains partial/additive; `sys_shims` adoption is the dominant standardization path. |

## Update Rules
- Additional active module minimums not yet represented as columns:
  - `codework_compute_runtime`: `core_headless_job >= 0.2.0` is the normative
    platform-v1 semantic authority for the coordinator's canonical workflow,
    job, event, result, artifact, and worker-capability parser/writer mechanics.
    The Python runtime owns persistence, projections/replay, CAS paths, and
    later process supervision; it does not link the C implementation or
    redefine app payload meaning.
  - `ray_tracing`: `core_screen_pick >= 0.1.0` for deterministic nearest
    whole-object selection over app-projected primitive and mesh-instance
    origins. Projection, visibility, click/hover routing, exact mesh/face
    picking, and rendering remain app-owned.
  - `ray_tracing`: `kit_ui >= 0.11.2` for object-list wheel evaluation,
    top-anchor content sizing, and offset clamping. Row semantics, clipping,
    drawing, selection, and pane input routing remain app-owned.
  - `line_drawing`: `core_screen_pick >= 0.1.0` for deterministic nearest
    whole-object selection over app-projected visual centers. Hitbox rebuild
    lifecycle, gizmo/handle/topology precedence, authoring arbitration, and
    rendering remain app-owned.
  - `physics_sim`: `core_screen_pick >= 0.1.0` for projected object-body
    nearest/ranked queries. Projection, hit-stack policy, imports, emitters,
    boundaries, drag policy, and repeat-click cycling remain app-owned.
  - `ray_tracing`: `kit_viewport3d >= 0.1.0` for the source-level
    Solid/Material CPU surface outline adapter: stable object accents,
    selected/hover priority, silhouette, relative depth-discontinuity, and
    object-owner boundaries. RayTracing retains projection, rasterization,
    cache and Vulkan texture ownership, picking, and overlay visibility policy.
  - `line_drawing`: `kit_viewport3d >= 0.1.0` for the source-level
    filled-surface and outline-only composition step over its existing CPU
    raster buffers. LineDrawing retains projection, rasterization, adaptive
    quality/cache policy, picking, authoring, and renderer ownership.
  - `ball_bounce_sim`: opt-in collision adapter proof depends on
    `core_collision2d >= 0.2.0` for shape/AABB conversion, primitive
    circle/circle, box/box, polygon/polygon parity, and compound descriptor
    area/mass, center-of-mass, inertia, local-AABB parity through
    `core-collision2d-adapter-parity-contract`. The clang/package compound
    descriptor AABB/mass route now default-adopts
    `core_collision2d >= 0.2.0` through
    `BALL_COLLISION2D_USE_CORE_COMPOUND_DESCRIPTOR_OPT_IN`, with
    `core-collision2d-compound-usage-contract` and
    `core-collision2d-default-cutover-contract` as proof gates. The
    clang/package circle-vs-circle, box/box, and convex polygon/polygon default
    contact paths now adopt `core_collision2d >= 0.2.0` through the app-local
    collision adapter and `BALL_COLLISION2D_USE_CORE_CIRCLE_DEFAULT` /
    `BALL_COLLISION2D_USE_CORE_BOX_DEFAULT` /
    `BALL_COLLISION2D_USE_CORE_POLYGON_DEFAULT`, while fisiCs and standalone
    local contact contracts retain the Ball Bounce implementation as the oracle. The
    clang/package rigid body and solver symbol path default-adopts
    `core_rigid2d >= 0.1.1` through shared-backed alternate files, with
    `core_collision2d >= 0.1.1` as the shape/manifold dependency and local
    rigid files retained as the fisiCs oracle. P14-S5 keeps Ball Bounce on
    direct live `SHARED_ROOT ?= ../shared` source references for `core_sim`,
    `core_collision2d`, and `core_rigid2d`; managed subtree packaging remains
    deferred until a release-grade standalone lane exists.
  - `ide`: `kit_graph_struct >= 0.8.1` for Libraries-panel include
    dependency graph layout/hit helpers over the IDE-local `include_graph`
    snapshot, plus `core_viewport2d >= 0.2.1` for cursor-anchor zoom and
    drag-pan camera math. `kit_ui >= 0.11.1` is now linked from the managed
    vendored subtree as the foundation for the IDE button behavior unification
    lane. SDL drawing, source/header meaning, graph HUD, button call-site
    migration, and future collapse policy remain IDE-owned.
  - `video_editor`: `core_pane >= 0.3.1` for split-pane solve and cached
    splitter-hit enumeration in the SDL shell while drag lifecycle and
    `ui_settings.cfg` persistence remain app-local.
  - `ray_tracing`: `core_mesh_asset >= 0.3.1` for MRT2 runtime mesh asset
    file loading/validation plus MRT8 larger runtime mesh payload parsing
    without linked-list indexed JSON array traversal; scene-relative JSON
    traversal and native `3D` triangle build remain app-local.
    `core_mesh_preview >= 0.5.0` is now used for runtime mesh preview sidecar
    path/probe/metadata attachment on loaded assets and preview-limited skipped
    instances plus an editor-only store of bounded coherent indexed LODs. The
    app-local Bounds/Wire/Solid/Material vocabulary and quality invalidation
    contract preserve geometry tiers across zoom, pan, hover, and selection;
    native rendering, materials, camera, overlays, final geometry, and BVHs
    remain RayTracing-owned. `core_scene_view >= 0.1.0` now owns the renderer-free
    scene-view schema/readback vocabulary for `ray_tracing_scene_view_packet_v0`,
    including preview quality, degraded reason, display flags, pick ids, and
    compact JSON readback; RayTracing still owns packet production,
    serialization, material meaning, live editor routing, and scene mutation.
  - `physics_sim`: `core_mesh_preview >= 0.4.0` for runtime-scene
    `mesh_asset_instance` preview sidecar path/probe/metadata attachment
    through an app-local import bridge. This is advisory retained-scene/editor
    plumbing only; collision proxies, obstacle truth, SDFs, solver projection,
    and physics semantics remain PhysicsSim-owned.
    `core_scene_view >= 0.2.0` is now used for the app-local read-only
    `PhysicsSimSceneViewPacketReadout`, which consumes
    `ray_tracing_scene_view_packet_v0` compact packet readback and derives its
    material/transparent/display/count summary through
    `CoreSceneViewPacketSummary` without routing into retained scene apply,
    solver projection, scene-project cache output, or
    `extensions.physics_sim` overlay writeback.
  - `line_drawing`: `core_mesh_asset >= 0.5.2` and
    `core_mesh_compile >= 0.6.5` for file-pane STL import and the private
    x3 high-triangle sidecar proof ceiling, plus
    `core_mesh_preview >= 0.5.0` for bounded viewport-safe runtime preview
    sidecars, S1 source/preview count metadata, and S2 feature-edge,
    sampled-triangle, point-cloud, and bounds-proxy preview modes plus S3
    runtime-file build/save, metadata-only load, and preview-probe APIs.
    `core_mesh_asset` provides streaming runtime mesh save for large
    imported meshes, `core_mesh_compile` provides dirty ASCII/binary STL
    tolerance with zero-area/degenerate triangles skipped before runtime mesh
    validation, and `core_mesh_preview` provides sampled local-space feature
    edges plus explicit preview counts, budget/coverage, bounds/span/sphere,
    source-count metadata for UI display, and mode-selecting preview payloads.
    `core_scene_view >= 0.1.0` is now used for read-only scene-view packet
    readback in `LayoutSceneViewPacketConsumer`; face-group mapping,
    canonical object/scene mutation, viewport drawing, and editor picking
    remain LineDrawing-owned.
    for follow-on host adoption. Mesh repair, retopo,
    GPU buffers, collision proxies, SDFs, and richer editor decimation remain
    host/shared roadmap work.
- Update this matrix whenever an app starts using a new shared module.
- `connected_mechanics_sim`: CMS-F9 directly links `core_base >= 1.0.1`,
  `core_sim >= 0.4.2`, `core_trace >= 1.0.2`, and
  `core_sim_trace >= 0.1.1`, plus `core_action >= 0.1.1`,
  `core_time >= 1.0.1`, and `core_wake >= 1.0.2` from the managed snapshot at
  shared commit `8ac8abf`. It also links `core_theme >= 2.0.1`,
  `core_font >= 1.0.2`, `core_pane >= 0.3.1`, `kit_render >= 0.14.4`,
  `kit_ui >= 0.11.2`, `kit_pane >= 0.3.1`, and
  `kit_workspace_authoring >= 0.5.1` for CMS-F5. `core_pack >= 1.1.1` remains
  transitive trace closure; the checked direct manifest pins later foundation
  versions without claiming they are linked.
  CMS-F6 now links `vk_runtime >= 0.6.0` beneath `vk_renderer >= 1.3.1` for
  committed managed presentation lifecycle, validation/readback,
  resize/recovery, capture, and material-frame proof; CMS-F8 links
  `core_viewport3d >= 0.1.0`, `core_screen_pick >= 0.1.0`, and
  `kit_viewport3d >= 0.1.0`; CMS-F9 links `core_config >= 0.1.1` and
  `core_io >= 1.1.1`.
- Update minimum versions whenever an app relies on newly added module behavior.
- Keep `N/A` for modules not yet linked by that app.
- For shared patch bumps in active deps (for example `core_wake` `1.0.1`), update dependent app minimums only when they require that patch behavior.
