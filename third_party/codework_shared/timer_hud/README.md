# timer_hud

`timer_hud` provides shared frame-timing and HUD instrumentation utilities used by multiple CodeWork apps.

Current version: `0.7.1`

## Scope
- timer lifecycle and frame tracking
- event/logging hooks for runtime timing surfaces
- HUD rendering integration hooks

## Public Headers
- `include/timer_hud/time_scope.h`
- `include/timer_hud/timer_hud_config.h`
- `include/timer_hud/timer_hud_backend.h`

Legacy compatibility:
- `include/timer_hud/settings_loader.h` remains available for the JSON load/save contract and legacy settings access, but new host integrations should prefer the explicit config accessors exposed through `timer_hud_config.h`.

## Notes
- This module is in active cleanup for public release readiness.
- Legacy and backup snapshots were moved out of public scope during cleanup.
- `0.2.0` starts the safety/API hardening lane:
  - explicit config getters/setters are now public
  - host code no longer needs `time_scope.h` to expose the private settings loader surface
  - timer names are now runtime-owned instead of borrowed pointers
- `0.3.0` adds a typed HUD visual-mode API on top of the legacy string setter:
  - `TimerHUDVisualMode` gives hosts a safer mode-switch boundary
  - string parsing/normalization remains available for env/config ingestion
  - renderer mode checks now route through the typed accessor instead of raw string compares
- `0.4.0` adds a shared bootstrap/init helper for adopters:
  - `TimerHUDInitConfig` centralizes program/output/settings-path setup
  - settings seeding can now be handled by the shared layer instead of host-local copy logic
  - `physics_sim`, `ray_tracing`, `ide`, and `daw` can converge on one bootstrap pattern
- `0.5.0` adds the first session/context ownership slice:
  - `TimerHUDSession` is now a real public runtime handle
  - the legacy `ts_*` surface forwards through a default session for compatibility
  - timer manager, logger, event tracker, backend binding, and runtime path state are now session-owned
- `0.5.1` splits runtime data shaping from presentation:
  - HUD rendering now consumes a read-only snapshot instead of walking timer/runtime state directly
  - mode parsing, timer text formatting, anchor capture, and graph scaling now live in a presenter-facing snapshot builder
  - the compatibility surface still used legacy global-settings access, but the renderer no longer depended on that mutable global directly
- `0.6.0` moves settings ownership behind the session contract and proves the first explicit-session host:
  - `TimerHUDSession` now owns HUD/log/event settings as runtime state instead of relying on a mutable global settings object
  - `settings_loader.h` remains source-compatible through a default-session compatibility macro, but new integrations should treat it as legacy-only
  - `ide` now uses an explicit `TimerHUDSession*` for init, frame hooks, timer scopes, render, and shutdown
- `0.6.1` refines HUD presentation for long-running timers:
  - HUD labels now format large durations as seconds, minutes, or hours instead of always emitting raw milliseconds
  - graph scale labels are captured in the render snapshot and drawn in the row header instead of over the graph trace
  - hybrid/history rows stay graph-first while preserving the existing host backend and settings contracts
- `0.7.0` adds analysis-backed visual modes:
  - row snapshots now carry last/avg/min/max/p95-ish/stddev/sample/spike/percent-of-total analysis fields
  - `stats`, `spikes`, and `compare` visual modes are accepted through the existing HUD visual-mode config path
  - the renderer draws mode headers, spike-focused history rows, dense stats rows, and relative average-time compare bars without changing host backend hooks
  - hosts can optionally pass pointer-down events to TimerHUD; only the three tiny header controls (`<`, `H`, `>`) are consumed, while all other points fall through to the host UI
- `0.7.1` adds `ts_record_duration_ms` / `ts_session_record_duration_ms` for hosts that already measured a duration externally, such as worker-thread tile schedulers that need to publish timing samples from a safe aggregation path

## Public Contract Status
- top-level module docs now present
- session-owned settings and the renderer snapshot seam are now both live behind the compatibility shim
- build/test contract normalization is still pending
