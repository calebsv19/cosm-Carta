# core_layout Roadmap

## Mission
Own generic layout transaction semantics that multiple workspace hosts can reuse.

## Immediate Steps
1. Keep transaction state machine deterministic and test-backed.
2. Keep no-op authoring apply and metadata preload behavior explicit instead of inferring it from tests alone.
3. Add optional structured metadata report helpers for debugging revision provenance only if hosts need more than the current stored fields.

## Future Steps
1. Add bounded diff metadata for change summaries.
2. Add optional validation callback hooks (without pane policy leakage).
3. Add rollback reason codes for telemetry/diagnostics.
