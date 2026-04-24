# Trio Contract Fixtures

Baseline scene fixtures for trio interop preflight and bridge validation.

- `scene_authoring_min.json`: minimal `scene_authoring_v1` source fixture.
- `scene_runtime_min.json`: minimal `scene_runtime_v1` handoff fixture (aligned to `core_scene_compile` `v0.2.0` metadata, including `compile_meta.normalization` and `hierarchy` lane).
- `scene_authoring_interop_min.json`: deterministic trio interop fixture with shared + app namespace extensions.
- `ray_overlay_min.json`: minimal permitted ray writeback overlay (`extensions.ray_tracing.*`).
- `physics_overlay_min.json`: minimal permitted physics writeback overlay (`extensions.physics_sim.*`).
- `scene_runtime_min_reordered.json`: semantically equivalent runtime fixture with reordered keys + merge metadata lane for diff-tooling smoke checks.
- `run_scene_contract_diff_smoke.sh`: semantic diff smoke gate (`scene_contract_diff`) used by app integration test targets.

These are intentionally compact, deterministic fixtures for test harnesses in `ray_tracing` and future `physics_sim` bridge lanes.
