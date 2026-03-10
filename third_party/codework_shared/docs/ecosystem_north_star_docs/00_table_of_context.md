# Ecosystem North Star - Table Of Contents

This is the quick entrypoint for docs in `shared/docs/ecosystem_north_star_docs/`.
It separates stable reference docs and archived completed phase docs.

## Summary / Reference Docs (General Use)

| File | Type | Summary |
| --- | --- | --- |
| `01_core_and_kit_catalog.md` | Behavior summary | Canonical catalog of current and planned `core_*` and `kit_*` module responsibilities. |
| `02_core_kit_workflow.md` | Workflow summary | Rollout sequencing rules for safe, additive core/kit adoption. |
| `03_north_star_execution_plan.md` | Master plan summary | Top-level phase roadmap (Phase 0 through Phase 8). |
| `04_shared_inventory.md` | State summary | Inventory of legacy shared libs and migration candidates. |
| `05_data_exchange_inventory.md` | State summary | Current import/export pathways and format usage across apps. |
| `11_version_compat_matrix.md` | Compatibility summary | Minimum supported shared-module versions per app. |
| `29_program_shared_lib_connection_gaps.md` | Gap tracker | Canonical per-program list of missing/partial shared-lib integrations and next steps. |
| `44_program_library_connective_tissue_state.md` | State summary | Current program-by-program shared-library integration snapshot (implemented/partial/missing + next migrations). |
| `14_daw_pack_usage_policy.md` | Policy summary | DAW `.pack` usage policy and warm-start behavior guidance. |
| `25_ui_theme_and_font_standardization_plan.md` | Phase plan | Execution plan for shared theme presets and font catalog standardization across apps. |
| `28_execution_core_adoption_candidates_plan.md` | Phase plan | Safe phased adoption plan for applying execution cores to existing timing/worker/IPC/runtime paths. |
| `34_ray_tracing_theme_surface_completion_plan.md` | Phase plan | RayTracing follow-on plan for default-on shared-theme usage, persistence, and menu-first completion with editor windows kept as a later cleanup slice. |
| `36_kit_graph_struct_growth_plan.md` | Phase plan | Multi-phase shared roadmap for evolving `kit_graph_struct` from layered node rendering to a reusable layout+routing pipeline with additive adoption in `mem_console`. |
| `37_kit_graph_struct_phase_g0_execution_plan.md` | Phase plan | Phase G0 execution checklist for deterministic layered ordering and mem-console graph layout caching, with explicit completion gates. |
| `38_kit_graph_struct_phase_g1_execution_plan.md` | Phase plan | Phase G1 execution checklist for layered DAG crossing reduction, validation tests, and version/doc closeout. |
| `39_kit_graph_struct_phase_g2_execution_plan.md` | Phase plan | Phase G2 execution checklist for additive edge routing APIs (straight/orthogonal), route tests, and mem_console routed-edge adoption. |
| `40_kit_graph_struct_phase_g3_execution_plan.md` | Phase plan | Phase G3 execution checklist for route-aware edge labels, density controls, and mem_console label adoption. |
| `41_kit_graph_struct_phase_g4_execution_plan.md` | Phase plan | Phase G4 execution checklist for additive edge/label hit testing, viewport helper utilities, and mem_console edge/label hit adoption. |
| `42_kit_graph_struct_phase_g5_execution_plan.md` | Phase plan | Phase G5 execution checklist for mem_console viewport interaction adoption (wheel zoom, drag pan, drag-release click suppression). |
| `43_kit_graph_struct_phase_g6_execution_plan.md` | Phase plan | Phase G6 execution checklist for extensibility hardening: expanded validation fixtures, perf instrumentation, and layout-strategy hooks. |

## Completed Plan Docs (Archived)

These are kept for reference under `shared/docs/completed/`.

| File | Original Phase | Summary | Status |
| --- | --- | --- | --- |
| `../completed/ecosystem_north_star_docs/06_phase1_core_spine_bootstrap.md` | Phase 1 | Initial `core_base`/`core_io`/`core_data`/`core_pack` bootstrap implementation details. | Completed |
| `../completed/ecosystem_north_star_docs/08_phase2_kit_viz_datalab_execution_plan.md` | Phase 2/3 | Minimal `kit_viz` + DataLab setup and first profile validation flow. | Completed |
| `../completed/ecosystem_north_star_docs/09_phase4_daw_pack_adoption_plan.md` | Phase 4 | DAW `.pack` export adoption and cross-domain validation details. | Completed |
| `../completed/ecosystem_north_star_docs/10_phase5_stabilize_plan.md` | Phase 5 | Versioning and `.pack v1` freeze/stabilization execution details. | Accepted |
| `../completed/ecosystem_north_star_docs/12_phase5_release_checklist.md` | Phase 5 | Operational release checklist used for stabilization acceptance. | Completed |
| `../completed/ecosystem_north_star_docs/13_phase6_embedded_viz_plan.md` | Phase 6A/6B | DAW embedded `kit_viz` adoption and hardening execution notes. | Accepted |
| `../completed/ecosystem_north_star_docs/15_phase6c_physics_embedded_viz_plan.md` | Phase 6C | PhysicsSim embedded `kit_viz` rollout and fallback behavior checks. | Accepted |
| `../completed/ecosystem_north_star_docs/16_phase6d6e_meter_standardization_and_safety.md` | Phase 6D/6E | DAW metering adapter standardization and safety/perf gating details. | Completed |
| `../completed/ecosystem_north_star_docs/17_phase6f_raytracing_pack_kitviz_adoption_plan.md` | Phase 6F | RayTracing `.pack` + `kit_viz` adoption with fallback safety implementation. | Accepted |
| `../completed/ecosystem_north_star_docs/18_phase6g_core_space_standardization_plan.md` | Phase 6G | Shared spatial contract (`core_space`) adoption across RayTracing/MapForge. | Accepted |
| `../completed/ecosystem_north_star_docs/19_phase7_core_trace_plan.md` | Phase 7 | `core_trace` foundations, producer adoption, DataLab trace viewer, and trace v1 freeze. | Accepted |
| `../completed/ecosystem_north_star_docs/24_system_include_shims_plan.md` | Phase 8 | `sys_shims` compatibility/conformance plan through S7 completion. | Completed |
| `../completed/ecosystem_north_star_docs/26_execution_core_layer_implementation_plan.md` | Phase 8 | Execution-core implementation order and module gate plan. | Completed |
| `../completed/ecosystem_north_star_docs/27_execution_core_milestone_checklist.md` | Phase 8 | Execution-core 1.0 milestone and release checklist. | Completed |
| `../completed/ecosystem_north_star_docs/30_ide_theme_surface_completion_plan.md` | Phase 8 | IDE-specific shared-theme surface completion and runtime persistence follow-on. | Completed |
| `../completed/ecosystem_north_star_docs/31_daw_theme_surface_completion_plan.md` | Phase 8 | DAW shared-theme surface completion across transport, timeline, inspector, and effects UI. | Completed |
| `../completed/ecosystem_north_star_docs/32_map_forge_theme_surface_completion_plan.md` | Phase 8 | MapForge shared-theme default-on, persistence, and overlay/chrome completion. | Completed |
| `../completed/ecosystem_north_star_docs/33_line_drawing_theme_surface_completion_plan.md` | Phase 8 | Combined line_drawing and line_drawing3d shared-theme completion and persistence follow-on. | Completed |
| `../completed/ecosystem_north_star_docs/35_physics_sim_theme_menu_stability_plan.md` | Phase 8 | PhysicsSim shared-theme UI stability pass for menu and main scene editor shell. | Completed |
| `../completed/rollout_history/07_project_core_adoption_matrix.md` | Legacy matrix | Earlier per-project core adoption matrix superseded by active gap/compat docs. | Archived |

## Planned Docs (Unresolved Placeholders)

The following were referenced in older updates but are not currently committed in this folder (no source files found in this repo):
- `20_phase8_expansion_plan.md`
- `21_phase8_gate_commands.md`
- `22_core_kit_program_dependency_graph.md`
- `23_phase8f_physics_sim_core_completion_plan.md`

## Suggested Read Order

1. `03_north_star_execution_plan.md`
2. `01_core_and_kit_catalog.md`
3. `02_core_kit_workflow.md`
4. `29_program_shared_lib_connection_gaps.md`
5. `11_version_compat_matrix.md`
6. `14_daw_pack_usage_policy.md`
7. `../completed/ecosystem_north_star_docs/24_system_include_shims_plan.md`
8. `../completed/ecosystem_north_star_docs/19_phase7_core_trace_plan.md`
9. `../completed/ecosystem_north_star_docs/10_phase5_stabilize_plan.md`

## Top-Level Shared Operational Docs

These live in `shared/docs/` (outside this folder) and track active rollout state:
- `../SHARED_LIBS_CURRENT_STATE.md`
- `../SHARED_THEME_FONT_ROLLOUT_CONTRACT.md`
- `../THEME_FONT_ADAPTER_COVERAGE_STATUS.md`
- `../SHARED_LIB_OWNERSHIP_BOUNDARIES.md`
- `../SHIMS_VS_RUNTIME_LIBS_BOUNDARY.md`
- `../memory_db_autonomous_maintenance/README.md` (strategic future-work lane; planning docs not yet implementation-complete)
