# MapForge Known Constraints And Backlog

This file tracks practical current-state constraints and follow-up items after structural hardening.

## Current Constraints

1. Heavy runtime state still lives in one `AppState` composition root.
2. Route and tile worker pipelines are stable but still app-local abstractions.
3. GUI behavior (panel interactions, stress zoom behavior) still depends on manual runtime validation for full confidence.
4. Vulkan and SDL paths share runtime orchestration and need parity checks after deeper rendering changes.
5. Contour is intentionally parked until terrain/DEM-backed contour pipeline is added (`runtime_hardening.contour_enabled` keeps re-enable path).

## Near-Term Backlog

1. Add more non-GUI tests for route panel model/controller invariants.
2. Add queue-pressure regression tests for stage/ready queue interactions.
3. Add a repeatable stress checklist document (zoom/layer toggle/region switch loops).
4. Consolidate duplicate theme/palette constants between UI surfaces.
5. Expand architecture docs with sequence diagrams once stable.

## Shared-Core Promotion Candidates (Deferred)

Potential future shared extraction candidates, only after boundaries stabilize:

1. worker generation + stale-result utility contract
2. generic queue pressure drop/evict policy helper
3. HUD model snapshot pattern for update/render separation

Do not promote until at least one additional app needs the same behavior.
