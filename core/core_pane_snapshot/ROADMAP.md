# core_pane_snapshot Roadmap

## Mission

Provide deterministic snapshot validation primitives shared by pane-host runtimes.

## Current State

1. Shared v1 snapshot schema and validation boundary is in place.
2. Docs/tests hardening slices are complete in `0.1.1`.
3. WorkspaceSandbox remains the proving host for bridge import/export and debug JSON flows.

## Immediate Steps

1. Add structured validation report output (error index/details) only if multiple hosts need richer import diagnostics.
2. Add optional compatibility-mode gates for future minor schema revisions only when a second schema minor is real.
3. Add helper transforms for import normalization only if host adapters duplicate the same repair logic.

## Future Steps

1. Add chunk-level decode/encode adapters layered above this core.
2. Add snapshot diff helpers for diagnostics and review tools.
3. Add optional JSON debug export helpers if shared ownership becomes beneficial.
