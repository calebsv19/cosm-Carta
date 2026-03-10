# core_layout Roadmap

## Mission
Own generic layout transaction semantics that multiple workspace hosts can reuse.

## Immediate Steps
1. Keep transaction state machine deterministic and test-backed.
2. Add optional revision metadata hooks for preset persistence.
3. Add no-op-safe transition helpers for integration robustness.

## Future Steps
1. Add bounded diff metadata for change summaries.
2. Add optional validation callback hooks (without pane policy leakage).
3. Add rollback reason codes for telemetry/diagnostics.
