# Shared Module Versioning Policy

This document defines semantic versioning and compatibility rules for shared ecosystem modules.

## Scope
Modules covered:
- `shared/core/core_base`
- `shared/core/core_io`
- `shared/core/core_data`
- `shared/core/core_pack`
- `shared/core/core_layout`
- `shared/core/core_config`
- `shared/core/core_action`
- `shared/core/core_scene`
- `shared/core/core_scene_compile`
- `shared/core/core_space`
- `shared/core/core_units`
- `shared/core/core_object`
- `shared/core/core_trace`
- `shared/core/core_math`
- `shared/core/core_pane`
- `shared/core/core_time`
- `shared/core/core_queue`
- `shared/core/core_sched`
- `shared/core/core_jobs`
- `shared/core/core_workers`
- `shared/core/core_wake`
- `shared/core/core_kernel`
- `shared/kit/kit_render`
- `shared/kit/kit_pane`
- `shared/kit/kit_graph_timeseries`
- `shared/kit/kit_graph_struct`
- `shared/kit/kit_ui`
- `shared/kit/kit_viz`
- `shared/vk_renderer`

Each module has a module-local `VERSION` file as source of truth.

## SemVer Rules
Versions follow `MAJOR.MINOR.PATCH`.

- `MAJOR`: backward-incompatible API or behavior change.
- `MINOR`: additive, backward-compatible API/behavior.
- `PATCH`: bug fix or internal correction with no compatibility break.

## Bump Decision Guide
Use this quick guide for every shared module change:

- `PATCH` (`x.y.Z`): small safe fixes.
  - Examples: bug fixes, warning cleanup, perf/internal refactors, docs/tests updates that do not change public API behavior.
  - Must not break callers, data contracts, or file/wire format compatibility.
- `MINOR` (`x.Y.z`): backward-compatible feature growth.
  - Examples: new optional API function, new non-breaking flags/options, additive metadata fields that old callers can ignore.
  - Existing callers must keep working without code changes.
- `MAJOR` (`X.y.z`): breaking changes.
  - Examples: removed/renamed API, signature changes, changed default behavior that breaks existing expectations, incompatible data/format change.
  - Requires migration notes before release.

Rule of thumb:
- If old code compiles/runs the same -> `PATCH` or `MINOR`.
- If old code must change -> `MAJOR`.

## Core vs Kit Stability
- `core_*` modules are high-stability contracts.
- `kit_*` modules can evolve faster, but must still follow SemVer for public APIs.

Expected strictness:
- `core_*`: do not break existing downstream callers without a planned migration.
- `kit_*`: allow faster additive growth; breaking changes still require `MAJOR` bump.

## Required Checks Before Bumping

### Before `PATCH`
- Unit tests pass for the touched module.
- No public API signature changes.
- No on-disk format contract changes.
- Update module `VERSION`.
- Add a short note to module README/roadmap describing what changed.

### Before `MINOR`
- Unit tests pass for touched modules.
- Existing API behavior remains valid for current callers.
- New API additions are documented in module README/roadmap as needed.
- For `core_pack`, fixture and parser tests are updated if new optional chunks are introduced.
- Update module `VERSION`.
- Document the additive API/behavior in module README/roadmap.

### Before `MAJOR`
- Breaking change note written (`what`, `why`, `migration`).
- Compatibility impact reviewed across adopting apps.
- Migration path documented before release.
- Version compatibility matrix updated.
- Update module `VERSION`.

## `.pack` Contract Relationship
`core_pack` versioning and `.pack` wire format are related but not identical:
- `core_pack` library SemVer controls C API and behavior compatibility.
- `.pack` format compatibility is governed by `shared/core/core_pack/PACK_V1_SPEC.md`.

Any `.pack` wire-level change must follow the freeze/change-control flow in the spec.

## Adoption Matrix
App-level minimum supported module versions are tracked in:
- `shared/docs/ecosystem_north_star_docs/11_version_compat_matrix.md`

This matrix is updated whenever module minimums for an app change.
