# Theme/Font Adapter Coverage Status

Status snapshot for the shared theme/font adapter rollout.

Last updated: 2026-03-02

## Apps in Scope

- DAW
- IDE
- line_drawing
- line_drawing3d
- ray_tracing
- physics_sim
- map_forge

## Coverage Table

| App | Adapter Module | Test Target | Theme Preset Matrix | Runtime Cycle | Persistence | Default Shared Mode |
|---|---|---|---|---|---|---|
| DAW | yes | `make -C daw test-shared-theme-font-adapter` | yes | yes | yes | yes |
| IDE | yes | `make -C ide test-shared-theme-font-adapter` | yes | yes | yes | yes |
| line_drawing | yes | `make -C line_drawing test-shared-theme-font-adapter` | yes | yes | yes | yes |
| line_drawing3d | yes | `make -C line_drawing3d test-shared-theme-font-adapter` | yes | yes | yes | yes |
| ray_tracing | yes | `make -C ray_tracing test-shared-theme-font-adapter` | yes | yes | yes | yes (menu path) |
| physics_sim | yes | `make -C physics_sim test-shared-theme-font-adapter` | yes | yes | yes | yes (menu + scene editor path) |
| map_forge | yes | `make -C map_forge test-shared-theme-font-adapter` | yes | yes | yes | yes |

## Quick Smoke Commands

- `make -C daw test-shared-theme-font-adapter`
- `make -C ide test-shared-theme-font-adapter`
- `make -C line_drawing test-shared-theme-font-adapter`
- `make -C line_drawing3d test-shared-theme-font-adapter`
- `make -C ray_tracing test-shared-theme-font-adapter`
- `make -C physics_sim test-shared-theme-font-adapter`
- `make -C map_forge test-shared-theme-font-adapter`

## Runtime Policy

- Fallback mode remains supported, but the current UI app set now defaults to shared mode in normal runtime use.
- The remaining active UI backlog is no longer adapter wiring; it is deferred surface cleanup where explicitly documented (currently the RayTracing editor-window follow-on).
