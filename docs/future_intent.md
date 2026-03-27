# MapForge Future Intent

Last updated: 2026-03-27

## Scaffold Alignment Intent
1. Preserve current subsystem decomposition strengths (`app`, `map`, `route`, `render`, etc.).
2. Keep locked scaffold lifecycle wrapper entrypoint stable while preserving behavior.
3. Keep shared subtree usage explicit and stable.
4. Keep verification gates deterministic and non-interactive for migration slices.

## Planned Next Structural Intent
- `MF-S3` (completed):
  - canonical wrapper entry API is now present:
    - `include/map_forge/map_forge_app_main.h`
    - `src/app/map_forge_app_main.c`
  - locked stage symbols are now present.
  - legacy runtime loop remains behavior-preserving through delegation to `app_run_legacy()`.

- `MF-S4` (completed in slices):
  - dependency-lane decision locked: keep `third_party/` exception for vendored-subtree mode.
  - temp/runtime lane hygiene landed: `tmp/` and `data/runtime/` ignored.
  - include strategy locked: keep existing domain include lanes; route new app-level public entry APIs through `include/map_forge/...`.

- `MF-S5` (completed):
  - stabilization closeout completed in scaffold matrix/backlog/private plan.
  - scaffold completion commit title policy executed.

## Non-Goals During Scaffold Migration
- No feature expansion unrelated to scaffold alignment.
- No shared subtree redesign in migration commits.
- No broad naming churn in one pass; only bounded slices with verification gates.
