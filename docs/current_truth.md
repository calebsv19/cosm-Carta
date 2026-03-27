# MapForge Current Truth

Last updated: 2026-03-27

## Program Identity
- Repository directory: `map_forge/`
- Public product name in README: `Carta`
- Primary runtime entry path today:
  - `src/main.c` -> `map_forge_app_main()` in `src/app/map_forge_app_main.c`
  - wrapper run-loop stage delegates to `app_run_legacy()` in `src/app/app.c`

## Current Structure
- Required scaffold lanes are present:
  - `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active source subsystem lanes:
  - `app`, `camera`, `core`, `map`, `render`, `route`, `ui`
- Header strategy:
  - include-dominant (`include/*`), with mirrored subsystem directories.
  - project-level entry/lifecycle headers use `include/map_forge/*`.

## Dependency Lane Policy
- `map_forge` keeps `third_party/` as an explicit scaffold exception because this repo runs in vendored-subtree mode (`third_party/codework_shared`).
- No `third_party` -> `external` rename is planned for this migration track.

## Include Namespace Strategy
- Existing domain include lanes remain in place (`include/app`, `include/map`, `include/route`, etc.) to avoid churn-heavy renames.
- New cross-subsystem/public app entry APIs use project namespace:
  - `include/map_forge/...`
- Private/local-only headers remain allowed in `src/<subsystem>/` when locality is clearer.

## Runtime/Verification Contract (Current)
- Build:
  - `make -C map_forge clean && make -C map_forge`
- Full non-interactive tests:
  - `make -C map_forge test`
- Headless smoke gate (non-interactive):
  - `make -C map_forge run-headless-smoke`
  - current implementation validates app build + focused runtime-adjacent smoke tests without launching interactive UI.
- Visual harness build gate:
  - `make -C map_forge visual-harness`
  - current implementation is an app-build availability alias.

## Runtime State Persistence
- Committed default config:
  - `config/app.config.json`
- Runtime-persisted mutable state:
  - `data/runtime/app_state.json`
- Load order:
  - runtime file first, then default config fallback.
- Save path:
  - runtime file only (`data/runtime/app_state.json`), so normal app runs do not dirty tracked config defaults.

## Active Scaffold Migration State
- Private migration plan:
  - `../docs/private_program_docs/map_forge/2026-03-27_map_forge_scaffold_standardization_switchover_plan.md`
- Baseline freeze:
  - `../docs/private_program_docs/map_forge/2026-03-27_mf_s0_baseline_freeze_and_mapping.md`
- Completed phases:
  - `MF-S0`, `MF-S1`, `MF-S2`, `MF-S3`, `MF-S4`, `MF-S5`
- Next phase:
  - scaffold migration closed; follow-on changes move to normal program planning tracks.
